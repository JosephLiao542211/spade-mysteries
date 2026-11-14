#include "Llm/LlmCaseData.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	bool ReadJsonFile(const FString& Path, TSharedPtr<FJsonObject>& Out, FString& OutError)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			OutError = FString::Printf(TEXT("Cannot read %s"), *Path);
			return false;
		}
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Out) || !Out.IsValid())
		{
			OutError = FString::Printf(TEXT("Invalid JSON in %s"), *Path);
			return false;
		}
		return true;
	}

	FString Str(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		FString V;
		if (O.IsValid()) O->TryGetStringField(Key, V);
		return V;
	}

	TArray<FString> StrArray(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		TArray<FString> V;
		if (O.IsValid()) O->TryGetStringArrayField(Key, V);
		return V;
	}

	TSharedPtr<FJsonObject> Obj(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		const TSharedPtr<FJsonObject>* V = nullptr;
		return (O.IsValid() && O->TryGetObjectField(Key, V)) ? *V : nullptr;
	}

	const TArray<TSharedPtr<FJsonValue>>& Arr(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		static const TArray<TSharedPtr<FJsonValue>> Empty;
		const TArray<TSharedPtr<FJsonValue>>* V = nullptr;
		return (O.IsValid() && O->TryGetArrayField(Key, V)) ? *V : Empty;
	}

	FLlmEffects Effects(const TSharedPtr<FJsonObject>& O)
	{
		FLlmEffects E;
		if (O.IsValid())
		{
			for (const auto& KV : O->Values) E.Add(KV.Key, static_cast<float>(KV.Value->AsNumber()));
		}
		return E;
	}

	TMap<FString, FLlmEffects> EffectsMap(const TSharedPtr<FJsonObject>& O)
	{
		TMap<FString, FLlmEffects> M;
		if (O.IsValid())
		{
			for (const auto& KV : O->Values) M.Add(KV.Key, Effects(KV.Value->AsObject()));
		}
		return M;
	}

	FLlmMeterWords Words(const TArray<TSharedPtr<FJsonValue>>& A)
	{
		FLlmMeterWords W;
		for (const TSharedPtr<FJsonValue>& V : A)
		{
			const TArray<TSharedPtr<FJsonValue>>& Pair = V->AsArray();
			if (Pair.Num() == 2) W.Emplace(static_cast<float>(Pair[0]->AsNumber()), Pair[1]->AsString());
		}
		return W;
	}

	FLlmCondition Condition(const TSharedPtr<FJsonObject>& O)
	{
		FLlmCondition C;
		if (!O.IsValid()) return C;
		for (const auto& KV : O->Values)
		{
			if (KV.Key.EndsWith(TEXT("_gte"))) C.Gte.Add(KV.Key.LeftChop(4), static_cast<float>(KV.Value->AsNumber()));
			else if (KV.Key.EndsWith(TEXT("_lt"))) C.Lt.Add(KV.Key.LeftChop(3), static_cast<float>(KV.Value->AsNumber()));
			else if (KV.Key == TEXT("evidence_seen")) for (const auto& V : KV.Value->AsArray()) C.EvidenceSeen.Add(V->AsString());
			else if (KV.Key == TEXT("revealed")) for (const auto& V : KV.Value->AsArray()) C.Revealed.Add(V->AsString());
		}
		return C;
	}

	bool LoadSuspect(const FString& Path, FLlmSuspectDef& S, FString& OutError)
	{
		TSharedPtr<FJsonObject> J;
		if (!ReadJsonFile(Path, J, OutError)) return false;

		S.Id = Str(J, TEXT("id"));
		S.Name = Str(J, TEXT("name"));
		S.Character = Str(J, TEXT("character"));
		J->TryGetNumberField(TEXT("slot"), S.Slot);
		J->TryGetNumberField(TEXT("fear_dampens_trust_above"), S.FearDampensTrustAbove);
		J->TryGetNumberField(TEXT("gossip_penalty"), S.GossipPenalty);
		S.BluffNote = Str(J, TEXT("bluff_note"));

		for (const auto& V : Arr(J, TEXT("claims")))
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			S.Claims.Add({ Str(O, TEXT("id")), Str(O, TEXT("text")) });
		}
		if (const TSharedPtr<FJsonObject> Meters = Obj(J, TEXT("meters")))
		{
			for (const auto& KV : Meters->Values)
			{
				const TSharedPtr<FJsonObject> M = KV.Value->AsObject();
				FLlmMeterDef Def;
				Def.Name = KV.Key;
				Def.Start = static_cast<float>(M->GetNumberField(TEXT("start")));
				Def.Words = Words(Arr(M, TEXT("words")));
				S.Meters.Add(Def);
			}
		}
		S.TacticEffects = EffectsMap(Obj(J, TEXT("tactic_effects")));
		S.EvidenceEffects = EffectsMap(Obj(J, TEXT("evidence_effects")));
		S.BluffEffects = Effects(Obj(J, TEXT("bluff_effects")));
		if (const TSharedPtr<FJsonObject> G = Obj(J, TEXT("gossip_heard")))
		{
			S.GossipHeardSelf = Effects(Obj(G, TEXT("self")));
			S.GossipHeardSource = Effects(Obj(G, TEXT("source")));
		}
		for (const auto& V : Arr(J, TEXT("rules")))
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FLlmRule R;
			R.bWhile = Str(O, TEXT("mode")) != TEXT("trigger");
			R.When = Condition(Obj(O, TEXT("when")));
			R.Note = Str(O, TEXT("note"));
			O->TryGetBoolField(TEXT("block_secrets"), R.bBlockSecrets);
			O->TryGetNumberField(TEXT("shutdown_turns"), R.ShutdownTurns);
			R.AfterShutdownSet = Effects(Obj(O, TEXT("after_shutdown_set")));
			O->TryGetBoolField(TEXT("end_interview"), R.bEndInterview);
			R.Others = Effects(Obj(O, TEXT("others")));
			S.Rules.Add(R);
		}
		for (const auto& V : Arr(J, TEXT("secrets")))
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FLlmSecret Sec;
			Sec.Id = Str(O, TEXT("id"));
			Sec.Topic = Str(O, TEXT("topic"));
			Sec.Text = Str(O, TEXT("text"));
			Sec.GrantsEvidence = Str(O, TEXT("grants_evidence"));
			for (const auto& RV : Arr(O, TEXT("routes")))
			{
				const TSharedPtr<FJsonObject> RO = RV->AsObject();
				Sec.Routes.Add({ Condition(Obj(RO, TEXT("when"))), Str(RO, TEXT("text")) });
			}
			S.Secrets.Add(Sec);
		}
		return true;
	}
}

void FLlmCaseState::Init(const FLlmCase& Case)
{
	Suspects.Reset();
	Inventory.Reset();
	for (const FLlmEvidence& E : Case.Evidence)
	{
		if (E.bFoundAtStart) Inventory.Add(E.Id);
	}
	for (const FLlmSuspectDef& S : Case.Suspects)
	{
		FLlmSuspectState& St = Suspects.Add(S.Id);
		for (const FLlmMeterDef& M : S.Meters) St.Meters.Add(M.Name, M.Start);
	}
}

bool LlmCaseData::LoadCase(const FString& CaseDir, FLlmCase& Out, FString& OutError)
{
	TSharedPtr<FJsonObject> J;
	if (!ReadJsonFile(CaseDir / TEXT("case.json"), J, OutError)) return false;

	Out = FLlmCase();
	Out.Id = Str(J, TEXT("id"));
	Out.Title = Str(J, TEXT("title"));
	Out.Setting = Str(J, TEXT("setting"));
	Out.Rules = StrArray(J, TEXT("rules"));
	Out.Emotions = StrArray(J, TEXT("emotions"));
	if (const TSharedPtr<FJsonObject> T = Obj(J, TEXT("tactics")))
	{
		for (const auto& KV : T->Values) Out.Tactics.Emplace(KV.Key, KV.Value->AsString());
	}
	if (const TSharedPtr<FJsonObject> MW = Obj(J, TEXT("meter_words")))
	{
		for (const auto& KV : MW->Values) Out.MeterWords.Add(KV.Key, Words(KV.Value->AsArray()));
	}
	for (const auto& V : Arr(J, TEXT("evidence")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FLlmEvidence E;
		E.Id = Str(O, TEXT("id"));
		E.Name = Str(O, TEXT("name"));
		E.Description = Str(O, TEXT("description"));
		O->TryGetBoolField(TEXT("found_at_start"), E.bFoundAtStart);
		Out.Evidence.Add(E);
	}
	for (const FString& File : StrArray(J, TEXT("suspects")))
	{
		FLlmSuspectDef S;
		if (!LoadSuspect(CaseDir / File, S, OutError)) return false;
		Out.Suspects.Add(MoveTemp(S));
	}
	if (const TSharedPtr<FJsonObject> A = Obj(J, TEXT("accusation")))
	{
		FLlmAccusation& Acc = Out.Accusation;
		Acc.Culprit = Str(A, TEXT("culprit"));
		Acc.ConfessionSecrets = StrArray(A, TEXT("confession_secrets"));
		Acc.MotiveEvidence = StrArray(A, TEXT("motive_evidence"));
		Acc.PhysicalEvidence = StrArray(A, TEXT("physical_evidence"));
		Acc.WrongText = Str(A, TEXT("wrong_text"));
		Acc.WinText = Str(A, TEXT("win_text"));
		for (const auto& V : Arr(A, TEXT("testimony")))
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FLlmTestimonyReq T;
			T.Suspect = Str(O, TEXT("suspect"));
			T.Secret = Str(O, TEXT("secret"));
			O->TryGetNumberField(TEXT("min_trust"), T.MinTrust);
			Acc.Testimony.Add(T);
		}
	}
	return true;
}
