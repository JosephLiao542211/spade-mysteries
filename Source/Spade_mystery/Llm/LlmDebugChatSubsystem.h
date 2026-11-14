// Spike-only debug window: console command "Llm.DebugChat" opens a Slate window to interrogate every suspect
// in the case from Project Settings > Local LLM > CaseDir. Shows meters, how each line was read, clue cards,
// evidence buttons and accusation. No assets needed. Replace with the real interrogation UI later.

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
class SWrapBox;
class UInterrogationSession;
struct FInterrogationReply;
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
	void SelectSuspect(const FString& SuspectId);
	void OnSubmit(const FText& Text, ETextCommit::Type CommitType);
	void OnPresent(const FString& EvidenceId);
	void OnAccuse();
	void OnReply(const FInterrogationReply& Reply);
	void OnServerState(ELlmServerState State);
	void Append(const FString& SuspectId, const FString& Line);
	void RefreshTranscript();
	void RefreshStatus();
	void RebuildEvidenceButtons();
	FString SuspectName(const FString& SuspectId) const;

	UPROPERTY()
	TObjectPtr<UInterrogationSession> Session;

	IConsoleObject* Command = nullptr;
	FString CurrentSuspect;
	TMap<FString, FString> Transcripts;
	TSharedPtr<SWindow> Window;
	TSharedPtr<SMultiLineEditableTextBox> TranscriptBox;
	TSharedPtr<SEditableTextBox> InputBox;
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<STextBlock> HeaderText;
	TSharedPtr<SWrapBox> EvidenceBox;
};
