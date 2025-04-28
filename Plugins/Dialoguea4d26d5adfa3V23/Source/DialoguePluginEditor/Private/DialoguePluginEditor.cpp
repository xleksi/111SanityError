#include "DialoguePluginEditor.h"
#include "DialoguePluginEditorPrivatePCH.h"
#include "IDialoguePluginEditorModule.h"
#include "DialoguePluginEditorDetails.h"
#include "DialogueAssetTypeActions.h"
#include "DialogueEditorStyle.h"
#include "Runtime/Core/Public/Features/IModularFeatures.h"
#include "Runtime/SlateCore/Public/Rendering/SlateRenderer.h"
#include "Editor/MainFrame/Public/Interfaces/IMainFrameModule.h"
#include "ISettingsModule.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "DialoguePluginEditorSettings.h"

#define LOCTEXT_NAMESPACE "DialoguePluginEditor"

uint32 FDialoguePluginEditorModule::GameAssetCategory;

IMPLEMENT_MODULE(FDialoguePluginEditorModule, DialoguePluginEditor)

// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
void FDialoguePluginEditorModule::StartupModule()
{	
	// registers the style (sets icons)
	FDialogueEditorStyle::Initialize(); 

	// Register asset types
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	// creates a category named Dialogue Plugin in the advanced assets category (rmb)
	// this uint32 may then be used by factories that want their custom type to be listed in this category as well
	GameAssetCategory = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("DialoguePlugin")), LOCTEXT("DialoguePluginCategory", "Dialogue Plugin"));

	// Register settings
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Project", "Plugins", "Dialogue Plugin",
			LOCTEXT("RuntimeSettingsName", "Dialogue Plugin"),
			LOCTEXT("RuntimeSettingsDescription", "Configure Dialogue Plugin"),
			GetMutableDefault<UDialoguePluginEditorSettings>());
	}

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout("Dialogue", FOnGetDetailCustomizationInstance::CreateStatic(&FDialoguePluginEditorDetails::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();

	//EAssetTypeCategories::Type AssetCategoryBit = EAssetTypeCategories::Misc;

	// register AssetTypeActions (changes their background color)
	AssetTools.RegisterAssetTypeActions(MakeShareable(new FDialogueAssetTypeActions(GameAssetCategory)));
}

void FDialoguePluginEditorModule::Initialize( TSharedPtr<SWindow> InRootWindow, bool bIsNewProjectWindow )
{

}

void FDialoguePluginEditorModule::AddGraphicsSwitcher( FToolBarBuilder& ToolBarBuilder )
{
	
}

void FDialoguePluginEditorModule::ShutdownModule()
{
	FDialogueEditorStyle::Shutdown();
	// Unregister settings
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "DialoguePlugin");
	}
}

bool FDialoguePluginEditorModule::AllowAutomaticGraphicsSwitching()
{
	return bAllowAutomaticGraphicsSwitching;
}

bool FDialoguePluginEditorModule::AllowMultipleGPUs()
{
	return bAllowMultiGPUs;
}

void FDialoguePluginEditorModule::RegisterAssetTypeAction(class IAssetTools& AssetTools, TSharedRef<class IAssetTypeActions> Action)
{
	AssetTools.RegisterAssetTypeActions(Action);
}

#undef LOCTEXT_NAMESPACE