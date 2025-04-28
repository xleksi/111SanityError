// CodeSpartan 2023

#pragma once

#include "DialoguePluginEditorSettings.generated.h"

UCLASS(config = Engine, defaultconfig)
class DIALOGUEPLUGINEDITOR_API UDialoguePluginEditorSettings : public UObject
{
	GENERATED_BODY()

public:

	UDialoguePluginEditorSettings();

	UPROPERTY(Config, EditAnywhere, Category = "DialoguePlugin")
	bool bEnableChatGPT;

	/** Keys can be created at https://platform.openai.com/account/api-keys */
	UPROPERTY(Config, EditAnywhere, Category = "DialoguePlugin", meta = (DisplayName = "API Key", EditCondition = "bEnableChatGPT"))
	FString APIKey;

	UPROPERTY(Config, EditAnywhere, Category = "DialoguePlugin", meta = (GetOptions = "GetModelOptions", EditCondition = "bEnableChatGPT"))
	FString Model = "gpt-4";

	UPROPERTY(Config, EditAnywhere, Category = "DialoguePlugin", AdvancedDisplay, meta = (EditCondition = "bEnableChatGPT"))
	TArray<FString> CustomModels;

	UPROPERTY(Config, EditAnywhere, Category = "DialoguePlugin", AdvancedDisplay, meta = (EditCondition = "bEnableChatGPT"))
	TArray<FString> Dispositions {"Undefined", "Hostile", "Neutral", "Friendly" };

	/** You can override the Bluetility that performs the API request to ChatGPT by creating your own Object that inherits from ChatGptApiClient class.
	 * If you don't intend to override, just keep the default value, which is BP_GptApiClient */
	UPROPERTY(Config, EditAnywhere, Category = "DialoguePlugin", meta = (ConfigHierarchyEditable, DisplayName = "GPT Client Class Override", AllowedClasses = "/Script/DialoguePluginEditor.ChatGptApiClient", EditCondition = "bEnableChatGPT"))
	FSoftClassPath GptClientClass;

	/** This is a high level System Instruction that precedes your prompt
	 * More on System Instructions: https://help.openai.com/en/articles/7042661-chatgpt-api-transition-guide */
	UPROPERTY(Config, EditAnywhere, Category = "DialoguePlugin", meta = (EditCondition = "bEnableChatGPT"))
	FString SystemInstruction = "You are an assistant that helps write dialogues for a computer video game.";

	/** This is a general instruction that explains the what we're feeding into the prompt and what we expect to get in return */
	UPROPERTY(Config, EditAnywhere, Category = "DialoguePlugin", meta = (EditCondition = "bEnableChatGPT"))
	FString DialogueInstructions = "Below is a JSON with dialogue lines. Each line contains an ID, its parent id, text, and a bool to say whether the line belongs to the player or to the NPC. For every line with empty text, you are to fill it. If a line has text inside parentheses, you are to understand it as intent of the line, and replace it by writing text representing its intent. If a text already exists, you don't modify it. Respond only with a JSON of the same format and with the same number of JSON elements as I'm sending you. Be engaging, natural, authentic, descriptive, creative.";

	/** Sampling temperature to use, between 0 and 2. Higher values like 1.8 will make the output more random,
	 * while lower values like 0.2 will make it more focused and deterministic. Default is 1. Modify only if you know what you're doing. */
	UPROPERTY(Config, EditAnywhere, Category = "DialoguePlugin", meta = (ClampMin = 0, ClampMax = 2, EditCondition = "bEnableChatGPT"))
	float Temperature = 1.0f;

	UFUNCTION()
	TArray<FString> GetModelOptions() const
	{
		TArray<FString> result { "gpt-4", "gpt-3.5-turbo" };
		for (auto CustomModel : CustomModels) result.Add(CustomModel);
		return result;
	}

	// Static function used by UDialogue::NpcDisposition's UPROPERTY to get a dropdown list
	UFUNCTION()
	static TArray<FString> GetDispositions();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "DialoguePlugin")
	FSoftClassPath GetGptClientClass() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "DialoguePlugin")
	FString GetModel() const;
};