#include "DialoguePluginEditorDetails.h"
#include "DialoguePluginEditorPrivatePCH.h"
#include "Dialogue.h"
#include "DialogueViewportWidget.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Editor/PropertyEditor/Public/DetailLayoutBuilder.h"
#include "Editor/PropertyEditor/Public/DetailCategoryBuilder.h"
#include "Editor/UnrealEd/Public/ScopedTransaction.h"
#include "Internationalization/Text.h"
#include "SSimpleComboButton.h"
#include "Styling/StyleColors.h"
#include "STextPropertyEditableTextBox.h"
#include "Internationalization/TextNamespaceUtil.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"
//#include "TextCustomization.cpp"
//#include "STextPropertyEditableTextBox.h"
#include "DialogueEditorSubsystem.h"
#include "DialoguePluginEditorSettings.h"
#include "EditableTextPropertyHandle.h"
#include "PropertyRestriction.h"
#include "Animation/CurveSequence.h"
#include "Containers/Ticker.h"

#define LOCTEXT_NAMESPACE "DialoguePluginSettingsDetails"

TSharedRef<IDetailCustomization> FDialoguePluginEditorDetails::MakeInstance()
{
	return MakeShareable(new FDialoguePluginEditorDetails());
}

/* This code draws the panel to the right of the graph */
void FDialoguePluginEditorDetails::CustomizeDetails( IDetailLayoutBuilder& DetailLayout )
{
	using namespace DialoguePluginEd;

	DetailLayoutBuilder = &DetailLayout;

	// Initiate throbber spinning animation
	if (!ThrobberAnimation.IsPlaying()) {
		UDialogueEditorSubsystem* DialogueEditorSubsystem = GEditor->GetEditorSubsystem<UDialogueEditorSubsystem>();
		ThrobberAnimation = FCurveSequence(0.0f, 1.0f);
		auto Delegate = FTickerDelegate::CreateUObject(DialogueEditorSubsystem, &UDialogueEditorSubsystem::SpecialTick);
		ThrobberAnimation.Play(Delegate, true);
	}

	const TSharedPtr<IPropertyHandle> DataProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UDialogue, Data));
	DetailLayout.HideProperty(DataProperty);
	const TSharedPtr<IPropertyHandle> NextNodeProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UDialogue, NextNodeId));
	DetailLayout.HideProperty(NextNodeProperty);

	const TSharedPtr<IPropertyHandle> DispositionProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UDialogue, NpcDisposition));
	DetailLayout.HideProperty(DispositionProperty);

	// Create a category so this is displayed early in the properties
	IDetailCategoryBuilder& DefaultCategory = DetailLayout.EditCategory("Dialogue");
	IDetailCategoryBuilder& CurrentNodeCategory = DetailLayout.EditCategory("Current Node", LOCTEXT("CurrentNode", "Current Node"), ECategoryPriority::Important);

	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailLayout.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	TArray<UObject*> StrongObjects;
	CopyFromWeakArray(StrongObjects, ObjectsBeingCustomized);
	if (StrongObjects.Num() == 0) return;

	UDialogue* Dialogue = Cast<UDialogue>(StrongObjects[0]);

	// used in making text boxes
	const TSharedRef<FLinkedBoxManager> LinkedBoxManager = MakeShared<FLinkedBoxManager>();
	const FSlateFontInfo PropertyNormalFont = FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont");

	/* If current node is selected and it's not the first (orange) node, and only one node is selected, display fields of the selected node */
	if (Dialogue->CurrentNodeId != -1 && Dialogue->CurrentNodeId != 0)
	{
		int32 index;
		FDialogueNode CurrentNode = Dialogue->GetNodeById(Dialogue->CurrentNodeId, index);

		const TSharedPtr<IPropertyHandleArray> Array = DataProperty->AsArray();
		const TSharedPtr<IPropertyHandle> Child = Array->GetElement(index);
		const TSharedPtr<IPropertyHandle> AltText = Child->GetChildHandle("Text");
		AltText->GetMetaDataProperty()->SetMetaData("MultiLine", "true");
		
		TSharedRef<FEditableTextPropertyHandle> EditableTextSharedRef = MakeShareable(new FEditableTextPropertyHandle(AltText.ToSharedRef(), DetailLayout.GetPropertyUtilities()));
		//const TSharedPtr<FEditableTextPropertyHandle> EditableTextSharedPtr(EditableTextSharedRef);
		
		CurrentNodeCategory.AddCustomRow(LOCTEXT("Text", "Text"))
		.WholeRowContent()
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox) 
			+ SHorizontalBox::Slot().HAlign(HAlign_Left)
			// Just a row that says "Text (node id: ...)"
			[
				SNew(STextBlock).Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(FText::Format(LOCTEXT("TextRowWithId", "Text (node id: {0})"), Dialogue->CurrentNodeId)) // displays current Node Id in inspector
			]
		];

		// Now we build a multiline textbox and the difficult part is, we add a localization button that we build from scratch
		CurrentNodeCategory.AddCustomRow(LOCTEXT("TextValue", "TextValue"))
		.WholeRowContent()
		[
			// this is the text box, it's not complicated:
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.HeightOverride(100)
				[
					SNew(SMultiLineEditableTextBox).Text(CurrentNode.Text)
					.AutoWrapText(true)
					.OnTextCommitted(this, &FDialoguePluginEditorDetails::TextCommitted, Dialogue, Dialogue->CurrentNodeId)
					.ModiferKeyForNewLine(EModifierKey::Shift)
					.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
				]
			]
			// this is the localization button, it's a lot of code copied from STextPropertyEditableTextBox.cpp from line 807:
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SSimpleComboButton)
				.Icon(EditableTextSharedRef, &FEditableTextPropertyHandle::GetAdvancedTextSettingsComboImage)
				.MenuContent()
				[
					SNew(SBox)
					.WidthOverride(340)
					.Padding(1)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						[
							SNew(STextPropertyEditableOptionRow, LinkedBoxManager)
							.Text(LOCTEXT("TextLocalizableLabel", "Localize"))
							.ToolTipText(LOCTEXT("TextLocalizableCheckBoxToolTip", "Whether to assign this text a key and allow it to be gathered for localization.\nIf set to false, marks this text as 'culture invariant' to prevent it being gathered for localization."))
							.ContentHAlign(HAlign_Left)
							[
								SNew(SCheckBox)
								/* The next three properties come from STextPropertyEditableTextBox::Construct (lines 824-826) */
								.IsEnabled(EditableTextSharedRef, &FEditableTextPropertyHandle::IsCultureInvariantFlagEnabled)
								.IsChecked(EditableTextSharedRef, &FEditableTextPropertyHandle::GetLocalizableCheckState)
								.OnCheckStateChanged(EditableTextSharedRef, &FEditableTextPropertyHandle::HandleLocalizableCheckStateChanged)
								//.IsChecked(TAttribute<ECheckBoxState>::Create([EditableTextSharedPtr]() -> ECheckBoxState { ...								
								//.OnCheckStateChanged_Lambda([EditableTextSharedPtr](ECheckBoxState InCheckBoxState) { ...
							]
						]
						+ SVerticalBox::Slot()
						[
							SNew(STextPropertyEditableOptionRow, LinkedBoxManager)
							.IsHeader(true)
							.Text(LOCTEXT("TextReferencedTextLabel", "Referenced Text"))
						]
						+ SVerticalBox::Slot()
						[
							SNew(STextPropertyEditableOptionRow, LinkedBoxManager)
							.Text(LOCTEXT("TextStringTableLabel", "String Table"))
							.IsEnabled(EditableTextSharedRef, &FEditableTextPropertyHandle::IsTextLocalizable)
							[
								SNew(STextPropertyEditableStringTableReference, EditableTextSharedRef)
								.AllowUnlink(true)
								.Font(PropertyNormalFont)
								.IsEnabled(EditableTextSharedRef, &FEditableTextPropertyHandle::CanEdit)
							]
						]
						+ SVerticalBox::Slot()
						[
							SNew(STextPropertyEditableOptionRow, LinkedBoxManager)
							.IsHeader(true)
							.Text(LOCTEXT("TextInlineTextLabel", "Inline Text"))
						]
#if USE_STABLE_LOCALIZATION_KEYS
						+ SVerticalBox::Slot()
						[
							SNew(STextPropertyEditableOptionRow, LinkedBoxManager)
							.Text(LOCTEXT("TextPackageLabel", "Package"))
							.IsEnabled(EditableTextSharedRef, &FEditableTextPropertyHandle::IsTextLocalizable)
							[
								SNew(SEditableTextBox)
								.Text(EditableTextSharedRef, &FEditableTextPropertyHandle::GetPackageValue)
								.Font(PropertyNormalFont)
								.IsReadOnly(true)
							]
						]
#endif // USE_STABLE_LOCALIZATION_KEYS
						+ SVerticalBox::Slot()
						[
							SNew(STextPropertyEditableOptionRow, LinkedBoxManager)
							.Text(LOCTEXT("TextNamespaceLabel", "Namespace"))
							.IsEnabled(EditableTextSharedRef, &FEditableTextPropertyHandle::IsTextLocalizable)
							[
								SAssignNew(EditableTextSharedRef->NamespaceEditableTextBox, SEditableTextBox)
								.Text(EditableTextSharedRef, &FEditableTextPropertyHandle::GetNamespaceValue)
								.Font(PropertyNormalFont)
								.SelectAllTextWhenFocused(true)
								.ClearKeyboardFocusOnCommit(false)
								.OnTextChanged(EditableTextSharedRef, &FEditableTextPropertyHandle::OnNamespaceChanged)
								.OnTextCommitted(EditableTextSharedRef, &FEditableTextPropertyHandle::OnNamespaceCommitted)
								.SelectAllTextOnCommit(true)
								.IsReadOnly(EditableTextSharedRef, &FEditableTextPropertyHandle::IsIdentityReadOnly)
							]
						]
						+ SVerticalBox::Slot()
						[
							SNew(STextPropertyEditableOptionRow, LinkedBoxManager)
							.Text(LOCTEXT("TextKeyLabel", "Key"))
							.IsEnabled(EditableTextSharedRef, &FEditableTextPropertyHandle::IsTextLocalizable)
							[
								SAssignNew(EditableTextSharedRef->KeyEditableTextBox, SEditableTextBox)
								.Text(EditableTextSharedRef, &FEditableTextPropertyHandle::GetKeyValue)
								.Font(PropertyNormalFont)
#if USE_STABLE_LOCALIZATION_KEYS
								.SelectAllTextWhenFocused(true)
								.ClearKeyboardFocusOnCommit(false)
								.OnTextChanged(EditableTextSharedRef, &FEditableTextPropertyHandle::OnKeyChanged)
								.OnTextCommitted(EditableTextSharedRef, &FEditableTextPropertyHandle::OnKeyCommitted)
								.SelectAllTextOnCommit(true)
								.IsReadOnly(EditableTextSharedRef, &FEditableTextPropertyHandle::IsIdentityReadOnly)
#else	// USE_STABLE_LOCALIZATION_KEYS
								.IsReadOnly(true)
#endif	// USE_STABLE_LOCALIZATION_KEYS
							]
						]
					]
				]
			]
		];

		const TSharedPtr<IPropertyHandle> IsPlayerField = Child->GetChildHandle("isPlayer");
		const TSharedPtr<IPropertyHandle> DrawCommentBubble = Child->GetChildHandle("bDrawBubbleComment");
		const TSharedPtr<IPropertyHandle> Comment = Child->GetChildHandle("BubbleComment");
		const TSharedPtr<IPropertyHandle> EventsField = Child->GetChildHandle("Events");
		const TSharedPtr<IPropertyHandle> ConditionsField = Child->GetChildHandle("Conditions");
		const TSharedPtr<IPropertyHandle> SoundField = Child->GetChildHandle("Sound");
		const TSharedPtr<IPropertyHandle> DialogueWaveField = Child->GetChildHandle("DialogueWave");
		
		CurrentNodeCategory.AddProperty(IsPlayerField);
		CurrentNodeCategory.AddProperty(DrawCommentBubble);
		CurrentNodeCategory.AddProperty(Comment);
		CurrentNodeCategory.AddProperty(EventsField);
		CurrentNodeCategory.AddProperty(ConditionsField);		
		CurrentNodeCategory.AddProperty(SoundField);
		CurrentNodeCategory.AddProperty(DialogueWaveField);		
	}

	/* If ChatGPT is enabled, draw 1 additional category and rows inside */
	if (auto DialoguePluginSettings = GetDefault<UDialoguePluginEditorSettings>())
	{
		if (DialoguePluginSettings->bEnableChatGPT)
		{
			// Create a category
			IDetailCategoryBuilder& GptCategory = DetailLayout.EditCategory("Chat GPT", LOCTEXT("ChatGPT", "Chat GPT"), ECategoryPriority::Uncommon);

			// Add a "Disposition" property
			GptCategory.AddProperty(DispositionProperty);

			// Add a row with just "NPC Description" text inside
			GptCategory.AddCustomRow(LOCTEXT("NpcDescription", "NPC Description"))
			.WholeRowContent()
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().HAlign(HAlign_Left)
				// Just a row that says "NPC Description"
				[
					SNew(STextBlock).Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(LOCTEXT("NpcDescriptionRowText", "NPC Description"))
				]
			];

			// Add a row with Dialogue->Description multiline text box
			GptCategory.AddCustomRow(LOCTEXT("DescriptionValue", "DescriptionValue"))
			.WholeRowContent()
			[
				// this is the text box, it's not complicated:
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.HeightOverride(100)
					[
						SNew(SMultiLineEditableTextBox).Text(Dialogue->Description)
						.AutoWrapText(true)
						.OnTextCommitted(this, &FDialoguePluginEditorDetails::DescriptionCommitted, Dialogue)
						.ModiferKeyForNewLine(EModifierKey::Shift)
						.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
					]
				]
			];

			// Add a Generate button
			GptCategory.AddCustomRow(LOCTEXT("GenerateButtonRow", "GenerateButtonRow"))
			.WholeRowContent()
			.HAlign(HAlign_Right)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(0.f, 0.f, 10.f, 0.f)
				.AutoWidth()
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("NotificationList.Throbber"))
					.Visibility(this, &FDialoguePluginEditorDetails::GetThrobberVisibility, Dialogue)
					.RenderTransform(this, &FDialoguePluginEditorDetails::GetThrobberTransform)
					.RenderTransformPivot(FVector2D(.5f, .5f))
					.DesiredSizeOverride(FVector2D(25.f, 25.f))
				]

				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				[
					SNew(SBox)
					.MinDesiredWidth(170.0f)
					[
						SNew(SButton)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left)
						.ContentPadding(FMargin(17.f, 0.f, 0.f, 0.f))
						.IsEnabled(MakeAttributeLambda([Dialogue]
						{
							if (Dialogue->CurrentNodeId == 0) return false; // Start node is not eligible for generation
							return !Dialogue->bGenerating && (Dialogue->CurrentNodeId != -1 || Dialogue->SelectedNodes.Num() > 0);
						}))
						.Text(this, &FDialoguePluginEditorDetails::GetGenerateButtonText, Dialogue)
						.ToolTipText(LOCTEXT("Generate_Tooltip","To generate, select at least 1 node to start from."))
						.OnClicked(this, &FDialoguePluginEditorDetails::OnGenerateButtonClicked, Dialogue)
					]
				]
			];
		}
	}
}

void FDialoguePluginEditorDetails::TextCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UDialogue* Dialogue, int32 id)
{
	int32 index;
	FDialogueNode CurrentNode = Dialogue->GetNodeById(id, index);

	// we don't commit text if it hasn't changed
	if (Dialogue->Data[index].Text.EqualTo(NewText))
	{
		return;
	}
	
	const FScopedTransaction Transaction(LOCTEXT("TextCommitted", "Edited Node Text"));
	Dialogue->Modify();

	UDialogueEditorSubsystem* DialogueEditorSubsystem = GEditor->GetEditorSubsystem<UDialogueEditorSubsystem>();
	DialogueEditorSubsystem->UpdateNodeText(Dialogue, id, NewText);
}

void FDialoguePluginEditorDetails::DescriptionCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UDialogue* Dialogue)
{
	// we don't commit text if it hasn't changed
	if (Dialogue->Description.EqualTo(NewText))
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("DescriptionCommitted", "Edited NPC Description"));
	Dialogue->Modify();

	// avoids being grabbed by localization, since this is for editor use only
	Dialogue->Description = FText::ChangeKey(
		FTextKey(""),
		FTextKey(""),
		NewText
	);
}

FReply FDialoguePluginEditorDetails::OnGenerateButtonClicked(UDialogue* Dialogue)
{
	if (Dialogue->bGenerating) 
	{
		UE_LOG(LogTemp, Warning, TEXT("We're already generating."));
	}
	else 
	{
		Dialogue->bGenerating = true;

		TArray<int32> Nodes;
		for (auto num : Dialogue->SelectedNodes) Nodes.Add(num);

		UDialogueEditorSubsystem* DialogueEditorSubsystem = GEditor->GetEditorSubsystem<UDialogueEditorSubsystem>();
		bool launched = DialogueEditorSubsystem->LaunchGptTask(Dialogue, Nodes);
		if (!launched)
		{
			Dialogue->bGenerating = false;
		}
	}

	return FReply::Handled();
}

/**
 * When the text is being generated, alternate between different (1 to 3) number of dots after "Generating..."
 */
FText FDialoguePluginEditorDetails::GetGenerateButtonText(UDialogue* Dialogue) const
{
	if (Dialogue->bGenerating)
	{
		auto Milliseconds = FDateTime::Now().GetMillisecond();
		int32 Dots;
		if (Milliseconds < 334) {
			Dots = 1;
		}
		else if (Milliseconds < 667) {
			Dots = 2;
		}
		else {
			Dots = 3;
		}

		FString Result = TEXT("Generating Dialogue");
		for (int i = 0; i < Dots; i++)
			Result += TEXT(".");

		return FText::FromString(Result);
	}

	return FText::FromString("Generate Dialogue");
}

TOptional<FSlateRenderTransform> FDialoguePluginEditorDetails::GetThrobberTransform() const
{
	const float DeltaAngle = ThrobberAnimation.GetLerp() * 2 * PI;
	return FSlateRenderTransform(FQuat2D(DeltaAngle));
}

EVisibility FDialoguePluginEditorDetails::GetThrobberVisibility(UDialogue* Dialogue) const
{
	return Dialogue->bGenerating ? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
