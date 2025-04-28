// CodeSpartan 2023

#include "DialogueEditorSubsystem.h"
#include "ChatGptApiClient.h"
#include "Dialogue.h"
#include "DialoguePluginEditorSettings.h"
#include "Internationalization/Text.h"

bool UDialogueEditorSubsystem::LaunchGptTask(UDialogue* Dialogue, TArray<int32> Nodes)
{
	const auto DialoguePluginSettings = GetDefault<UDialoguePluginEditorSettings>();
	auto GptClientClass = DialoguePluginSettings->GetGptClientClass();

	UClass* ObjectClass = StaticLoadClass(UObject::StaticClass(), NULL, *GptClientClass.GetAssetPath().ToString());
	if (!ObjectClass || !IsValidChecked(ObjectClass) || ObjectClass->IsUnreachable())
	{
		UE_LOG(LogTemp, Warning, TEXT("Could not Init GPT Task: %s"), ObjectClass ? *ObjectClass->GetPathName() : TEXT("None"));
		return false;
	}
	
	if (!ObjectClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Missing class: %s"), *ObjectClass->GetPathName());
		return false;
	}
	if (ObjectClass->IsChildOf(AActor::StaticClass()))
	{
		UE_LOG(LogTemp, Warning, TEXT("Could not run because functions on actors can only be called when spawned in a world: %s"), *ObjectClass->GetPathName());
		return false;
	}

	static const FName RunFunctionName("Run");
	UFunction* RunFunction = ObjectClass->FindFunctionByName(RunFunctionName);
	if (RunFunction)
	{
		FEditorScriptExecutionGuard ScriptGuard;
		UObject* Instance = NewObject<UObject>(this, ObjectClass);
		UChatGptApiClient* GptApiClient = dynamic_cast<UChatGptApiClient*>(Instance);
		if (!GptApiClient)
		{
			return false;
		}
		GptApiClient->Initialize(Dialogue, Nodes);
		Instance->ProcessEvent(RunFunction, nullptr); // Run should call StartRequest();
		return true;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Missing function named 'Run': %s"), *ObjectClass->GetPathName());
		return false;
	}
}

bool UDialogueEditorSubsystem::UpdateNodeText(UDialogue* Dialogue, int32 id, FText NewText)
{
	if (!Dialogue)
		return false;

	int32 index;
	FDialogueNode Node = Dialogue->GetNodeById(id, index);

	if (index == -1)
		return false;

	// we don't commit text if it hasn't changed
	if (Dialogue->Data[index].Text.EqualTo(NewText))
		return false;

	if (NewText.IsEmpty())
	{
		Dialogue->Data[index].Text = NewText;
	}
	else
	{
		TOptional<FString> keyOpt = FTextInspector::GetKey(Dialogue->Data[index].Text);
		TOptional<FString> nsOpt = FTextInspector::GetNamespace(Dialogue->Data[index].Text);
		Dialogue->Data[index].Text = FText::ChangeKey(
			FTextKey(nsOpt.IsSet() ? nsOpt.GetValue() : ""),
			FTextKey(keyOpt.IsSet() ? keyOpt.GetValue() : ""),
			NewText
		);
	}

	return true;
}