// Interrogation engine: meters, secret keys, rules, evidence, bluffs, gossip, prompt and schema building,
// turn resolution and accusation. Pure C++ (no UObjects), so it can be unit-tested.
// Mirrors Tools/llm-harness/engine.py function for function: change both together.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Llm/LlmCaseData.h"

struct FLlmMessage
{
	FString Role;
	FString Content;
};

struct FLlmAvailableSecret
{
	FString Id;
	FString Text;
	FString Topic;
};

/** The model's classification and reply for one turn (schema fields t, c, g, f, e, r). */
struct FLlmParsedReply
{
	FString Tactic;           // t
	FString ClaimedEvidence;  // c
	FString RepeatedSuspect;  // g
	FString Secret;           // f
	FString Emotion;          // e
	FString Reply;            // r
};

struct FLlmTurnEvent
{
	FLlmEffects Deltas;
	FString Revealed;
	FString RevealedText;
	TArray<FString> Granted;
	FString Gossip;
	bool bBluff = false;
	TArray<FString> Triggers;
};

struct FLlmAccusationResult
{
	bool bWin = false;
	FString Ending; // ending id from case.json
	FString Title;
	FString Text;
};

namespace LlmEngine
{
	inline const FString& None() { static const FString N = TEXT("none"); return N; }

	bool CondOk(const FLlmCondition& Cond, const FLlmSuspectState& St);
	void AddMeter(const FLlmCase& Case, FLlmCaseState& State, const FString& Sid, const FString& Meter, float Delta);
	void ApplyEffects(const FLlmCase& Case, FLlmCaseState& State, const FString& Sid, const FLlmEffects* Effects);
	FString WordsFor(const FLlmMeterWords& Bands, float Value);

	TArray<const FLlmRule*> WhileRules(const FLlmSuspectDef& Def, const FLlmSuspectState& St);
	bool SecretsBlocked(const FLlmSuspectDef& Def, const FLlmSuspectState& St);
	/** Secrets whose key is satisfied now. Only these go into the prompt and the schema. */
	TArray<FLlmAvailableSecret> AvailableSecrets(const FLlmSuspectDef& Def, const FLlmSuspectState& St);

	/** Stable prefix: cached once per suspect slot. */
	FString StaticSystem(const FLlmCase& Case, const FString& Sid);
	/** Changing part: goes in the last user message, after the history. */
	FString StateNote(const FLlmCase& Case, const FLlmCaseState& State, const FString& Sid);
	TSharedRef<FJsonObject> Schema(const FLlmCase& Case, const FLlmCaseState& State, const FString& Sid);

	FString DetectiveMessage(const FString& Line);
	FString EvidenceMessage(const FLlmCase& Case, const FString& EvidenceId);

	/** The player repeated Source's words to Sid (typed, or by presenting Source's statement). Once per turn. */
	void ApplyGossip(const FLlmCase& Case, FLlmCaseState& State, const FString& Sid, const FString& Source, FLlmTurnEvent& Ev);
	/** First time Sid hears or sees an item: its effects, plus gossip if it is someone's statement. */
	void SeeEvidence(const FLlmCase& Case, FLlmCaseState& State, const FString& Sid, const FString& EvidenceId, FLlmTurnEvent& Ev);
	TArray<FLlmMessage> BuildMessages(const FLlmCase& Case, const FLlmCaseState& State, const FString& Sid, const FString& LastUser);

	/** True if the line uses any part of the suspect's name ("Tommy", "Mullen"). */
	bool NamesSuspect(const FString& Line, const FLlmSuspectDef& Def);
	/** What the player reads when a secret comes out, in the third person: the secret's clue, or the statements it
	 *  grants. The secret's own text is written to the suspect ("You pulled the main fuse..."). */
	FString ClueText(const FLlmCase& Case, const FString& Sid, const FString& SecretId, const FString& Fallback);

	/** Apply one exchange. Parsed = nullptr if the reply was invalid. PresentedEvidence non-empty for an evidence event.
	 *  Allowed = secrets available when the request was sent (id -> text). */
	FLlmTurnEvent ResolveTurn(const FLlmCase& Case, FLlmCaseState& State, const FString& Sid, const FString& UserMsg,
		const FLlmParsedReply* Parsed, const FString& Raw, const TMap<FString, FString>& Allowed, const FString& PresentedEvidence);

	FLlmAccusationResult Accuse(const FLlmCase& Case, const FLlmCaseState& State, const FString& Accused);

	/** Length cap and removal of chat-template control tokens. */
	FString SanitizePlayerLine(const FString& Line, int32 MaxChars);
}
