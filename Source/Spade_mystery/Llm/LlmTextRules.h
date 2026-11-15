// Text rules that back up the model: a rule-based reading of the detective's tactic, and a guard on the
// spoken reply. Patterns live in Content/Llm/text_rules.json, shared with the Python harness
// (Tools/llm-harness/tactic_rules.py and reply_guard.py): change the JSON, not the code.
// Pure C++ (no UObjects).
#pragma once

#include "CoreMinimal.h"
#include "Llm/LlmCaseData.h"
#include "Llm/LlmEngine.h" // FLlmAvailableSecret, FLlmParsedReply

namespace LlmTextRules
{
	/** Tactic id (one of Case.Tactics) when the rules are confident about the line, otherwise empty.
	 *  When non-empty, use it instead of the model's t: small models misread tactics (E2B ~70%). */
	FString ClassifyTactic(const FString& Line, const FLlmCase& Case);

	/** Tidies the spoken reply: removes *actions*, [directions], (asides), think blocks, name labels,
	 *  narration sentences ("Vera smiles."), and keeps only the dialogue when the model wrote prose with quotes. */
	FString CleanReply(const FString& Reply, const FString& SuspectName);

	/** Problems that make a reply unusable: "empty", "meta" (talks about AI), "echo" (repeats the state note).
	 *  Non-empty: retry the request once, then treat it like broken JSON (no state change). */
	TArray<FString> ReplyIssues(const FString& Reply);

	/** The available secret whose topic the detective's line touches most (topic words, plus anything quoted in
	 *  the topic matched exactly), or empty. The game then forces that reveal (LlmEngine::Schema ForceSecret),
	 *  because small models rarely decide to admit on their own. Mirrors Tools/llm-harness/reveal_rules.py. */
	FString TouchedSecret(const FString& Line, const TArray<FLlmAvailableSecret>& Available);

	/** Vetoes for the model's c and g. Small models fill these at random, and a wrong c counts as a bluff.
	 *  Claiming an item or quoting someone requires the line to mention it; otherwise use "none". */
	bool EvidenceMentioned(const FString& Line, const FLlmEvidence& Evidence);
	bool SuspectMentioned(const FString& Line, const FLlmSuspectDef& Suspect);

	/** Compact JSON for the history, keys in schema order (t, c, g, f, e, r). */
	FString SerializeReply(const FLlmParsedReply& Parsed);
}
