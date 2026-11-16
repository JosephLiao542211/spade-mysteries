// Case and suspect data for the interrogation engine, loaded from Content/Llm/Cases/<Case>/*.json.
// Format documented in docs/cases/spade-club.md. Mirrors Tools/llm-harness/engine.py.

#pragma once

#include "CoreMinimal.h"

using FLlmEffects = TMap<FString, float>;
using FLlmMeterWords = TArray<TPair<float, FString>>;

/** All keys must hold: "<meter>_gte", "<meter>_lt", "evidence_seen", "revealed". */
struct FLlmCondition
{
	TMap<FString, float> Gte;
	TMap<FString, float> Lt;
	TArray<FString> EvidenceSeen;
	TArray<FString> Revealed;
};

struct FLlmMeterDef
{
	FString Name;
	/** Shown on the suspect's bar in the UI ("Composure"). Empty = Name. */
	FString Label;
	float Start = 0.f;
	/** Empty = use the case's default words for this meter (fear). */
	FLlmMeterWords Words;
};

struct FLlmRule
{
	/** "while": applies as long as the condition holds. "trigger": fires once when it becomes true (re-arms when false). */
	bool bWhile = true;
	FLlmCondition When;
	FString Note;
	bool bBlockSecrets = false;
	int32 ShutdownTurns = 0;
	FLlmEffects AfterShutdownSet;
	bool bEndInterview = false;
	/** Applied to every other suspect when a trigger fires. */
	FLlmEffects Others;
	/** Suspects whose interviews also end when this trigger fires (he walks out and takes his wife with him). */
	TArray<FString> CloseOthers;
};

struct FLlmSecretRoute
{
	FLlmCondition When;
	/** Empty = the secret's main text. A different route can give a distorted or partial version. */
	FString Text;
};

struct FLlmSecret
{
	FString Id;
	FString Topic;
	FString Text;
	TArray<FString> GrantsEvidence;
	TArray<FLlmSecretRoute> Routes;
};

struct FLlmClaim
{
	FString Id;
	FString Text;
};

/** Positive changes to Meter are multiplied by Factor while When holds (frightened men don't think about loyalty). */
struct FLlmDampen
{
	FString Meter;
	FLlmCondition When;
	float Factor = 1.f;
};

struct FLlmSuspectDef
{
	FString Id;
	FString Name;
	FString Character;
	int32 Slot = 0;
	TArray<FLlmClaim> Claims;
	TArray<FLlmMeterDef> Meters; // ordered: state note lists them in this order
	TArray<FLlmDampen> Dampen;
	TMap<FString, FLlmEffects> TacticEffects;
	TMap<FString, FLlmEffects> EvidenceEffects;
	FLlmEffects BluffEffects;
	/** Non-empty = this suspect sees through bluffs and the next state note says so. */
	FString BluffNote;
	/** Applied to this suspect whenever the player repeats their words to anyone. */
	FLlmEffects GossipCost;
	/** When the player repeats someone's words to THIS suspect: effects on this suspect and on the quoted one. */
	FLlmEffects GossipHeardSelf;
	FLlmEffects GossipHeardSource;
	TArray<FLlmRule> Rules;
	TArray<FLlmSecret> Secrets;

	const FLlmMeterDef* FindMeter(const FString& Name) const { return Meters.FindByPredicate([&](const FLlmMeterDef& M) { return M.Name == Name; }); }
	const FLlmSecret* FindSecret(const FString& InId) const { return Secrets.FindByPredicate([&](const FLlmSecret& S) { return S.Id == InId; }); }
};

struct FLlmEvidence
{
	FString Id;
	FString Name;
	FString Description;
	/** Non-empty = a statement by this suspect. Presenting it to someone else counts as repeating their words. */
	FString Source;
	bool bFoundAtStart = false;
};

/** A witness backs a proof if they revealed Secret, are still in the club, and When still holds for them. */
struct FLlmWitness
{
	FString Suspect;
	FString Secret;
	FLlmCondition When;
	/** Said when When fails. Empty = "<name> takes back their statement." */
	FString Recant;
};

struct FLlmProof
{
	FString Label; // "a motive": listed when missing
	TArray<FLlmWitness> Witnesses; // any one is enough
};

struct FLlmEnding
{
	FString Title;
	/** "{name}" = the accused, "{missing}" = the proofs that were missing. */
	FString Text;
};

/** Added to the ending text when When holds for Suspect (and the ending is one of Endings, if any are listed). */
struct FLlmEpilogue
{
	FString Suspect;
	FLlmCondition When;
	FString Text;
	TArray<FString> Endings;
};

struct FLlmAccusation
{
	FString Culprit;
	TArray<FString> ConfessionSecrets;
	TArray<FLlmProof> Proofs; // all are needed without a confession
	TMap<FString, FLlmEnding> Endings;
	FString OnConfession;
	FString OnProof;
	FString OnNoCase;
	/** Suspect id (or "default") -> ending id for accusing the wrong person. */
	TMap<FString, FString> OnWrong;
	TArray<FLlmEpilogue> Epilogues;
};

struct FLlmCase
{
	FString Id;
	FString Title;
	FString Setting;
	TArray<FString> Rules;
	TArray<TPair<FString, FString>> Tactics; // id, definition (ordered)
	TArray<FString> Emotions;
	TMap<FString, FLlmMeterWords> MeterWords;
	TArray<FLlmEvidence> Evidence;
	TArray<FLlmSuspectDef> Suspects; // ordered as listed in case.json
	FLlmAccusation Accusation;

	const FLlmSuspectDef* FindSuspect(const FString& InId) const { return Suspects.FindByPredicate([&](const FLlmSuspectDef& S) { return S.Id == InId; }); }
	const FLlmEvidence* FindEvidence(const FString& InId) const { return Evidence.FindByPredicate([&](const FLlmEvidence& E) { return E.Id == InId; }); }
};

/** Runtime state of one suspect. The game's truth; the model never owns any of it. */
struct FLlmSuspectState
{
	TMap<FString, float> Meters;
	/** Secret id -> the text actually unlocked (route version). Shown on the clue card. */
	TMap<FString, FString> Revealed;
	/** (user message, raw assistant JSON), replayed byte-identically so the prompt cache matches. */
	TArray<TPair<FString, FString>> History;
	TSet<FString> EvidenceSeen;
	int32 ShutdownLeft = 0;
	FString ShutdownNote;
	FLlmEffects AfterShutdownSet;
	TSet<int32> Fired;
	bool bClosed = false;
	bool bCaughtBluff = false;

	float Meter(const FString& Name) const { const float* V = Meters.Find(Name); return V ? *V : 0.f; }
};

struct FLlmCaseState
{
	TMap<FString, FLlmSuspectState> Suspects;
	TSet<FString> Inventory;

	void Init(const FLlmCase& Case);
};

namespace LlmCaseData
{
	/** CaseDir is absolute; reads case.json and every suspect file it lists. */
	bool LoadCase(const FString& CaseDir, FLlmCase& Out, FString& OutError);
}
