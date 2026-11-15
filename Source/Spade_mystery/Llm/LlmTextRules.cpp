#include "Llm/LlmTextRules.h"

#include "Dom/JsonObject.h"
#include "Internationalization/Regex.h"
#include "Llm/LlmEngine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogLlmTextRules, Log, All);

namespace
{
	struct FTextRules
	{
		TArray<TPair<FString, FString>> Tactics; // tactic id, pattern (in priority order)
		FString Shout, Question, CitesTestimony, GuardMeta, GuardEcho, GuardAction;
		bool bLoaded = false;
	};

	const FTextRules& Rules()
	{
		static FTextRules R;
		static bool bTried = false;
		if (bTried) return R;
		bTried = true;

		const FString Path = FPaths::ProjectContentDir() / TEXT("Llm/text_rules.json");
		FString Text;
		TSharedPtr<FJsonObject> J;
		if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J.IsValid())
		{
			UE_LOG(LogLlmTextRules, Warning, TEXT("Could not load %s; tactic rules and reply checks are off."), *Path);
			return R;
		}
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (J->TryGetArrayField(TEXT("tactic_rules"), Arr))
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				const TArray<TSharedPtr<FJsonValue>>& Pair = V->AsArray();
				if (Pair.Num() == 2) R.Tactics.Emplace(Pair[0]->AsString(), Pair[1]->AsString());
			}
		}
		J->TryGetStringField(TEXT("shout"), R.Shout);
		J->TryGetStringField(TEXT("question"), R.Question);
		J->TryGetStringField(TEXT("cites_testimony"), R.CitesTestimony);
		J->TryGetStringField(TEXT("guard_meta"), R.GuardMeta);
		J->TryGetStringField(TEXT("guard_echo"), R.GuardEcho);
		J->TryGetStringField(TEXT("guard_action"), R.GuardAction);
		R.bLoaded = true;
		return R;
	}

	/** Case-insensitive search. Patterns are shared with Python, so (?i) is added here, not in the JSON. */
	bool Matches(const FString& Pattern, const FString& Text)
	{
		if (Pattern.IsEmpty()) return false;
		const FRegexPattern P(TEXT("(?i)") + Pattern);
		FRegexMatcher M(P, Text);
		return M.FindNext();
	}

	int32 CountLetters(const FString& S)
	{
		int32 N = 0;
		for (const TCHAR C : S) if (FChar::IsAlpha(C)) ++N;
		return N;
	}

	FString CollapseSpaces(const FString& In)
	{
		FString Out;
		bool bSpace = false;
		for (const TCHAR C : In)
		{
			if (FChar::IsWhitespace(C)) { bSpace = true; continue; }
			if (bSpace && !Out.IsEmpty()) Out.AppendChar(TEXT(' '));
			bSpace = false;
			Out.AppendChar(C);
		}
		return Out;
	}

	/** Removes every Open...Close span (Open == Close for *actions*). */
	FString RemoveSpans(const FString& In, TCHAR Open, TCHAR Close)
	{
		FString Out;
		bool bInside = false;
		for (const TCHAR C : In)
		{
			if (!bInside && C == Open) { bInside = true; Out.AppendChar(TEXT(' ')); continue; }
			if (bInside && C == Close) { bInside = false; continue; }
			if (!bInside) Out.AppendChar(C);
		}
		return Out;
	}

	bool IsQuote(TCHAR C) { return C == TEXT('"') || C == 0x201C || C == 0x201D; }

	TArray<FString> SplitSentences(const FString& Text)
	{
		TArray<FString> Out;
		FString Cur;
		for (int32 i = 0; i < Text.Len(); ++i)
		{
			const TCHAR C = Text[i];
			Cur.AppendChar(C);
			if (C == TEXT('.') || C == TEXT('!') || C == TEXT('?') || C == 0x2026)
			{
				// Keep runs like "..." and a closing quote with the sentence.
				while (i + 1 < Text.Len() && (Text[i + 1] == TEXT('.') || Text[i + 1] == TEXT('!') || Text[i + 1] == TEXT('?') || IsQuote(Text[i + 1]) || Text[i + 1] == TEXT('\'')))
				{
					Cur.AppendChar(Text[++i]);
				}
				Cur.TrimStartAndEndInline();
				if (!Cur.IsEmpty()) Out.Add(Cur);
				Cur.Reset();
			}
		}
		Cur.TrimStartAndEndInline();
		if (!Cur.IsEmpty()) Out.Add(Cur);
		return Out;
	}

	/** Words of 4+ letters in an evidence name ("Brass button" -> brass, button). */
	TArray<FString> NameWords(const FString& Name)
	{
		TArray<FString> Out;
		FString W;
		for (const TCHAR C : Name.ToLower() + TEXT(" "))
		{
			if (C >= TEXT('a') && C <= TEXT('z')) { W.AppendChar(C); continue; }
			if (W.Len() >= 4) Out.Add(W);
			W.Reset();
		}
		return Out;
	}

	bool CitesSomething(const FString& Text, const FLlmCase& Case)
	{
		if (Matches(Rules().CitesTestimony, Text)) return true;
		for (const FLlmEvidence& E : Case.Evidence)
		{
			for (const FString& W : NameWords(E.Name))
			{
				if (Matches(TEXT("\\b") + W + TEXT("s?\\b"), Text)) return true;
			}
		}
		return false;
	}
}

FString LlmTextRules::ClassifyTactic(const FString& Line, const FLlmCase& Case)
{
	const FTextRules& R = Rules();
	if (!R.bLoaded) return FString();
	const FString Text = CollapseSpaces(Line);
	auto Has = [&](const FString& Tactic) { return Case.Tactics.ContainsByPredicate([&](const TPair<FString, FString>& T) { return T.Key == Tactic; }); };

	for (const TPair<FString, FString>& Rule : R.Tactics)
	{
		if (Has(Rule.Key) && Matches(Rule.Value, Text)) return Rule.Key;
	}
	// Short shouted orders ("Talk!", "Answer me!") count as threats ("shouting at you").
	if (Has(TEXT("threaten")) && Matches(R.Shout, Text)) return TEXT("threaten");
	if (Has(TEXT("question")))
	{
		const TArray<FString> Sentences = SplitSentences(Text);
		const FString Last = Sentences.Num() ? Sentences.Last() : Text;
		if (Matches(R.Question, Last) && !CitesSomething(Text, Case)) return TEXT("question");
	}
	return FString();
}

FString LlmTextRules::CleanReply(const FString& Reply, const FString& SuspectName)
{
	FString T = Reply;
	// <think> ... </think> (or an unclosed one)
	for (int32 Start; (Start = T.Find(TEXT("<think>"), ESearchCase::IgnoreCase)) != INDEX_NONE;)
	{
		const int32 End = T.Find(TEXT("</think>"), ESearchCase::IgnoreCase, ESearchDir::FromStart, Start);
		T = T.Left(Start) + TEXT(" ") + (End == INDEX_NONE ? FString() : T.Mid(End + 8));
	}
	T = RemoveSpans(T, TEXT('*'), TEXT('*'));
	T = RemoveSpans(T, TEXT('['), TEXT(']'));
	T = RemoveSpans(T, TEXT('('), TEXT(')'));
	T = CollapseSpaces(T);

	FString First, Rest;
	if (!SuspectName.Split(TEXT(" "), &First, &Rest)) First = SuspectName;
	for (const FString& Label : { SuspectName, First })
	{
		if (Label.IsEmpty() || !T.StartsWith(Label, ESearchCase::IgnoreCase)) continue;
		FString After = T.Mid(Label.Len()).TrimStart();
		if (After.StartsWith(TEXT(":")) || After.StartsWith(TEXT("-")) || After.StartsWith(TEXT("\x2013")))
		{
			T = After.Mid(1).TrimStart();
			break;
		}
	}

	// Prose with quoted dialogue: keep only the dialogue.
	{
		TArray<FString> Quotes;
		FString Outside, Cur;
		bool bIn = false;
		for (const TCHAR C : T)
		{
			if (IsQuote(C))
			{
				if (bIn && CountLetters(Cur) >= 2) Quotes.Add(Cur.TrimStartAndEnd());
				bIn = !bIn;
				Cur.Reset();
				continue;
			}
			if (bIn) Cur.AppendChar(C); else Outside.AppendChar(C);
		}
		TArray<FString> OutsideWords;
		CollapseSpaces(Outside).ParseIntoArray(OutsideWords, TEXT(" "));
		int32 RealWords = 0;
		for (const FString& W : OutsideWords) if (CountLetters(W) > 0) ++RealWords;
		if (Quotes.Num() > 0 && RealWords >= 2) T = FString::Join(Quotes, TEXT(" "));
	}

	// Narrated actions: "Vera smiles." / "I laugh nervously."
	{
		TArray<FString> Kept;
		for (const FString& S : SplitSentences(T))
		{
			const bool bSelfThird = !First.IsEmpty() && S.StartsWith(First + TEXT(" "), ESearchCase::CaseSensitive) &&S.Len() > First.Len() + 1 && FChar::IsLower(S[First.Len() + 1]);
			if (!bSelfThird && !Matches(Rules().GuardAction, S)) Kept.Add(S);
		}
		if (Kept.Num() > 0) T = FString::Join(Kept, TEXT(" "));
	}

	T.TrimStartAndEndInline();
	if (T.Len() >= 2 && IsQuote(T[0]) && IsQuote(T[T.Len() - 1]))
	{
		const FString Inner = T.Mid(1, T.Len() - 2);
		bool bInnerQuote = false;
		for (const TCHAR C : Inner) bInnerQuote |= IsQuote(C);
		if (!bInnerQuote) T = Inner.TrimStartAndEnd();
	}
	for (const TCHAR* P : { TEXT(" ,"), TEXT(" ."), TEXT(" !"), TEXT(" ?") })
	{
		T.ReplaceInline(P, P + 1);
	}
	return T;
}

TArray<FString> LlmTextRules::ReplyIssues(const FString& Reply)
{
	TArray<FString> Out;
	if (CountLetters(Reply) < 2) Out.Add(TEXT("empty"));
	if (Matches(Rules().GuardMeta, Reply)) Out.Add(TEXT("meta"));
	if (Matches(Rules().GuardEcho, Reply)) Out.Add(TEXT("echo"));
	return Out;
}

namespace
{
	const TSet<FString>& Filler()
	{
		static const TSet<FString> F = [] {
			TSet<FString> S;
			TArray<FString> Words;
			FString(TEXT("about after again also anything because been before being between could does doing done during "
				"each either every from going have having into just like made make many more most much must never only other over "
				"said same should some such than that their them then there these they this those through very want what when where "
				"which while whom whose will with without would your yours")).ParseIntoArrayWS(Words);
			for (const FString& W : Words) S.Add(W);
			return S;
		}();
		return F;
	}

	FString Stem(const FString& Word)
	{
		const FString W = Word.ToLower();
		for (const TCHAR* Suf : { TEXT("ings"), TEXT("ing"), TEXT("ies"), TEXT("ied"), TEXT("ed"), TEXT("es"), TEXT("s") })
		{
			const int32 L = FCString::Strlen(Suf);
			if (W.EndsWith(Suf) && W.Len() - L >= 4) return W.LeftChop(L);
		}
		return W;
	}

	/** Words made of letters and apostrophes, like Python's [A-Za-z']+. */
	TArray<FString> Words(const FString& Text)
	{
		TArray<FString> Out;
		FString W;
		for (const TCHAR C : Text + TEXT(" "))
		{
			if ((C >= TEXT('a') && C <= TEXT('z')) || (C >= TEXT('A') && C <= TEXT('Z')) || C == TEXT('\'')) { W.AppendChar(C); continue; }
			if (!W.IsEmpty()) Out.Add(W);
			W.Reset();
		}
		return Out;
	}

	bool IsOpenQuote(TCHAR C) { return C == TEXT('\'') || C == TEXT('"') || C == 0x2018 || C == 0x201C; }
	bool IsCloseQuote(TCHAR C) { return C == TEXT('\'') || C == TEXT('"') || C == 0x2019 || C == 0x201D; }

	/** Quoted bits of a topic ('R.'): open after a space or at the start (so "Augie's" is not a quote),
	 *  close before a space, punctuation or the end. */
	TArray<FString> QuotedTokens(const FString& Topic)
	{
		TArray<FString> Out;
		for (int32 i = 0; i < Topic.Len(); ++i)
		{
			if (!IsOpenQuote(Topic[i]) || (i > 0 && !FChar::IsWhitespace(Topic[i - 1]))) continue;
			for (int32 j = i + 1; j < Topic.Len() && j - i <= 41; ++j)
			{
				if (!IsCloseQuote(Topic[j])) continue;
				const bool bEnds = j + 1 >= Topic.Len() || FChar::IsWhitespace(Topic[j + 1]) || FString(TEXT(",.;:!?)")).Contains(FString(1, &Topic[j + 1]));
				if (bEnds && j > i + 1)
				{
					const FString Q = Topic.Mid(i + 1, j - i - 1).TrimStartAndEnd();
					if (!Q.IsEmpty()) Out.Add(Q);
					i = j;
				}
				break;
			}
		}
		return Out;
	}
}

FString LlmTextRules::TouchedSecret(const FString& Line, const TArray<FLlmAvailableSecret>& Available)
{
	// Secrets are admitted "if asked about <topic>": statements and accusations never force a reveal.
	const FString Trimmed = Line.TrimStart();
	const bool bAsking = Line.Contains(TEXT("?")) || Trimmed.StartsWith(TEXT("tell me")) || Trimmed.StartsWith(TEXT("explain"))
		|| Trimmed.StartsWith(TEXT("describe")) || Trimmed.StartsWith(TEXT("walk me through"));
	if (!bAsking) return FString();

	TSet<FString> LineStems;
	for (const FString& W : Words(Line)) if (W.Len() >= 3) LineStems.Add(Stem(W));
	const FString LowerLine = Line.ToLower();

	FString Best;
	int32 BestN = 0;
	for (const FLlmAvailableSecret& A : Available)
	{
		TSet<FString> TopicStems;
		for (const FString& W : Words(A.Topic))
		{
			if (W.Len() < 4 || FChar::IsUpper(W[0]) || Filler().Contains(W.ToLower())) continue;
			TopicStems.Add(Stem(W));
		}
		int32 N = 0;
		for (const FString& S : TopicStems) if (LineStems.Contains(S)) ++N;
		for (const FString& Q : QuotedTokens(A.Topic)) if (LowerLine.Contains(Q.ToLower())) ++N;
		if (N > BestN)
		{
			Best = A.Id;
			BestN = N;
		}
	}
	return Best;
}

namespace
{
	TSet<FString> LowerWords(const FString& S)
	{
		TSet<FString> Out;
		// Like Python's re.findall("[a-z]+"): apostrophes split words ("augie's" -> augie, s).
		for (const FString& W : Words(S.ToLower().Replace(TEXT("'"), TEXT(" ")))) Out.Add(W);
		return Out;
	}
}

bool LlmTextRules::EvidenceMentioned(const FString& Line, const FLlmEvidence& Evidence)
{
	TSet<FString> Keys;
	for (const FString& W : NameWords(Evidence.Name)) Keys.Add(W);
	TArray<FString> IdParts;
	Evidence.Id.ToLower().ParseIntoArray(IdParts, TEXT("_"));
	for (const FString& P : IdParts) if (P.Len() >= 3 && P != TEXT("the") && P != TEXT("and")) Keys.Add(P);
	const TSet<FString> LineWords = LowerWords(Line);
	for (const FString& K : Keys)
	{
		if (LineWords.Contains(K) || LineWords.Contains(K + TEXT("s")) || (K.EndsWith(TEXT("s")) && LineWords.Contains(K.LeftChop(1)))) return true;
	}
	return false;
}

bool LlmTextRules::SuspectMentioned(const FString& Line, const FLlmSuspectDef& Suspect)
{
	const TSet<FString> LineWords = LowerWords(Line);
	TArray<FString> Parts;
	Suspect.Name.ParseIntoArrayWS(Parts);
	for (const FString& P : Parts) if (P.Len() >= 3 && LineWords.Contains(P.ToLower())) return true;
	return false;
}

FString LlmTextRules::SerializeReply(const FLlmParsedReply& P)
{
	FString Out;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> W = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	W->WriteObjectStart();
	W->WriteValue(TEXT("t"), P.Tactic);
	W->WriteValue(TEXT("c"), P.ClaimedEvidence);
	W->WriteValue(TEXT("g"), P.RepeatedSuspect);
	W->WriteValue(TEXT("f"), P.Secret);
	W->WriteValue(TEXT("e"), P.Emotion);
	W->WriteValue(TEXT("r"), P.Reply);
	W->WriteObjectEnd();
	W->Close();
	return Out;
}
