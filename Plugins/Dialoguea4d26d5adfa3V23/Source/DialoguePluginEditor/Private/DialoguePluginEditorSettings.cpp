// CodeSpartan 2023

#include "DialoguePluginEditorSettings.h"


UDialoguePluginEditorSettings::UDialoguePluginEditorSettings()
{
	// If GptClientClass isn't set in Config, use default blueprint class
	// Note that "None" won't be overridden by this constructor, so if user resets it to none, we can't set it back to anything
	if (GptClientClass.ToString().Len() == 0)
	{
		GptClientClass = FSoftClassPath(TEXT("/DialoguePlugin/BP_GptApiClient.BP_GptApiClient_C"));
	}
}

TArray<FString> UDialoguePluginEditorSettings::GetDispositions()
{
	const UDialoguePluginEditorSettings* DefaultObj = GetDefault<UDialoguePluginEditorSettings>();
	return DefaultObj->Dispositions;
}

FSoftClassPath UDialoguePluginEditorSettings::GetGptClientClass() const
{
	if (GptClientClass.IsValid())
		return GptClientClass;
	UE_LOG(LogTemp, Warning, TEXT("GptClientClass isn't valid in DialoguePlugin's Settings, so we're falling back to using the default BP_GptApiClient"));
	return FSoftClassPath(TEXT("/DialoguePlugin/BP_GptApiClient.BP_GptApiClient_C"));
}

FString UDialoguePluginEditorSettings::GetModel() const
{
	// if Model is not set, return the first one (in this case GPT 4)
	if (Model.Len() == 0) return GetModelOptions()[0];
	
	return Model;
}
