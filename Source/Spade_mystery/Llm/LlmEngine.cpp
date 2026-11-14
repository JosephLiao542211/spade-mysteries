#include "Llm/LlmEngine.h"

namespace
{
	TSharedRef<FJsonObject> EnumProp(const TArray<FString>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const FString& V : Values) Arr.Add(MakeShared<FJsonValueString>(V));
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetArrayField(TEXT("enum"), Arr);
		return O;
	}

	FString Join(const TArray<FString>& Parts, const TCHAR* Sep)
	{
		return FString::Join(Parts, Sep);
	}
}

bool LlmEngine::CondOk(const FLlmCondition& Cond, const FLlmSuspectState& St)
{
	for (const auto& KV : Cond.Gte)
	{
		if (St.Meter(KV.Key) < KV.Value) return false;
	}
	for (const auto& KV : Cond.Lt)
	{
		if (St.Meter(KV.Key) >= KV.Value) return false;
	}
	for (const FString& E : Cond.EvidenceSeen)
	{
		if (!St.EvidenceSeen.Contains(E)) return false;
	}
	for (const FString& R : Cond.Revealed)
	{
		if (!St.Revealed.Contains(R)) return false;
	}
	return true;
}

void LlmEngine::AddMeter(const FLlmCase& Case, FLlmCaseState& State, const FString& Sid, const FString& Meter, float Delta)
{
	FLlmSuspectState* St = State.Suspects.Find(Sid);
	const FLlmSuspectDef* Def = Case.FindSuspect(Sid);
	if (!St || !Def) return;
	float* V = St->Meters.Find(Meter);
	if (!V) return;
	if (Meter == TEXT("trust") && Delta > 0.f && St->Meter(TEXT("fear")) >= Def->FearDampensTrustAbove)
	{
		Delta *= 0.5f; // frightened people don't confide
	}
	*V = FMath::Clamp(*V + Delta, 0.f, 100.f);
}

void LlmEngine::ApplyEffects(const FLlmCase& Case, FLlmCaseState& State, const FString& Sid, const FLlmEffects* Effects)
{
	if (!Effects) return;
	for (const auto& KV : *Effects) AddMeter(Case, State, Sid, KV.Key, KV.Value);
}

FString LlmEngine::WordsFor(const FLlmMeterWords& Bands, float Value)
{
	if (Bands.Num() == 0) return FString();
	FString Text = Bands[0].Value;
	for (const auto& B : Bands)
	{
		if (Value >= B.Key) Text = B.Value;
	}
	return Text;
}

TArray<const FLlmRule*> LlmEngine::WhileRules(const FLlmSuspectDef& Def, const FLlmSuspectState& St)
{
	TArray<const FLlmRule*> Out;
	for (const FLlmRule& R : Def.Rules)
	{
		if (R.bWhile && CondOk(R.When, St)) Out.Add(&R);
	}
	return Out;
}

bool LlmEngine::SecretsBlocked(const FLlmSuspectDef& Def, const FLlmSuspectState& St)
{
	if (St.ShutdownLeft > 0 || St.bClosed) return true;
	for (const FLlmRule* R : WhileRules(Def, St))
	{
		if (R->bBlockSecrets) return true;
	}
	return false;
}

TArray<FLlmAvailableSecret> LlmEngine::AvailableSecrets(const FLlmSuspectDef& Def, const FLlmSuspectState& St)
{
	TArray<FLlmAvailableSecret> Out;
	if (SecretsBlocked(Def, St)) return Out;
	for (const FLlmSecret& Sec : Def.Secrets)
	{
		if (St.Revealed.Contains(Sec.Id)) continue;
		for (const FLlmSecretRoute& Route : Sec.Routes)
		{
			if (CondOk(Route.When, St))
			{
				Out.Add({ Sec.Id, Route.Text.IsEmpty() ? Sec.Text : Route.Text, Sec.Topic });
				break;
			}
		}
	}
	return Out;
}

FString LlmEngine::StaticSystem(const FLlmCase& Case, const FString& Sid)
{
	const FLlmSuspectDef* S = Case.FindSuspect(Sid);
	if (!S) return FString();

	TArray<FString> Rules, Claims, Tactics, Evidence, Others;
	for (const FString& R : Case.Rules) Rules.Add(TEXT("- ") + R);
	for (const FLlmClaim& C : S->Claims) Claims.Add(TEXT("- ") + C.Text);
	for (const auto& T : Case.Tactics) Tactics.Add(FString::Printf(TEXT("%s (%s)"), *T.Key, *T.Value));
	for (const FLlmEvidence& E : Case.Evidence) Evidence.Add(FString::Printf(TEXT("%s (%s)"), *E.Id, *E.Name.ToLower()));
	for (const FLlmSuspectDef& O : Case.Suspects)
	{
		if (O.Id != Sid) Others.Add(FString::Printf(TEXT("%s (%s)"), *O.Id, *O.Name));
	}

	return FString::Printf(TEXT("SETTING: %s\n\nWHO YOU ARE: %s\n\nWHAT YOU HAVE TOLD THE POLICE:\n%s\n\nRULES:\n%s\n\n"), *Case.Setting, *S->Character, *Join(Claims, TEXT("\n")), *Join(Rules, TEXT("\n")))
		+ TEXT("OUTPUT: Answer only with JSON.\n")
		+ FString::Printf(TEXT("t = the detective's tactic in their LAST line, one of: %s.\n"), *Join(Tactics, TEXT("; ")))
		+ FString::Printf(TEXT("c = the evidence the detective claims to HAVE or shows you in their LAST line, one of: %s; "), *Join(Evidence, TEXT("; ")))
		+ TEXT("or none. Only asking about something is not claiming to have it: then c is none. ")
		+ TEXT("These labels are only for classifying; never bring these items up yourself.\n")
		+ FString::Printf(TEXT("g = whose words the detective is repeating to you in their LAST line, one of: %s; or none.\n"), *Join(Others, TEXT("; ")))
		+ TEXT("f = the id of a secret from SECRETS YOU WILL ADMIT whose topic the detective's line touches, ")
		+ TEXT("otherwise none; if f is not none, r must say that secret. e = your emotion. r = your spoken reply.");
}

FString LlmEngine::StateNote(const FLlmCase& Case, const FLlmCaseState& State, const FString& Sid)
{
	const FLlmSuspectDef* Def = Case.FindSuspect(Sid);
	const FLlmSuspectState* St = State.Suspects.Find(Sid);
	if (!Def || !St) return FString();

	TArray<FString> Lines = { TEXT("[STATE]") };
	for (const FLlmMeterDef& M : Def->Meters)
	{
		const FLlmMeterWords* Bands = M.Words.Num() > 0 ? &M.Words : Case.MeterWords.Find(M.Name);
		if (Bands && Bands->Num() > 0) Lines.Add(WordsFor(*Bands, St->Meter(M.Name)));
	}
	if (St->bCaughtBluff)
	{
		Lines.Add(Def->BluffNote.IsEmpty() ? FString(TEXT("The detective just claimed to have something they don't have.")) : Def->BluffNote);
	}
	if (St->ShutdownLeft > 0) Lines.Add(St->ShutdownNote);
	for (const FLlmRule* R : WhileRules(*Def, *St)) Lines.Add(R->Note);

	const TArray<FLlmAvailableSecret> Avail = AvailableSecrets(*Def, *St);
	if (Avail.Num() > 0)
	{
		Lines.Add(TEXT("SECRETS YOU WILL ADMIT: you have decided to give these up. If the detective's line touches the topic, set f to the id and say it in r, in your own words, keeping the facts exact:"));
		for (const FLlmAvailableSecret& A : Avail) Lines.Add(FString::Printf(TEXT("(%s) If asked about %s: %s"), *A.Id, *A.Topic, *A.Text));
	}
	else
	{
		Lines.Add(TEXT("SECRETS YOU WILL ADMIT: none. Keep to what you have told the police."));
	}
	if (St->Revealed.Num() > 0)
	{
		TArray<FString> Texts;
		St->Revealed.GenerateValueArray(Texts);
		Lines.Add(TEXT("You have already admitted: ") + Join(Texts, TEXT(" ")));
	}
	return Join(Lines, TEXT("\n"));
}

TSharedRef<FJsonObject> LlmEngine::Schema(const FLlmCase& Case, const FLlmCaseState& State, const FString& Sid)
{
	TArray<FString> Tactics, Evidence, Others, Secrets = { None() };
	for (const auto& T : Case.Tactics) Tactics.Add(T.Key);
	for (const FLlmEvidence& E : Case.Evidence) Evidence.Add(E.Id);
	Evidence.Add(None());
	for (const FLlmSuspectDef& O : Case.Suspects)
	{
		if (O.Id != Sid) Others.Add(O.Id);
	}
	Others.Add(None());
	const FLlmSuspectDef* Def = Case.FindSuspect(Sid);
	const FLlmSuspectState* St = State.Suspects.Find(Sid);
	if (Def && St)
	{
		for (const FLlmAvailableSecret& A : AvailableSecrets(*Def, *St)) Secrets.Add(A.Id);
	}

	TSharedRef<FJsonObject> Reply = MakeShared<FJsonObject>();
	Reply->SetStringField(TEXT("type"), TEXT("string"));
	Reply->SetNumberField(TEXT("maxLength"), 300);

	// Order matters: classification first, then the decision to admit (f), then the reply that carries it.
	// With f after r, E4B never admitted anything in the harness; the clue card covers wording drift in r.
	TSharedRef<FJsonObject> Props = MakeShared<FJsonObject>();
	Props->SetObjectField(TEXT("t"), EnumProp(Tactics));
	Props->SetObjectField(TEXT("c"), EnumProp(Evidence));
	Props->SetObjectField(TEXT("g"), EnumProp(Others));
	Props->SetObjectField(TEXT("f"), EnumProp(Secrets));
	Props->SetObjectField(TEXT("e"), EnumProp(Case.Emotions));
	Props->SetObjectField(TEXT("r"), Reply);

	TArray<TSharedPtr<FJsonValue>> Required;
	for (const TCHAR* K : { TEXT("t"), TEXT("c"), TEXT("g"), TEXT("f"), TEXT("e"), TEXT("r") }) Required.Add(MakeShared<FJsonValueString>(K));

	TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
	Out->SetStringField(TEXT("type"), TEXT("object"));
	Out->SetObjectField(TEXT("properties"), Props);
	Out->SetArrayField(TEXT("required"), Required);
	return Out;
}

FString LlmEngine::DetectiveMessage(const FString& Line)
{
	return TEXT("[DETECTIVE] ") + Line;
}

FString LlmEngine::EvidenceMessage(const FLlmCase& Case, const FString& EvidenceId)
{
	const FLlmEvidence* E = Case.FindEvidence(EvidenceId);
	return FString::Printf(TEXT("[EVIDENCE] The detective places this on the table: %s."), E ? *E->Description : *EvidenceId);
}

TArray<FLlmMessage> LlmEngine::BuildMessages(const FLlmCase& Case, const FLlmCaseState& State, const FString& Sid, const FString& LastUser)
{
	TArray<FLlmMessage> Msgs;
	Msgs.Add({ TEXT("system"), StaticSystem(Case, Sid) });
	if (const FLlmSuspectState* St = State.Suspects.Find(Sid))
	{
		for (const auto& Turn : St->History)
		{
			Msgs.Add({ TEXT("user"), Turn.Key });
			Msgs.Add({ TEXT("assistant"), Turn.Value });
		}
	}
	Msgs.Add({ TEXT("user"), StateNote(Case, State, Sid) + TEXT("\n\n") + LastUser });
	return Msgs;
}

FLlmTurnEvent LlmEngine::ResolveTurn(const FLlmCase& Case, FLlmCaseState& State, const FString& Sid, const FString& UserMsg,
	const FLlmParsedReply* Parsed, const FString& Raw, const TMap<FString, FString>& Allowed, const FString& PresentedEvidence)
{
	FLlmTurnEvent Ev;
	const FLlmSuspectDef* Def = Case.FindSuspect(Sid);
	FLlmSuspectState* St = State.Suspects.Find(Sid);
	if (!Def || !St) return Ev;

	const TMap<FString, float> Before = St->Meters;
	const bool bWasShutdown = St->ShutdownLeft > 0;
	St->bCaughtBluff = false;

	if (!PresentedEvidence.IsEmpty())
	{
		if (!St->EvidenceSeen.Contains(PresentedEvidence))
		{
			St->EvidenceSeen.Add(PresentedEvidence);
			ApplyEffects(Case, State, Sid, Def->EvidenceEffects.Find(PresentedEvidence));
		}
	}
	else if (Parsed)
	{
		ApplyEffects(Case, State, Sid, Def->TacticEffects.Find(Parsed->Tactic));

		const FString& C = Parsed->ClaimedEvidence;
		if (!C.IsEmpty() && C != None())
		{
			if (State.Inventory.Contains(C))
			{
				if (!St->EvidenceSeen.Contains(C))
				{
					St->EvidenceSeen.Add(C);
					ApplyEffects(Case, State, Sid, Def->EvidenceEffects.Find(C));
				}
			}
			else
			{
				Ev.bBluff = true;
				St->bCaughtBluff = !Def->BluffNote.IsEmpty(); // only suspects who see through bluffs notice
				ApplyEffects(Case, State, Sid, &Def->BluffEffects);
			}
		}

		const FString& G = Parsed->RepeatedSuspect;
		const FLlmSuspectDef* Source = Case.FindSuspect(G);
		if (!G.IsEmpty() && G != None() && G != Sid && Source)
		{
			Ev.Gossip = G;
			AddMeter(Case, State, G, TEXT("trust"), -Source->GossipPenalty);
			ApplyEffects(Case, State, Sid, &Def->GossipHeardSelf);
			ApplyEffects(Case, State, G, &Def->GossipHeardSource);
		}
	}

	if (Parsed)
	{
		const FString* Text = Allowed.Find(Parsed->Secret);
		if (Parsed->Secret != None() && Text)
		{
			St->Revealed.Add(Parsed->Secret, *Text);
			Ev.Revealed = Parsed->Secret;
			Ev.RevealedText = *Text;
			if (const FLlmSecret* Sec = Def->FindSecret(Parsed->Secret))
			{
				if (!Sec->GrantsEvidence.IsEmpty())
				{
					State.Inventory.Add(Sec->GrantsEvidence);
					Ev.Granted = Sec->GrantsEvidence;
				}
			}
		}
	}

	if (bWasShutdown)
	{
		St->ShutdownLeft--;
		if (St->ShutdownLeft == 0)
		{
			for (const auto& KV : St->AfterShutdownSet) St->Meters.Add(KV.Key, KV.Value);
		}
	}

	for (int32 i = 0; i < Def->Rules.Num(); ++i)
	{
		const FLlmRule& R = Def->Rules[i];
		if (R.bWhile) continue;
		if (!CondOk(R.When, *St))
		{
			St->Fired.Remove(i);
			continue;
		}
		if (St->Fired.Contains(i)) continue;
		St->Fired.Add(i);
		Ev.Triggers.Add(R.Note);
		if (R.ShutdownTurns > 0)
		{
			St->ShutdownLeft = R.ShutdownTurns;
			St->ShutdownNote = R.Note;
			St->AfterShutdownSet = R.AfterShutdownSet;
		}
		if (R.bEndInterview) St->bClosed = true;
		for (const FLlmSuspectDef& O : Case.Suspects)
		{
			if (O.Id != Sid) ApplyEffects(Case, State, O.Id, &R.Others);
		}
	}

	St->History.Emplace(UserMsg, Raw);
	for (const auto& KV : St->Meters)
	{
		const float Old = Before.FindRef(KV.Key);
		if (KV.Value != Old) Ev.Deltas.Add(KV.Key, KV.Value - Old);
	}
	return Ev;
}

FLlmAccusationResult LlmEngine::Accuse(const FLlmCase& Case, const FLlmCaseState& State, const FString& Accused)
{
	const FLlmAccusation& Acc = Case.Accusation;
	if (Accused != Acc.Culprit) return { false, Acc.WrongText };

	if (const FLlmSuspectState* C = State.Suspects.Find(Acc.Culprit))
	{
		for (const FString& S : Acc.ConfessionSecrets)
		{
			if (C->Revealed.Contains(S)) return { true, Acc.WinText + TEXT(" (He gave himself away under questioning.)") };
		}
	}

	auto AnyHeld = [&](const TArray<FString>& Ids) { return Ids.ContainsByPredicate([&](const FString& Id) { return State.Inventory.Contains(Id); }); };
	TArray<FString> Missing, Notes;
	if (!AnyHeld(Acc.MotiveEvidence)) Missing.Add(TEXT("a motive"));
	if (!AnyHeld(Acc.PhysicalEvidence)) Missing.Add(TEXT("physical evidence placing him at the scene"));

	bool bWitness = false;
	for (const FLlmTestimonyReq& W : Acc.Testimony)
	{
		const FLlmSuspectState* Ws = State.Suspects.Find(W.Suspect);
		const FLlmSuspectDef* Wd = Case.FindSuspect(W.Suspect);
		if (!Ws || !Wd || !Ws->Revealed.Contains(W.Secret)) continue;
		if (Ws->Meter(TEXT("trust")) >= W.MinTrust) bWitness = true;
		else Notes.Add(FString::Printf(TEXT("%s takes back their statement: they don't trust you enough to testify."), *Wd->Name));
	}
	if (!bWitness) Missing.Add(TEXT("a witness willing to testify he left the table"));

	if (Missing.Num() > 0)
	{
		return { false, TEXT("Not enough to hold him: missing ") + Join(Missing, TEXT(", ")) + TEXT(". ") + Join(Notes, TEXT(" ")) };
	}
	return { true, Acc.WinText };
}

FString LlmEngine::SanitizePlayerLine(const FString& Line, int32 MaxChars)
{
	FString S = Line.Left(MaxChars);
	static const TCHAR* Banned[] = {
		TEXT("<start_of_turn>"), TEXT("<end_of_turn>"), TEXT("<|turn>"), TEXT("<turn|>"), TEXT("<|channel>"), TEXT("<channel|>"),
		TEXT("<|think|>"), TEXT("<bos>"), TEXT("<eos>"), TEXT("<|im_start|>"), TEXT("<|im_end|>"), TEXT("<|endoftext|>"),
		TEXT("[STATE]"), TEXT("[DETECTIVE]"), TEXT("[EVIDENCE]"),
	};
	for (const TCHAR* B : Banned) S.ReplaceInline(B, TEXT(""), ESearchCase::IgnoreCase);
	S.ReplaceInline(TEXT("\r"), TEXT(" "));
	S.ReplaceInline(TEXT("\n"), TEXT(" "));
	return S.TrimStartAndEnd();
}
