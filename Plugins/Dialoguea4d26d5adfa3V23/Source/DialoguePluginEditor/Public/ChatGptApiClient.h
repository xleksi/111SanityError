// CodeSpartan 2023

#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityObject.h"
#include "Dialogue.h"
#include "Interfaces/IHttpRequest.h"
#include "ChatGptApiClient.generated.h"

class UDialogue;


USTRUCT()
struct FGptMessage
{
	GENERATED_BODY()

	UPROPERTY()
	FString role;

	UPROPERTY()
	FString content;
};

USTRUCT()
struct FGptRequest
{
	GENERATED_BODY()

	UPROPERTY()
	FString model = TEXT("");

	UPROPERTY()
	TArray<FGptMessage> messages;

	UPROPERTY()
	float temperature = 0.0f;
};

USTRUCT(BlueprintType)
struct FDialogueLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Node")
	int32 Id = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Node")
	bool SpokenByPlayer = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Node")
	FString Text = TEXT("");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Node")
	int32 ParentId = 0;
};

USTRUCT(BlueprintType)
struct FDialogueLines
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Node")
	TArray<FDialogueLine> DialogueLines;
};


UCLASS(BlueprintType)
class DIALOGUEPLUGINEDITOR_API UChatGptApiClient : public UEditorUtilityObject
{
	GENERATED_BODY()

public:
	void Initialize(UDialogue* InDialogue, TArray<int32> Nodes);

	UFUNCTION(BlueprintCallable, Category = "Dialogue Plugin")
	void JobDone();

	UFUNCTION(BlueprintPure, Category = "Dialogue Plugin")
	FString GetModel();

	UFUNCTION(BlueprintCallable, Category = "DialoguePlugin")
	void StartRequest();

	UFUNCTION()
	void OnResponse(const FString& ResponseContent, TWeakObjectPtr<UChatGptApiClient> thisObj);

	void ResponseFailed(FHttpRequestPtr HttpRequestFulfilled);

	UFUNCTION(BlueprintNativeEvent, Category = "Dialogue Plugin")
	FString CreateRequestFull();

	UFUNCTION(BlueprintNativeEvent, Category = "Dialogue Plugin")
	FString CreateRequestBody();

	UFUNCTION(BlueprintNativeEvent, Category = "Dialogue Plugin")
	void ProcessResponse(const FString& ResponseContent);

	UFUNCTION(BlueprintNativeEvent, Category = "Dialogue Plugin")
	void ProcessContent(const FString& ResponseContent);

	UFUNCTION()
	TArray<FDialogueLine> ConvertToDialogueLines(TArray<FDialogueNode>& Nodes);

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue Plugin")
	TWeakObjectPtr<UDialogue> Dialogue;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue Plugin")
	TArray<int32> SelectedNodeIds;
};