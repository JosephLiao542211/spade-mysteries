#include "Llm/LlmPromptBuilder.h"

#include "Misc/FileHelper.h"
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

	TSharedRef<FJsonObject> EnumProp(const TArray<FString>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const FString& V : Values)
		{
			Arr.Add(MakeShared<FJsonValueString>(V));
		}
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetArrayField(TEXT("enum"), Arr);
		return Obj;
	}
}

const TArray<FString>& LlmPrompt::Tactics()
{
	static const TArray<FString> V = { TEXT("small_talk"), TEXT("question"), TEXT("empathy"), TEXT("intimidate"),
		TEXT("bribe"), TEXT("logic"), TEXT("accuse"), TEXT("off_topic_or_meta") };
	return V;
}

const TArray<FString>& LlmPrompt::Emotions()
{
	static const TArray<FString> V = { TEXT("calm"), TEXT("nervous"), TEXT("angry"), TEXT("sad"), TEXT("smug"), TEXT("afraid") };
	return V;
}

bool LlmPrompt::LoadCase(const FString& Path, FLlmCase& Out, FString& OutError)
{
	TSharedPtr<FJsonObject> J;
	if (!ReadJsonFile(Path, J, OutError)) return false;
	Out.Id = J->GetStringField(TEXT("id"));
	Out.Setting = J->GetStringField(TEXT("setting"));
	Out.Output = J->GetStringField(TEXT("output"));
	J->TryGetStringArrayField(TEXT("rules"), Out.Rules);
	return true;
}

bool LlmPrompt::LoadSuspect(const FString& Path, FLlmSuspect& Out, FString& OutError)
{
	TSharedPtr<FJsonObject> J;
	if (!ReadJsonFile(Path, J, OutError)) return false;
	Out.Id = J->GetStringField(TEXT("id"));
	Out.Name = J->GetStringField(TEXT("name"));
	Out.Character = J->GetStringField(TEXT("character"));
	Out.Slot = J->GetIntegerField(TEXT("slot"));

	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (J->TryGetArrayField(TEXT("claims"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			Out.Claims.Add({ O->GetStringField(TEXT("id")), O->GetStringField(TEXT("text")) });
		}
	}
	if (J->TryGetArrayField(TEXT("facts"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			Out.Facts.Add({ O->GetStringField(TEXT("id")), O->GetStringField(TEXT("text")), static_cast<float>(O->GetNumberField(TEXT("pressure_needed"))) });
		}
	}
	const TSharedPtr<FJsonObject>* Mults = nullptr;
	if (J->TryGetObjectField(TEXT("tactic_multipliers"), Mults))
	{
		for (const auto& KV : (*Mults)->Values)
		{
			Out.TacticMultipliers.Add(KV.Key, static_cast<float>(KV.Value->AsNumber()));
		}
	}
	return true;
}

FString LlmPrompt::StaticSystem(const FLlmCase& Case, const FLlmSuspect& Suspect)
{
	FString Rules, Claims;
	for (const FString& R : Case.Rules) Rules += FString::Printf(TEXT("- %s\n"), *R);
	for (const FLlmClaim& C : Suspect.Claims) Claims += FString::Printf(TEXT("- %s\n"), *C.Text);
	Rules.RemoveFromEnd(TEXT("\n"));
	Claims.RemoveFromEnd(TEXT("\n"));

	return FString::Printf(TEXT("SETTING: %s\n\nWHO YOU ARE: %s\n\nYOUR STORY SO FAR:\n%s\n\nRULES:\n%s\n\nOUTPUT: %s"),
		*Case.Setting, *Suspect.Character, *Claims, *Rules, *Case.Output);
}

FString LlmPrompt::PressureWords(float P)
{
	if (P < 20.f) return TEXT("composed and guarded");
	if (P < 40.f) return TEXT("uneasy");
	if (P < 70.f) return TEXT("rattled and slipping");
	return TEXT("close to breaking");
}

TArray<const FLlmFact*> LlmPrompt::AvailableFacts(const FLlmSuspect& Suspect, float Pressure, const TSet<FString>& Revealed)
{
	TArray<const FLlmFact*> Out;
	for (const FLlmFact& F : Suspect.Facts)
	{
		if (Pressure >= F.PressureNeeded && !Revealed.Contains(F.Id))
		{
			Out.Add(&F);
		}
	}
	return Out;
}

FString LlmPrompt::StateNote(const FLlmSuspect& Suspect, float Pressure, const TSet<FString>& Revealed)
{
	FString Note = FString::Printf(TEXT("[STATE] You feel %s."), *PressureWords(Pressure));

	const TArray<const FLlmFact*> Avail = AvailableFacts(Suspect, Pressure, Revealed);
	if (Avail.Num() > 0)
	{
		Note += TEXT("\nSECRETS YOU MAY ADMIT if pressed:");
		for (const FLlmFact* F : Avail) Note += FString::Printf(TEXT(" (%s) %s"), *F->Id, *F->Text);
	}
	else
	{
		Note += TEXT("\nSECRETS YOU MAY ADMIT: none yet. Keep to your story.");
	}

	if (Revealed.Num() > 0)
	{
		Note += TEXT("\nYou have already admitted:");
		for (const FLlmFact& F : Suspect.Facts)
		{
			if (Revealed.Contains(F.Id)) Note += TEXT(" ") + F.Text;
		}
	}
	return Note;
}

TSharedRef<FJsonObject> LlmPrompt::Schema(const FLlmSuspect& Suspect, float Pressure, const TSet<FString>& Revealed)
{
	TArray<FString> FactIds = { TEXT("none") };
	for (const FLlmFact* F : AvailableFacts(Suspect, Pressure, Revealed)) FactIds.Add(F->Id);

	TSharedRef<FJsonObject> Reply = MakeShared<FJsonObject>();
	Reply->SetStringField(TEXT("type"), TEXT("string"));
	Reply->SetNumberField(TEXT("maxLength"), 300);

	// Order matters: tactic first (a short "think" step), then the reply (streams early), then the reveal flag,
	// so f describes what r actually said. With f before r, the harness saw E2B flag a reveal it never spoke.
	TSharedRef<FJsonObject> Props = MakeShared<FJsonObject>();
	Props->SetObjectField(TEXT("t"), EnumProp(Tactics()));
	Props->SetObjectField(TEXT("e"), EnumProp(Emotions()));
	Props->SetObjectField(TEXT("r"), Reply);
	Props->SetObjectField(TEXT("f"), EnumProp(FactIds));

	TArray<TSharedPtr<FJsonValue>> Required;
	for (const TCHAR* K : { TEXT("t"), TEXT("e"), TEXT("r"), TEXT("f") }) Required.Add(MakeShared<FJsonValueString>(K));

	TSharedRef<FJsonObject> Schema = MakeShared<FJsonObject>();
	Schema->SetStringField(TEXT("type"), TEXT("object"));
	Schema->SetObjectField(TEXT("properties"), Props);
	Schema->SetArrayField(TEXT("required"), Required);
	return Schema;
}

FString LlmPrompt::SanitizePlayerLine(const FString& Line, int32 MaxChars)
{
	FString S = Line.Left(MaxChars);
	static const TCHAR* Banned[] = {
		TEXT("<start_of_turn>"), TEXT("<end_of_turn>"), TEXT("<bos>"), TEXT("<eos>"),
		TEXT("<|im_start|>"), TEXT("<|im_end|>"), TEXT("<|endoftext|>"),
		TEXT("[STATE]"), TEXT("[DETECTIVE]"),
	};
	for (const TCHAR* B : Banned)
	{
		S.ReplaceInline(B, TEXT(""), ESearchCase::IgnoreCase);
	}
	S.ReplaceInline(TEXT("\r"), TEXT(" "));
	S.ReplaceInline(TEXT("\n"), TEXT(" "));
	return S.TrimStartAndEnd();
}

TArray<FLlmMessage> LlmPrompt::BuildMessages(const FLlmCase& Case, const FLlmSuspect& Suspect,
	const TArray<TPair<FString, FString>>& History, float Pressure, const TSet<FString>& Revealed, const FString& PlayerLine)
{
	TArray<FLlmMessage> Msgs;
	Msgs.Add({ TEXT("system"), StaticSystem(Case, Suspect) });
	for (const TPair<FString, FString>& Turn : History)
	{
		Msgs.Add({ TEXT("user"), TEXT("[DETECTIVE] ") + Turn.Key });
		Msgs.Add({ TEXT("assistant"), Turn.Value });
	}
	Msgs.Add({ TEXT("user"), StateNote(Suspect, Pressure, Revealed) + TEXT("\n\n[DETECTIVE] ") + PlayerLine });
	return Msgs;
}
