#include "DialogueEditor.h"
#include "DialoguePluginEditorPrivatePCH.h"
#include "SSingleObjectDetailsPanel.h"
#include "Runtime/Engine/Public/Slate/SceneViewport.h"
#include "GraphEditor.h"
#include "SEditorViewport.h"
#include "WorkspaceMenuStructureModule.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Editor/EditorEngine.h"
#include "DialogueEditorActions.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Application/SlateApplication.h"
#include "DialogueViewportWidget.h"

#define LOCTEXT_NAMESPACE "DialogueEditor"

const FName DialogueEditorAppName = FName(TEXT("DialogueEditorApp"));

struct FDialogueEditorTabs
{
	// Tab identifiers
	static const FName DetailsID;
	static const FName ViewportID;
	//static const FName DialogueListID;
};

const FName FDialogueEditorTabs::DetailsID(TEXT("Details"));
const FName FDialogueEditorTabs::ViewportID(TEXT("Viewport"));

class SDialoguePropertiesTabBody : public SSingleObjectDetailsPanel
{
public:
	SLATE_BEGIN_ARGS(SDialoguePropertiesTabBody) {}
	SLATE_END_ARGS()

private:
	// Pointer back to owning Dialogue editor instance (the keeper of state)
	TWeakPtr<class FDialogueEditor> DialogueEditorPtr;
public:

	void Construct(const FArguments& InArgs, TSharedPtr<FDialogueEditor> InDialogueEditor)
	{
		DialogueEditorPtr = InDialogueEditor;

		SSingleObjectDetailsPanel::Construct(SSingleObjectDetailsPanel::FArguments().HostCommandList(InDialogueEditor->GetToolkitCommands()), /*bAutomaticallyObserveViaGetObjectToObserve=*/ true, /*bAllowSearch=*/ true);

		// Immediately set property view to display observed object
		// Necessary since 5.0-something, otherwise sometimes details window is empty on asset opening
		TArray<UObject*> SelectedObjects;
		SelectedObjects.Add(GetObjectToObserve());

		PropertyView->SetObjects(SelectedObjects, true);
	}

	// SSingleObjectDetailsPanel interface
	virtual UObject* GetObjectToObserve() const override
	{
		return DialogueEditorPtr.Pin()->GetDialogueBeingEdited().Get();
	}

	virtual TSharedRef<SWidget> PopulateSlot(TSharedRef<SWidget> PropertyEditorWidget) override
	{
		return SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.FillHeight(1)
			[
				PropertyEditorWidget
			];
	}
	// End of SSingleObjectDetailsPanel interface
	
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		if (FSlateApplication::IsInitialized() && DialogueEditorPtr.IsValid() && DialogueEditorPtr.Pin()->GetDialogueBeingEdited().Get()->bRefreshDetails)
		{
			TArray<UObject*> SelectedObjects;
			SelectedObjects.Add(GetObjectToObserve());

			PropertyView->SetObjects(SelectedObjects, true);

			DialogueEditorPtr.Pin()->GetDialogueBeingEdited().Get()->bRefreshDetails = false;
		}
	}
};


FDialogueEditor::FDialogueEditor()
{
	UEditorEngine* Editor = (UEditorEngine*)GEngine;
	if (Editor != NULL)
	{
		Editor->RegisterForUndo(this);
	}
}

FDialogueEditor::~FDialogueEditor()
{
	UEditorEngine* Editor = (UEditorEngine*)GEngine;
	if (Editor)
	{
		Editor->UnregisterForUndo(this);
	}
}

TSharedRef<SDockTab> FDialogueEditor::SpawnTab_Viewport(const FSpawnTabArgs& Args)
{
	TSharedPtr<FDialogueEditor> DialogueEditorPtr = SharedThis(this);

	return SNew(SDockTab)
	.Label(LOCTEXT("ViewportTab_Title", "Viewport"))
	[
		SAssignNew(DialogueViewportWidget, SDialogueViewportWidget, DialogueEditorPtr)			
		.Dialogue(DialogueBeingEdited.Get())
	];	
}

TSharedRef<SDockTab> FDialogueEditor::SpawnTab_Details(const FSpawnTabArgs& Args)
{
	TSharedPtr<FDialogueEditor> DialogueEditorPtr = SharedThis(this);

	// Spawn the details tab
	return SNew(SDockTab)
	.Label(LOCTEXT("DetailsTab_Title", "Details"))
	[
		SNew(SDialoguePropertiesTabBody, DialogueEditorPtr)
	];
}

void FDialogueEditor::PostUndo(bool bSuccess)
{
	//UE_LOG(LogTemp, Log, TEXT("post undo!"));
	if (bSuccess)
	{
		GetDialogueBeingEdited().Get()->CurrentNodeId = -1;

		DialogueViewportWidget->SpawnNodes();
		DialogueViewportWidget->ForceRefresh();

		FSlateApplication::Get().DismissAllMenus();
	}
}

void FDialogueEditor::PostRedo(bool bSuccess)
{
	if (bSuccess)
	{		
		GetDialogueBeingEdited().Get()->CurrentNodeId = -1;

		DialogueViewportWidget->SpawnNodes();
		DialogueViewportWidget->ForceRefresh();
	
		FSlateApplication::Get().DismissAllMenus();
	}
}


void FDialogueEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_DialogueEditor", "Dialogue Editor"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(FDialogueEditorTabs::ViewportID, FOnSpawnTab::CreateSP(this, &FDialogueEditor::SpawnTab_Viewport))
		.SetDisplayName( LOCTEXT("ViewportTab", "Viewport") )
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"));

	InTabManager->RegisterTabSpawner(FDialogueEditorTabs::DetailsID, FOnSpawnTab::CreateSP(this, &FDialogueEditor::SpawnTab_Details))
		.SetDisplayName( LOCTEXT("DetailsTabLabel", "Details") )
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FDialogueEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(FDialogueEditorTabs::ViewportID);
	InTabManager->UnregisterTabSpawner(FDialogueEditorTabs::DetailsID);
	
}

void FDialogueEditor::InitDialogueEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, TObjectPtr<UDialogue> InitDialogue)
{
	DialogueBeingEdited = InitDialogue;
	//EditingObjects.AddUnique(DialogueBeingEdited);
	TSharedPtr<FDialogueEditor> DialogueEditorPtr = SharedThis(this);

	// Default layout
	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_DialogueEditor_Layout_v7")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			/*->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.1f)
				->SetHideTabWell(true)
				->AddTab(GetEditorName(), ETabState::OpenedTab)
			)*/
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				//->SetSizeCoefficient(0.9f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.7f)
					->SetHideTabWell(true)
					->AddTab(FDialogueEditorTabs::ViewportID, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.3f)
					->SetHideTabWell(true)
					->AddTab(FDialogueEditorTabs::DetailsID, ETabState::OpenedTab
					)
				/*	->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.25f)
						->AddTab(FDialogueEditorTabs::DialogueListID, ETabState::OpenedTab)
					)*/
				)
			)
		);

	// Initialize the asset editor
	InitAssetEditor(Mode, InitToolkitHost, DialogueEditorAppName, StandaloneDefaultLayout, /*bCreateDefaultStandaloneMenu=*/ true, /*bCreateDefaultToolbar=*/ true, InitDialogue);
	RegenerateMenusAndToolbars();
	BindCommands();
}

FName FDialogueEditor::GetToolkitFName() const
{
	return FName("DialogueEditor");
}

FText FDialogueEditor::GetBaseToolkitName() const
{
	//return LOCTEXT("DialogueEditorAppLabel", "Dialogue Editor");

	return GetToolkitName();
}

FText FDialogueEditor::GetToolkitName() const
{
	return FText::FromString(DialogueBeingEdited.Get()->GetName());
}

FText FDialogueEditor::GetToolkitToolTipText() const
{
	const TObjectPtr<UObject> EditingObject = DialogueBeingEdited;
	
	check(EditingObject.Get() != NULL);

	return GetToolTipTextForObject(EditingObject.Get());
}

//const TArray<UObject*> * FDialogueEditor::GetObjectsCurrentlyBeingEdited() const
//{
//	return &EditingObjects;
//}

// no longer needed in ue5
//class FEdMode* FDialogueEditor::GetEditorMode() const
//{
//	return NULL;
//}

bool FDialogueEditor::ProcessCommandBindings(const FKeyEvent& InKeyEvent) const
{
	return DialogueEditorCommands->ProcessCommandBindings(InKeyEvent);
}

bool FDialogueEditor::IsAssetEditor() const
{
	return true;
}

FString FDialogueEditor::GetWorldCentricTabPrefix() const
{
	return TEXT("DialogueEditor");
}

//FString FDialogueEditor::GetDocumentationLink() const
//{
//	return TEXT("Engine/Paper2D/FlipbookEditor");
//}

FLinearColor FDialogueEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor::White;
}

void FDialogueEditor::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(DialogueBeingEdited);
}

void FDialogueEditor::BindCommands()
{
	FDialogueEditorCommands::Register();

	// No need to regenerate the commands.
	if (!DialogueEditorCommands.IsValid())
	{
		DialogueEditorCommands = MakeShareable(new FUICommandList);
		{
			DialogueEditorCommands->MapAction(FDialogueEditorCommands::Get().TestCommand,
				FExecuteAction::CreateRaw(this, &FDialogueEditor::OnTestCommand),
				//FCanExecuteAction::CreateSP(this, &FDialoguePluginEditorModule::CanTestCommand, true)
				FCanExecuteAction()
			);

			DialogueEditorCommands->MapAction(FGenericCommands::Get().Duplicate,
				FExecuteAction::CreateRaw(this, &FDialogueEditor::OnDuplicateSelected),
				FCanExecuteAction()
			);

			DialogueEditorCommands->MapAction(FGenericCommands::Get().Copy,
				FExecuteAction::CreateRaw(this, &FDialogueEditor::OnCopySelected),
				FCanExecuteAction()
			);

			DialogueEditorCommands->MapAction(FGenericCommands::Get().Cut,
				FExecuteAction::CreateRaw(this, &FDialogueEditor::OnCutSelected),
				FCanExecuteAction()
			);

			DialogueEditorCommands->MapAction(FGenericCommands::Get().Paste,
				FExecuteAction::CreateRaw(this, &FDialogueEditor::OnPasteNodes),
				FCanExecuteAction()
			);

			DialogueEditorCommands->MapAction(FGenericCommands::Get().Delete,
				FExecuteAction::CreateRaw(this, &FDialogueEditor::OnDelete),
				FCanExecuteAction()
			);
		}
	}
}

void FDialogueEditor::OnTestCommand()
{
	UE_LOG(LogTemp, Log, TEXT("Log: Test"));
}

void FDialogueEditor::OnDuplicateSelected()
{
	DialogueViewportWidget->DuplicateSelected();
}

void FDialogueEditor::OnCopySelected()
{
	DialogueViewportWidget->CopySelected();
}

void FDialogueEditor::OnCutSelected()
{
	DialogueViewportWidget->CutSelected();
}

void FDialogueEditor::OnPasteNodes()
{
	DialogueViewportWidget->PasteNodes();
}

void FDialogueEditor::OnDelete()
{
	DialogueViewportWidget->DeleteSelected();
}

#undef LOCTEXT_NAMESPACE
