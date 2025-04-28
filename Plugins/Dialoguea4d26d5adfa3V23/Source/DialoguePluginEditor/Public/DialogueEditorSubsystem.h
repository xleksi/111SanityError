// CodeSpartan 2023

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "DialogueEditorSubsystem.generated.h"

class UDialogue;

UCLASS()
class DIALOGUEPLUGINEDITOR_API UDialogueEditorSubsystem : public UEditorSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:

	bool LaunchGptTask(UDialogue* Dialogue, TArray<int32> Nodes);

	/** Note: does not create a FScopedTransaction for Undo/Redo or mark the package as dirty. */
	UFUNCTION(BlueprintCallable, Category = "DialoguePlugin")
	bool UpdateNodeText(UDialogue* Dialogue, int32 id, FText NewText);

	// FTickableGameObject interface
	virtual void Tick(float DeltaTime) override { }
	virtual bool IsTickable() const override { return true; }
	virtual bool IsTickableInEditor() const override { return true; }
	virtual ETickableTickType GetTickableTickType() const override { return (HasAnyFlags(RF_ClassDefaultObject) ? ETickableTickType::Always : ETickableTickType::Never); }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UDialogueEditorSubsystem, STATGROUP_Tickables); }
	virtual UWorld* GetTickableGameObjectWorld() const override { return GEditor->GetEditorWorldContext().World(); }
	// ~FTickableGameObject interface
	bool SpecialTick(float dts) { return true; }
};
