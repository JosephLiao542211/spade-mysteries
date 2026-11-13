// One conversation with one suspect: holds the game's truth (pressure, revealed facts, history)
// and turns each player line into a constrained llama-server request.

#pragma once

#include "CoreMinimal.h"
#include "Llm/LlmPromptBuilder.h"
#include "UObject/Object.h"
#include "SuspectChatSession.generated.h"

class ULlmServerSubsystem;

USTRUCT(BlueprintType)
struct FSuspectReply
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) bool bOk = false;
	UPROPERTY(BlueprintReadOnly) FString Error;

	UPROPERTY(BlueprintReadOnly) FString PlayerTactic;
	UPROPERTY(BlueprintReadOnly) FString Emotion;
	/** Fact the game accepted as revealed this turn, or empty. */
	UPROPERTY(BlueprintReadOnly) FString RevealedFactId;
	UPROPERTY(BlueprintReadOnly) FString Reply;

	UPROPERTY(BlueprintReadOnly) float PressureAfter = 0.f;

	/** Diagnostics: tokens read this turn (low = prompt cache working), tokens generated, wall time. */
	UPROPERTY(BlueprintReadOnly) int32 PromptTokens = 0;
	UPROPERTY(BlueprintReadOnly) int32 GeneratedTokens = 0;
	UPROPERTY(BlueprintReadOnly) float Seconds = 0.f;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSuspectReply, const FSuspectReply& /*Reply*/);

UCLASS()
class SPADE_MYSTERY_API USuspectChatSession : public UObject
{
	GENERATED_BODY()

public:
	/** Paths are relative to the Content directory, e.g. "Llm/case.json". */
	bool Init(ULlmServerSubsystem* InServer, const FString& CasePath, const FString& SuspectPath, FString& OutError);

	/** Send the static prefix once so the suspect's first real reply is fast. Call while a loading screen is up. */
	void Preload();

	/** Returns false if a request is already in flight or the server is not ready. */
	bool Send(const FString& PlayerLine);

	bool IsBusy() const { return bBusy; }
	float GetPressure() const { return Pressure; }
	const FLlmSuspect& GetSuspect() const { return Suspect; }
	const TSet<FString>& GetRevealed() const { return Revealed; }

	FOnSuspectReply OnReply;

private:
	void PostChat(const TArray<FLlmMessage>& Messages, const TSharedRef<FJsonObject>& Schema, int32 MaxTokens,
		TFunction<void(bool bOk, const TSharedPtr<FJsonObject>& Response, const FString& Error)> Done);
	void HandleReply(const FString& PlayerLine, const TSet<FString>& AllowedFacts, double StartTime,
		bool bOk, const TSharedPtr<FJsonObject>& Response, const FString& Error);

	TWeakObjectPtr<ULlmServerSubsystem> Server;
	FLlmCase Case;
	FLlmSuspect Suspect;

	float Pressure = 0.f;
	TSet<FString> Revealed;
	/** Player line, raw assistant JSON. Kept byte-identical so llama-server's prefix cache matches. */
	TArray<TPair<FString, FString>> History;
	bool bBusy = false;
};
