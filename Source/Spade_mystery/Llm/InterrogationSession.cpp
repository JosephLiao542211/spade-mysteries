#include "Llm/InterrogationSession.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Llm/LlmServerSubsystem.h"
#include "Llm/LlmSettings.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogInterrogation, Log, All);

bool UInterrogationSession::Init(ULlmServerSubsystem* InServer, const FString& CaseDir, FString& OutError)
{
	Server = InServer;
	if (!LlmCaseData::LoadCase(FPaths::ProjectContentDir() / CaseDir, Case, OutError))
	{
		return false;
	}
	State.Init(Case);
	return true;
}

void UInterrogationSession::PreloadAll()
{
	if (!Server.IsValid() || !Server->IsReady()) return;
	for (const FLlmSuspectDef& S : Case.Suspects)
	{
		const TArray<FLlmMessage> Msgs = { { TEXT("system"), LlmEngine::StaticSystem(Case, S.Id) }, { TEXT("user"), TEXT("[DETECTIVE] ...") } };
		PostChat(S.Slot, Msgs, LlmEngine::Schema(Case, State, S.Id), 1, [](bool, const TSharedPtr<FJsonObject>&, const FString&) {});
	}
}

bool UInterrogationSession::Ask(const FString& SuspectId, const FString& PlayerLine)
{
	const FString Line = LlmEngine::SanitizePlayerLine(PlayerLine, GetDefault<ULlmSettings>()->MaxPlayerChars);
	if (Line.IsEmpty()) return false;
	return Send(SuspectId, LlmEngine::DetectiveMessage(Line), FString());
}

bool UInterrogationSession::Present(const FString& SuspectId, const FString& EvidenceId)
{
	if (!State.Inventory.Contains(EvidenceId)) return false;
	return Send(SuspectId, LlmEngine::EvidenceMessage(Case, EvidenceId), EvidenceId);
}

bool UInterrogationSession::Send(const FString& SuspectId, const FString& UserMsg, const FString& PresentedEvidence)
{
	const FLlmSuspectDef* Def = Case.FindSuspect(SuspectId);
	FLlmSuspectState* St = State.Suspects.Find(SuspectId);
	if (!Def || !St || Busy.Contains(SuspectId) || !Server.IsValid() || !Server->IsReady())
	{
		return false;
	}

	if (St->bClosed)
	{
		FInterrogationReply Out;
		Out.SuspectId = SuspectId;
		Out.bOk = true;
		Out.bInterviewClosed = true;
		Out.Reply = FString::Printf(TEXT("(%s has walked out and refuses to talk.)"), *Def->Name);
		Out.Meters = St->Meters;
		OnReply.Broadcast(Out);
		return true;
	}

	// Trim history in one big step so the cached prefix stays valid between cuts.
	const ULlmSettings* Settings = GetDefault<ULlmSettings>();
	if (St->History.Num() >= Settings->HistoryMax)
	{
		St->History.RemoveAt(0, FMath::Clamp(St->History.Num() - Settings->HistoryKeep, 0, St->History.Num()));
	}

	TMap<FString, FString> Allowed;
	for (const FLlmAvailableSecret& A : LlmEngine::AvailableSecrets(*Def, *St)) Allowed.Add(A.Id, A.Text);

	const TArray<FLlmMessage> Msgs = LlmEngine::BuildMessages(Case, State, SuspectId, UserMsg);
	const double Start = FPlatformTime::Seconds();
	Busy.Add(SuspectId);

	TWeakObjectPtr<UInterrogationSession> WeakThis(this);
	PostChat(Def->Slot, Msgs, LlmEngine::Schema(Case, State, SuspectId), Settings->MaxTokens,
		[WeakThis, SuspectId, UserMsg, PresentedEvidence, Allowed, Start](bool bOk, const TSharedPtr<FJsonObject>& Resp, const FString& Err)
		{
			if (UInterrogationSession* Self = WeakThis.Get())
			{
				Self->HandleReply(SuspectId, UserMsg, PresentedEvidence, Allowed, Start, bOk, Resp, Err);
			}
		});
	return true;
}

void UInterrogationSession::PostChat(int32 Slot, const TArray<FLlmMessage>& Messages, const TSharedRef<FJsonObject>& Schema, int32 MaxTokens,
	TFunction<void(bool, const TSharedPtr<FJsonObject>&, const FString&)> Done)
{
	const ULlmSettings* Settings = GetDefault<ULlmSettings>();

	TArray<TSharedPtr<FJsonValue>> MsgArr;
	for (const FLlmMessage& M : Messages)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("role"), M.Role);
		O->SetStringField(TEXT("content"), M.Content);
		MsgArr.Add(MakeShared<FJsonValueObject>(O));
	}

	TSharedRef<FJsonObject> JsonSchema = MakeShared<FJsonObject>();
	JsonSchema->SetStringField(TEXT("name"), TEXT("reply"));
	JsonSchema->SetObjectField(TEXT("schema"), Schema);
	TSharedRef<FJsonObject> Format = MakeShared<FJsonObject>();
	Format->SetStringField(TEXT("type"), TEXT("json_schema"));
	Format->SetObjectField(TEXT("json_schema"), JsonSchema);
	TSharedRef<FJsonObject> TemplateKwargs = MakeShared<FJsonObject>();
	TemplateKwargs->SetBoolField(TEXT("enable_thinking"), false);

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetArrayField(TEXT("messages"), MsgArr);
	Body->SetNumberField(TEXT("id_slot"), Slot);
	Body->SetBoolField(TEXT("cache_prompt"), true);
	Body->SetNumberField(TEXT("temperature"), Settings->Temperature);
	Body->SetNumberField(TEXT("max_tokens"), MaxTokens);
	Body->SetObjectField(TEXT("chat_template_kwargs"), TemplateKwargs);
	Body->SetObjectField(TEXT("response_format"), Format);

	FString BodyStr;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Body, Writer);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(Server->GetBaseUrl() / TEXT("v1/chat/completions"));
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Req->SetContentAsString(BodyStr);
	Req->SetTimeout(120.f); // CPU-only machines can be slow on a cold prefix
	Req->OnProcessRequestComplete().BindLambda([Done](FHttpRequestPtr, FHttpResponsePtr Resp, bool bConnected)
	{
		if (!bConnected || !Resp.IsValid())
		{
			Done(false, nullptr, TEXT("No connection to llama-server"));
			return;
		}
		TSharedPtr<FJsonObject> Json;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Resp->GetContentAsString());
		if (Resp->GetResponseCode() != 200 || !FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
		{
			Done(false, nullptr, FString::Printf(TEXT("HTTP %d: %s"), Resp->GetResponseCode(), *Resp->GetContentAsString().Left(300)));
			return;
		}
		Done(true, Json, FString());
	});
	Req->ProcessRequest();
}

void UInterrogationSession::HandleReply(const FString& SuspectId, const FString& UserMsg, const FString& PresentedEvidence,
	const TMap<FString, FString>& Allowed, double StartTime, bool bOk, const TSharedPtr<FJsonObject>& Response, const FString& Error)
{
	Busy.Remove(SuspectId);

	FInterrogationReply Out;
	Out.SuspectId = SuspectId;
	Out.bEvidenceEvent = !PresentedEvidence.IsEmpty();
	Out.Seconds = static_cast<float>(FPlatformTime::Seconds() - StartTime);

	FString Content;
	if (bOk)
	{
		const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
		if (Response->TryGetArrayField(TEXT("choices"), Choices) && Choices->Num() > 0)
		{
			const TSharedPtr<FJsonObject>* Msg = nullptr;
			if ((*Choices)[0]->AsObject()->TryGetObjectField(TEXT("message"), Msg))
			{
				(*Msg)->TryGetStringField(TEXT("content"), Content);
			}
		}
		const TSharedPtr<FJsonObject>* Timings = nullptr;
		if (Response->TryGetObjectField(TEXT("timings"), Timings))
		{
			(*Timings)->TryGetNumberField(TEXT("prompt_n"), Out.PromptTokens);
			(*Timings)->TryGetNumberField(TEXT("predicted_n"), Out.GeneratedTokens);
		}
	}

	FLlmParsedReply Parsed;
	bool bParsed = false;
	if (bOk)
	{
		TSharedPtr<FJsonObject> J;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
		if (FJsonSerializer::Deserialize(Reader, J) && J.IsValid())
		{
			bParsed = J->TryGetStringField(TEXT("t"), Parsed.Tactic) & J->TryGetStringField(TEXT("c"), Parsed.ClaimedEvidence)
				& J->TryGetStringField(TEXT("g"), Parsed.RepeatedSuspect) & J->TryGetStringField(TEXT("f"), Parsed.Secret)
				& J->TryGetStringField(TEXT("e"), Parsed.Emotion) & J->TryGetStringField(TEXT("r"), Parsed.Reply);
		}
	}

	// On failure nothing changes and nothing enters the history (the model must never see a broken reply as its
	// own words). The player can simply try again; evidence effects still apply once, on the retry.
	if (!bOk || !bParsed)
	{
		Out.Error = Error.IsEmpty() ? FString::Printf(TEXT("Reply was not valid JSON: %s"), *Content.Left(200)) : Error;
		UE_LOG(LogInterrogation, Warning, TEXT("[%s] %s"), *SuspectId, *Out.Error);
		if (const FLlmSuspectState* St = State.Suspects.Find(SuspectId)) Out.Meters = St->Meters;
		OnReply.Broadcast(Out);
		return;
	}

	const FLlmTurnEvent Ev = LlmEngine::ResolveTurn(Case, State, SuspectId, UserMsg, &Parsed, Content, Allowed, PresentedEvidence);

	Out.bOk = true;
	Out.PlayerTactic = Parsed.Tactic;
	Out.ClaimedEvidence = Parsed.ClaimedEvidence;
	Out.RepeatedSuspect = Parsed.RepeatedSuspect;
	Out.Emotion = Parsed.Emotion;
	Out.Reply = Parsed.Reply;
	Out.bBluffDetected = Ev.bBluff;
	Out.RevealedSecretId = Ev.Revealed;
	Out.RevealedText = Ev.RevealedText;
	Out.GrantedEvidenceId = Ev.Granted;
	Out.TriggeredNotes = Ev.Triggers;
	if (const FLlmSuspectState* St = State.Suspects.Find(SuspectId))
	{
		Out.Meters = St->Meters;
		Out.bInterviewClosed = St->bClosed;
	}

	UE_LOG(LogInterrogation, Log, TEXT("[%s] t=%s c=%s g=%s f=%s read=%d gen=%d %.2fs%s"), *SuspectId, *Parsed.Tactic,
		*Parsed.ClaimedEvidence, *Parsed.RepeatedSuspect, *Parsed.Secret, Out.PromptTokens, Out.GeneratedTokens, Out.Seconds,
		Ev.Revealed.IsEmpty() ? TEXT("") : *(TEXT(" REVEALED ") + Ev.Revealed));
	OnReply.Broadcast(Out);
}
