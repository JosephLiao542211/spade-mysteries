// Spike-only debug window: console command "Llm.DebugChat" opens a Slate window to chat with the test suspect.
// No assets needed. Remove or replace with the real interrogation UI later.

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Types/SlateEnums.h"
#include "LlmDebugChatSubsystem.generated.h"

class SMultiLineEditableTextBox;
class SEditableTextBox;
class STextBlock;
class SWindow;
class USuspectChatSession;
struct FSuspectReply;
enum class ELlmServerState : uint8;

UCLASS()
class SPADE_MYSTERY_API ULlmDebugChatSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void OpenWindow();

private:
	void OnSubmit(const FText& Text, ETextCommit::Type CommitType);
	void OnReply(const FSuspectReply& Reply);
	void OnServerState(ELlmServerState State);
	void Append(const FString& Line);
	void RefreshStatus();

	UPROPERTY()
	TObjectPtr<USuspectChatSession> Session;

	IConsoleObject* Command = nullptr;
	FString Transcript;
	TSharedPtr<SWindow> Window;
	TSharedPtr<SMultiLineEditableTextBox> TranscriptBox;
	TSharedPtr<SEditableTextBox> InputBox;
	TSharedPtr<STextBlock> StatusText;
};
