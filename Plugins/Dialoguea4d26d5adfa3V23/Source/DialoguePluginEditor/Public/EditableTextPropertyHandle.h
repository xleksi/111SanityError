#pragma once

#include "Widgets/Text/STextBlock.h"
#include "Internationalization/Text.h"
#include "Styling/StyleColors.h"
#include "STextPropertyEditableTextBox.h"
#include "IPropertyUtilities.h"
#include "Internationalization/TextNamespaceUtil.h"
#include "Widgets/Layout/SLinkedBox.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SEditableTextBox.h"

#define LOCTEXT_NAMESPACE "DialoguePluginEditableText"

namespace DialoguePluginEd
{
	/*
	* This class is from TextCustomization.cpp, but a lot of functions and a few fields have been added to it
	* The functions come from STextPropertyEditableTextBox.cpp,
	* e.g. FEditableTextPropertyHandle::GetLocalizableCheckState comes from STextPropertyEditableTextBox::GetLocalizableCheckState
	* The three fields NamespaceEditableTextBox, KeyEditableTextBox, MultipleValuesText also come from STextPropertyEditableTextBox
	*/

	/** Allows STextPropertyEditableTextBox to edit a property handle */
	class FEditableTextPropertyHandle : public IEditableTextProperty
	{
	public:
		FEditableTextPropertyHandle(const TSharedRef<IPropertyHandle>& InPropertyHandle, const TSharedPtr<IPropertyUtilities>& InPropertyUtilities)
			: PropertyHandle(InPropertyHandle)
			, PropertyUtilities(InPropertyUtilities)
		{
		}

		TSharedPtr<SEditableTextBox> NamespaceEditableTextBox;
		TSharedPtr<SEditableTextBox> KeyEditableTextBox;
		static FText MultipleValuesText;

		ECheckBoxState GetLocalizableCheckState() const
		{
			const int32 NumTexts = GetNumTexts();
			if (NumTexts == 1)
			{
				const FText PropertyValue = GetText(0);

				const bool bIsLocalized = !PropertyValue.IsCultureInvariant();
				return bIsLocalized ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			}

			return ECheckBoxState::Unchecked;
		}

		bool IsCultureInvariantFlagEnabled() const
		{
			return !IsSourceTextReadOnly();
		}

		bool CanEdit() const
		{
			const bool bIsReadOnly = FTextLocalizationManager::Get().IsLocalizationLocked() || IsReadOnly();
			return !bIsReadOnly;
		}

		bool IsSourceTextReadOnly() const
		{
			if (!CanEdit())
			{
				return true;
			}

			// We can't edit the source string of string table references
			const int32 NumTexts = GetNumTexts();
			if (NumTexts == 1)
			{
				const FText TextValue = GetText(0);
				if (TextValue.IsFromStringTable())
				{
					return true;
				}
			}

			return false;
		}

		void HandleLocalizableCheckStateChanged(ECheckBoxState InCheckboxState)
		{
			const int32 NumTexts = GetNumTexts();

			if (InCheckboxState == ECheckBoxState::Checked)
			{
				for (int32 TextIndex = 0; TextIndex < NumTexts; ++TextIndex)
				{
					const FText PropertyValue = GetText(TextIndex);

					// Assign a key to any currently culture invariant texts
					if (PropertyValue.IsCultureInvariant())
					{
						// Get the stable namespace and key that we should use for this property
						FString NewNamespace;
						FString NewKey;
						GetStableTextId(
							TextIndex,
							IEditableTextProperty::ETextPropertyEditAction::EditedKey,
							PropertyValue.ToString(),
							FString(),
							FString(),
							NewNamespace,
							NewKey
						);

						SetText(TextIndex, FText::AsLocalizable_Advanced(*NewNamespace, *NewKey, *PropertyValue.ToString()));
					}
				}
			}
			else
			{
				for (int32 TextIndex = 0; TextIndex < NumTexts; ++TextIndex)
				{
					const FText PropertyValue = GetText(TextIndex);

					// Clear the identity from any non-culture invariant texts
					if (!PropertyValue.IsCultureInvariant())
					{
						const FString* TextSource = FTextInspector::GetSourceString(PropertyValue);
						SetText(TextIndex, FText::AsCultureInvariant(PropertyValue.ToString()));
					}
				}
			}
		}

		bool IsTextLocalizable() const
		{
			const int32 NumTexts = GetNumTexts();
			if (NumTexts == 1)
			{
				const FText PropertyValue = GetText(0);
				return !PropertyValue.IsCultureInvariant();
			}
			return true;
		}

#if USE_STABLE_LOCALIZATION_KEYS
		FText GetPackageValue() const
		{
			FText PackageValue;

			const int32 NumTexts = GetNumTexts();
			if (NumTexts == 1)
			{
				const FText PropertyValue = GetText(0);
				TOptional<FString> FoundNamespace = FTextInspector::GetNamespace(PropertyValue);
				if (FoundNamespace.IsSet())
				{
					PackageValue = FText::FromString(TextNamespaceUtil::ExtractPackageNamespace(FoundNamespace.GetValue()));
				}
			}
			else if (NumTexts > 1)
			{
				PackageValue = MultipleValuesText;
			}

			return PackageValue;
		}

		void OnKeyChanged(const FText& NewText)
		{
			FText ErrorMessage;
			const FText ErrorCtx = LOCTEXT("TextKeyErrorCtx", "Key");
			const bool bIsValidName = IsValidIdentity(NewText, &ErrorMessage, &ErrorCtx);

			if (NewText.IsEmptyOrWhitespace())
			{
				ErrorMessage = LOCTEXT("TextKeyEmptyErrorMsg", "Key cannot be empty so a new key will be assigned");
			}
			else if (bIsValidName)
			{
				// Valid name, so check it won't cause an identity conflict (only test if we have a single text selected to avoid confusion)
				const int32 NumTexts = GetNumTexts();
				if (NumTexts == 1)
				{
					const FText PropertyValue = GetText(0);

					const FString TextNamespace = FTextInspector::GetNamespace(PropertyValue).Get(FString());
					const FString TextKey = NewText.ToString();

					// Get the stable namespace and key that we should use for this property
					// If it comes back with the same namespace but a different key then it means there was an identity conflict
					FString NewNamespace;
					FString NewKey;
					const FString* TextSource = FTextInspector::GetSourceString(PropertyValue);
					GetStableTextId(
						0,
						IEditableTextProperty::ETextPropertyEditAction::EditedKey,
						TextSource ? *TextSource : FString(),
						TextNamespace,
						TextKey,
						NewNamespace,
						NewKey
					);

					if (TextNamespace.Equals(NewNamespace, ESearchCase::CaseSensitive) && !TextKey.Equals(NewKey, ESearchCase::CaseSensitive))
					{
						ErrorMessage = LOCTEXT("TextKeyConflictErrorMsg", "Identity (namespace & key) is being used by a different text within this package so a new key will be assigned");
					}
				}
			}

			KeyEditableTextBox->SetError(ErrorMessage);
		}

		void OnKeyCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
		{
			FText ErrorMessage;
			const FText ErrorCtx = LOCTEXT("TextKeyErrorCtx", "Key");
			if (!IsValidIdentity(NewText, &ErrorMessage, &ErrorCtx))
			{
				return;
			}

			const int32 NumTexts = GetNumTexts();

			// Don't commit the Multiple Values text if there are multiple properties being set
			if (NumTexts > 0 && (NumTexts == 1 || NewText.ToString() != MultipleValuesText.ToString()))
			{
				const FString& TextKey = NewText.ToString();
				for (int32 TextIndex = 0; TextIndex < NumTexts; ++TextIndex)
				{
					const FText PropertyValue = GetText(TextIndex);

					// Only apply the change if the new key is different - we want to keep the keys stable where possible
					const FString CurrentTextKey = FTextInspector::GetKey(PropertyValue).Get(FString());
					if (CurrentTextKey.Equals(TextKey, ESearchCase::CaseSensitive))
					{
						continue;
					}

					// Get the stable namespace and key that we should use for this property
					FString NewNamespace;
					FString NewKey;
					const FString* TextSource = FTextInspector::GetSourceString(PropertyValue);
					GetStableTextId(
						TextIndex,
						IEditableTextProperty::ETextPropertyEditAction::EditedKey,
						TextSource ? *TextSource : FString(),
						FTextInspector::GetNamespace(PropertyValue).Get(FString()),
						TextKey,
						NewNamespace,
						NewKey
					);

					SetText(TextIndex, FText::ChangeKey(NewNamespace, NewKey, PropertyValue));
				}
			}
		}
#endif // USE_STABLE_LOCALIZATION_KEYS

		FText GetNamespaceValue() const
		{
			FText NamespaceValue;

			const int32 NumTexts = GetNumTexts();
			if (NumTexts == 1)
			{
				const FText PropertyValue = GetText(0);
				TOptional<FString> FoundNamespace = FTextInspector::GetNamespace(PropertyValue);
				if (FoundNamespace.IsSet())
				{
					NamespaceValue = FText::FromString(TextNamespaceUtil::StripPackageNamespace(FoundNamespace.GetValue()));
				}
			}
			else if (NumTexts > 1)
			{
				NamespaceValue = MultipleValuesText;
			}

			return NamespaceValue;
		}

		void OnNamespaceChanged(const FText& NewText)
		{
			FText ErrorMessage;
			const FText ErrorCtx = LOCTEXT("TextNamespaceErrorCtx", "Namespace");
			IsValidIdentity(NewText, &ErrorMessage, &ErrorCtx);

			NamespaceEditableTextBox->SetError(ErrorMessage);
		}

		bool IsValidIdentity(const FText& InIdentity, FText* OutReason, const FText* InErrorCtx) const
		{
			const FString InvalidIdentityChars = FString::Printf(TEXT("%s%c%c"), INVALID_NAME_CHARACTERS, TextNamespaceUtil::PackageNamespaceStartMarker, TextNamespaceUtil::PackageNamespaceEndMarker);
			return FName::IsValidXName(InIdentity.ToString(), InvalidIdentityChars, OutReason, InErrorCtx);
		}

		void OnNamespaceCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
		{
			FText ErrorMessage;
			const FText ErrorCtx = LOCTEXT("TextNamespaceErrorCtx", "Namespace");
			if (!IsValidIdentity(NewText, &ErrorMessage, &ErrorCtx))
			{
				return;
			}

			const int32 NumTexts = GetNumTexts();

			// Don't commit the Multiple Values text if there are multiple properties being set
			if (NumTexts > 0 && (NumTexts == 1 || NewText.ToString() != MultipleValuesText.ToString()))
			{
				const FString& TextNamespace = NewText.ToString();
				for (int32 TextIndex = 0; TextIndex < NumTexts; ++TextIndex)
				{
					const FText PropertyValue = GetText(TextIndex);

					// Only apply the change if the new namespace is different - we want to keep the keys stable where possible
					const FString CurrentTextNamespace = TextNamespaceUtil::StripPackageNamespace(FTextInspector::GetNamespace(PropertyValue).Get(FString()));
					if (CurrentTextNamespace.Equals(TextNamespace, ESearchCase::CaseSensitive))
					{
						continue;
					}

					// Get the stable namespace and key that we should use for this property
					FString NewNamespace;
					FString NewKey;
#if USE_STABLE_LOCALIZATION_KEYS
					{
						const FString* TextSource = FTextInspector::GetSourceString(PropertyValue);
						GetStableTextId(
							TextIndex,
							IEditableTextProperty::ETextPropertyEditAction::EditedNamespace,
							TextSource ? *TextSource : FString(),
							TextNamespace,
							FTextInspector::GetKey(PropertyValue).Get(FString()),
							NewNamespace,
							NewKey
						);
					}
#else	// USE_STABLE_LOCALIZATION_KEYS
					{
						NewNamespace = TextNamespace;

						// If the current key is a GUID, then we can preserve that when setting the new namespace
						NewKey = FTextInspector::GetKey(PropertyValue).Get(FString());
						{
							FGuid TmpGuid;
							if (!FGuid::Parse(NewKey, TmpGuid))
							{
								NewKey = FGuid::NewGuid().ToString();
							}
						}
					}
#endif	// USE_STABLE_LOCALIZATION_KEYS

					SetText(TextIndex, FText::ChangeKey(NewNamespace, NewKey, PropertyValue));
				}
			}
		}

		bool IsIdentityReadOnly() const
		{
			if (!CanEdit())
			{
				return true;
			}

			// We can't edit the identity of texts that don't gather for localization
			const int32 NumTexts = GetNumTexts();
			if (NumTexts == 1)
			{
				const FText TextValue = GetText(0);
				if (!TextValue.ShouldGatherForLocalization())
				{
					return true;
				}
			}

			return false;
		}

		FText GetKeyValue() const
		{
			FText KeyValue;

			const int32 NumTexts = GetNumTexts();
			if (NumTexts == 1)
			{
				const FText PropertyValue = GetText(0);
				TOptional<FString> FoundKey = FTextInspector::GetKey(PropertyValue);
				if (FoundKey.IsSet())
				{
					KeyValue = FText::FromString(FoundKey.GetValue());
				}
			}
			else if (NumTexts > 1)
			{
				KeyValue = MultipleValuesText;
			}

			return KeyValue;
		}

		const FSlateBrush* GetAdvancedTextSettingsComboImage() const
		{
			if (IsTextLocalizable())
			{
				return FAppStyle::Get().GetBrush("LocalizationDashboard.MenuIcon");
			}
			else
			{
				return FCoreStyle::Get().GetBrush("Icons.Warning");
			}
		}

		virtual bool IsMultiLineText() const override
		{
			return PropertyHandle->IsValidHandle() && PropertyHandle->GetMetaDataProperty()->GetBoolMetaData("MultiLine");
		}

		virtual bool IsPassword() const override
		{
			return PropertyHandle->IsValidHandle() && PropertyHandle->GetMetaDataProperty()->GetBoolMetaData("PasswordField");
		}

		virtual bool IsReadOnly() const override
		{
			return !PropertyHandle->IsValidHandle() || PropertyHandle->IsEditConst();
		}

		virtual bool IsDefaultValue() const override
		{
			return PropertyHandle->IsValidHandle() && !PropertyHandle->DiffersFromDefault();
		}

		virtual FText GetToolTipText() const override
		{
			return (PropertyHandle->IsValidHandle())
				? PropertyHandle->GetToolTipText()
				: FText::GetEmpty();
		}

		virtual int32 GetNumTexts() const override
		{
			return (PropertyHandle->IsValidHandle())
				? PropertyHandle->GetNumPerObjectValues()
				: 0;
		}

		virtual FText GetText(const int32 InIndex) const override
		{
			if (PropertyHandle->IsValidHandle())
			{
				FString ObjectValue;
				if (PropertyHandle->GetPerObjectValue(InIndex, ObjectValue) == FPropertyAccess::Success)
				{
					FText TextValue;
					if (FTextStringHelper::ReadFromBuffer(*ObjectValue, TextValue))
					{
						return TextValue;
					}
				}
			}

			return FText::GetEmpty();
		}

		virtual void SetText(const int32 InIndex, const FText& InText) override
		{
			if (PropertyHandle->IsValidHandle())
			{
				FString ObjectValue;
				FTextStringHelper::WriteToBuffer(ObjectValue, InText);
				PropertyHandle->SetPerObjectValue(InIndex, ObjectValue);
			}
		}

		virtual bool IsValidText(const FText& InText, FText& OutErrorMsg) const override
		{
			return true;
		}

#if USE_STABLE_LOCALIZATION_KEYS
		virtual void GetStableTextId(const int32 InIndex, const ETextPropertyEditAction InEditAction, const FString& InTextSource, const FString& InProposedNamespace, const FString& InProposedKey, FString& OutStableNamespace, FString& OutStableKey) const override
		{
			if (PropertyHandle->IsValidHandle())
			{
				TArray<UPackage*> PropertyPackages;
				PropertyHandle->GetOuterPackages(PropertyPackages);

				check(PropertyPackages.IsValidIndex(InIndex));

				StaticStableTextId(PropertyPackages[InIndex], InEditAction, InTextSource, InProposedNamespace, InProposedKey, OutStableNamespace, OutStableKey);
			}
		}
#endif // USE_STABLE_LOCALIZATION_KEYS

	private:
		TSharedRef<IPropertyHandle> PropertyHandle;
		TSharedPtr<IPropertyUtilities> PropertyUtilities;
	};

	FText FEditableTextPropertyHandle::MultipleValuesText(NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values"));

	/* Copied from STextPropertyEditableTextBox.cpp
	* Single row in the advanced text settings/localization menu.
	* Has a similar appearance to a details row in the property editor. */
	class STextPropertyEditableOptionRow : public SCompoundWidget
	{
		SLATE_BEGIN_ARGS(STextPropertyEditableOptionRow)
			: _IsHeader(false)
			, _ContentHAlign(HAlign_Fill)
		{}
		SLATE_ARGUMENT(bool, IsHeader)
			SLATE_ARGUMENT(EHorizontalAlignment, ContentHAlign)
			SLATE_ATTRIBUTE(FText, Text)
			SLATE_DEFAULT_SLOT(FArguments, Content)
			SLATE_END_ARGS()

	public:
		void Construct(const FArguments& InArgs, TSharedRef<FLinkedBoxManager> InManager)
		{
			InArgs._Content.Widget->SetToolTip(GetToolTip());

			if (InArgs._IsHeader)
			{
				// Header row, text only, fills entire row
				ChildSlot
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::Get().GetBrush("DetailsView.GridLine"))
					.Padding(FMargin(0, 0, 0, 1))
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::Get().GetBrush("DetailsView.CategoryTop"))
					.BorderBackgroundColor(FSlateColor(FLinearColor::White))
					.Padding(FMargin(12, 8, 0, 8))
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.TextStyle(FAppStyle::Get(), "DetailsView.CategoryTextStyle")
					.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.BoldFont"))
					.Text(InArgs._Text)
					.ToolTip(GetToolTip())
					]
					]
					];
			}
			else
			{
				// Non-header row, has a name column followed by a value widget
				ChildSlot
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SLinkedBox, InManager)
						[
							SNew(SBorder)
							.BorderImage(FAppStyle::Get().GetBrush("DetailsView.GridLine"))
					.Padding(FMargin(0, 0, 0, 1))
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::Get().GetBrush("DetailsView.CategoryMiddle"))
					.BorderBackgroundColor(this, &STextPropertyEditableOptionRow::GetBackgroundColor)
					.Padding(FMargin(20, 3.5, 0, 3.5))
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
					.Text(InArgs._Text)
					.ToolTip(GetToolTip())
					]
					]
						]
					]
				+ SHorizontalBox::Slot()
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::Get().GetBrush("DetailsView.GridLine"))
					.Padding(FMargin(0, 0, 0, 1))
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::Get().GetBrush("DetailsView.CategoryMiddle"))
					.BorderBackgroundColor(this, &STextPropertyEditableOptionRow::GetBackgroundColor)
					.Padding(FMargin(14, 3.5, 4, 3.5))
					.HAlign(InArgs._ContentHAlign)
					.VAlign(VAlign_Center)
					[
						InArgs._Content.Widget
					]
					]
					]
					];

				// Clear the tooltip from this widget since it's set on the name/value widgets now
				SetToolTip(nullptr);
			}
		}

	private:
		FSlateColor GetBackgroundColor() const
		{
			if (IsHovered())
			{
				return FStyleColors::Header;
			}

			return FStyleColors::Panel;
		}
	};
}

#undef LOCTEXT_NAMESPACE