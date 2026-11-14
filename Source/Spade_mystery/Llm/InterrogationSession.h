// One playthrough of a case: owns every suspect's state, the evidence inventory and the requests to
// llama-server. The game's truth lives here (via LlmEngine); the model only classifies and plays the scene.

#pragma once

#include "CoreMinimal.h"
#include "Llm/LlmCaseData.h"
#include "Llm/LlmEngine.h"
#include "UObject/Object.h"
#include "InterrogationSession.generated.h"

class ULlmServerSubsystem;

USTRUCT(BlueprintType)
struct FInterrogationReply
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString SuspectId;
	UPROPERTY(BlueprintReadOnly) bool bOk = false;
	UPROPERTY(BlueprintReadOnly) FString Error;
	/** True if this answered an evidence presentation rather than a typed line. */
	UPROPERTY(BlueprintReadOnly) bool bEvidenceEvent = false;

	/** How the model read the detective's line: show this to the player ("Vera took that as a threat"). */
	UPROPERTY(BlueprintReadOnly) FString PlayerTactic;
	UPROPERTY(BlueprintReadOnly) FString ClaimedEvidence;
	UPROPERTY(BlueprintReadOnly) FString RepeatedSuspect;
	UPROPERTY(BlueprintReadOnly) bool bBluffDetected = false;

	UPROPERTY(BlueprintReadOnly) FString Emotion;
	UPROPERTY(BlueprintReadOnly) FString Reply;

	/** Secret accepted this turn, with its canonical text for the clue card. */
	UPROPERTY(BlueprintReadOnly) FString RevealedSecretId;
	UPROPERTY(BlueprintReadOnly) FString RevealedText;
	UPROPERTY(BlueprintReadOnly) FString GrantedEvidenceId;

	UPROPERTY(BlueprintReadOnly) bool bInterviewClosed = false;
	UPROPERTY(BlueprintReadOnly) TArray<FString> TriggeredNotes;
	UPROPERTY(BlueprintReadOnly) TMap<FString, float> Meters;

	/** Diagnostics: tokens read this turn (low = prompt cache working), tokens generated, wall time. */
	UPROPERTY(BlueprintReadOnly) int32 PromptTokens = 0;
	UPROPERTY(BlueprintReadOnly) int32 GeneratedTokens = 0;
	UPROPERTY(BlueprintReadOnly) float Seconds = 0.f;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnInterrogationReply, const FInterrogationReply& /*Reply*/);

UCLASS()
class SPADE_MYSTERY_API UInterrogationSession : public UObject
{
	GENERATED_BODY()

public:
	/** CaseDir relative to the Content directory, e.g. "Llm/Cases/SpadeClub". */
	bool Init(ULlmServerSubsystem* InServer, const FString& CaseDir, FString& OutError);

	/** Cache every suspect's static prefix in its slot. Call behind a loading screen once the server is ready. */
	void PreloadAll();

	/** Typed line to a suspect. False if that suspect is busy or the server is not ready. */
	bool Ask(const FString& SuspectId, const FString& PlayerLine);

	/** Show an evidence item (must be in the inventory) to a suspect. */
	bool Present(const FString& SuspectId, const FString& EvidenceId);

	FLlmAccusationResult Accuse(const FString& SuspectId) const { return LlmEngine::Accuse(Case, State, SuspectId); }

	bool IsBusy(const FString& SuspectId) const { return Busy.Contains(SuspectId); }
	const FLlmCase& GetCase() const { return Case; }
	const FLlmCaseState& GetState() const { return State; }

	FOnInterrogationReply OnReply;

private:
	bool Send(const FString& SuspectId, const FString& UserMsg, const FString& PresentedEvidence);
	void PostChat(int32 Slot, const TArray<FLlmMessage>& Messages, const TSharedRef<FJsonObject>& Schema, int32 MaxTokens,
		TFunction<void(bool bOk, const TSharedPtr<FJsonObject>& Response, const FString& Error)> Done);
	void HandleReply(const FString& SuspectId, const FString& UserMsg, const FString& PresentedEvidence,
		const TMap<FString, FString>& Allowed, double StartTime, bool bOk, const TSharedPtr<FJsonObject>& Response, const FString& Error);

	TWeakObjectPtr<ULlmServerSubsystem> Server;
	FLlmCase Case;
	FLlmCaseState State;
	TSet<FString> Busy;
};
