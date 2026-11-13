// Builds suspect prompts and per-request JSON schemas.
// Mirrors Tools/llm-harness/bench.py: change both together.
// Spike version reads plain JSON from Content/Llm; later this moves to USuspectDefinition data assets.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

struct FLlmFact
{
	FString Id;
	FString Text;
	float PressureNeeded = 0.f;
};

struct FLlmClaim
{
	FString Id;
	FString Text;
};

struct FLlmSuspect
{
	FString Id;
	FString Name;
	FString Character;
	int32 Slot = 0;
	TArray<FLlmClaim> Claims;
	TArray<FLlmFact> Facts;
	TMap<FString, float> TacticMultipliers;
};

struct FLlmCase
{
	FString Id;
	FString Setting;
	FString Output;
	TArray<FString> Rules;
};

struct FLlmMessage
{
	FString Role;
	FString Content;
};

namespace LlmPrompt
{
	const TArray<FString>& Tactics();
	const TArray<FString>& Emotions();

	bool LoadCase(const FString& Path, FLlmCase& Out, FString& OutError);
	bool LoadSuspect(const FString& Path, FLlmSuspect& Out, FString& OutError);

	/** Stable part: cached once per suspect slot. Never put changing state here. */
	FString StaticSystem(const FLlmCase& Case, const FLlmSuspect& Suspect);

	FString PressureWords(float Pressure);
	TArray<const FLlmFact*> AvailableFacts(const FLlmSuspect& Suspect, float Pressure, const TSet<FString>& Revealed);

	/** Changing part: goes in the last user message, after the history. */
	FString StateNote(const FLlmSuspect& Suspect, float Pressure, const TSet<FString>& Revealed);

	/** Schema rebuilt every request; field f only allows currently available fact ids. */
	TSharedRef<FJsonObject> Schema(const FLlmSuspect& Suspect, float Pressure, const TSet<FString>& Revealed);

	/** Length cap and removal of chat-template control tokens. */
	FString SanitizePlayerLine(const FString& Line, int32 MaxChars);

	/** system, then history (user/assistant pairs, byte-identical to earlier turns), then state note + new line. */
	TArray<FLlmMessage> BuildMessages(const FLlmCase& Case, const FLlmSuspect& Suspect,
		const TArray<TPair<FString, FString>>& History, float Pressure, const TSet<FString>& Revealed, const FString& PlayerLine);
}
