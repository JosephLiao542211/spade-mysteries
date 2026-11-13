#include "Llm/SuspectChatSession.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Llm/LlmServerSubsystem.h"
#include "Llm/LlmSettings.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspectChat, Log, All);

bool USuspectChatSession::Init(ULlmServerSubsystem* InServer, const FString& CasePath, const FString& SuspectPath, FString& OutError)
{
	Server = InServer;
	const FString Dir = FPaths::ProjectContentDir();
	return LlmPrompt::LoadCase(Dir / CasePath, Case, OutError) && LlmPrompt::LoadSuspect(Dir / SuspectPath, Suspect, OutError);
}

void USuspectChatSession::Preload()
{
	if (!Server.IsValid() || !Server->IsReady()) return;
	TArray<FLlmMessage> Msgs = { { TEXT("system"), LlmPrompt::StaticSystem(Case, Suspect) }, { TEXT("user"), TEXT("[DETECTIVE] ...") } };
	PostChat(Msgs, LlmPrompt::Schema(Suspect, 0.f, {}), 1, [](bool, const TSharedPtr<FJsonObject>&, const FString&) {});
}

bool USuspectChatSession::Send(const FString& RawLine)
{
	if (bBusy || !Server.IsValid() || !Server->IsReady())
	{
		return false;
	}
	const ULlmSettings* Settings = GetDefault<ULlmSettings>();
	const FString Line = LlmPrompt::SanitizePlayerLine(RawLine, Settings->MaxPlayerChars);
	if (Line.IsEmpty())
	{
		return false;
	}

	// Trim history in one big step so the cached prefix stays valid between cuts.
	if (History.Num() >= Settings->HistoryMax)
	{
		const int32 Drop = FMath::Clamp(History.Num() - Settings->HistoryKeep, 0, History.Num());
		History.RemoveAt(0, Drop);
	}

	TSet<FString> Allowed;
	for (const FLlmFact* F : LlmPrompt::AvailableFacts(Suspect, Pressure, Revealed)) Allowed.Add(F->Id);

	const TArray<FLlmMessage> Msgs = LlmPrompt::BuildMessages(Case, Suspect, History, Pressure, Revealed, Line);
	const double Start = FPlatformTime::Seconds();
	bBusy = true;

	TWeakObjectPtr<USuspectChatSession> WeakThis(this);
	PostChat(Msgs, LlmPrompt::Schema(Suspect, Pressure, Revealed), Settings->MaxTokens,
		[WeakThis, Line, Allowed, Start](bool bOk, const TSharedPtr<FJsonObject>& Resp, const FString& Err)
		{
			if (USuspectChatSession* Self = WeakThis.Get())
			{
				Self->HandleReply(Line, Allowed, Start, bOk, Resp, Err);
			}
		});
	return true;
}

void USuspectChatSession::PostChat(const TArray<FLlmMessage>& Messages, const TSharedRef<FJsonObject>& Schema, int32 MaxTokens,
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
	Body->SetNumberField(TEXT("id_slot"), Suspect.Slot);
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

void USuspectChatSession::HandleReply(const FString& PlayerLine, const TSet<FString>& AllowedFacts, double StartTime,
	bool bOk, const TSharedPtr<FJsonObject>& Response, const FString& Error)
{
	bBusy = false;
	FSuspectReply Out;
	Out.Seconds = static_cast<float>(FPlatformTime::Seconds() - StartTime);
	Out.PressureAfter = Pressure;

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

	TSharedPtr<FJsonObject> Parsed;
	if (bOk)
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
		if (!FJsonSerializer::Deserialize(Reader, Parsed) || !Parsed.IsValid())
		{
			bOk = false;
		}
	}

	if (!bOk)
	{
		Out.Error = Error.IsEmpty() ? FString::Printf(TEXT("Reply was not valid JSON: %s"), *Content.Left(200)) : Error;
		UE_LOG(LogSuspectChat, Warning, TEXT("%s"), *Out.Error);
		OnReply.Broadcast(Out);
		return;
	}

	Out.bOk = true;
	Parsed->TryGetStringField(TEXT("t"), Out.PlayerTactic);
	Parsed->TryGetStringField(TEXT("e"), Out.Emotion);
	Parsed->TryGetStringField(TEXT("r"), Out.Reply);
	FString RequestedFact;
	Parsed->TryGetStringField(TEXT("f"), RequestedFact);

	// Code owns the pressure meter: the model only classifies the tactic.
	if (const float* Mult = Suspect.TacticMultipliers.Find(Out.PlayerTactic))
	{
		Pressure = FMath::Clamp(Pressure + 10.f * *Mult, 0.f, 100.f);
	}

	// The schema already limits f to available facts; check again so the game stays the source of truth.
	if (RequestedFact != TEXT("none") && AllowedFacts.Contains(RequestedFact))
	{
		Revealed.Add(RequestedFact);
		Out.RevealedFactId = RequestedFact;
	}

	Out.PressureAfter = Pressure;
	History.Add({ PlayerLine, Content });

	UE_LOG(LogSuspectChat, Log, TEXT("[%s] t=%s e=%s f=%s p=%.0f read=%d gen=%d %.2fs"), *Suspect.Id, *Out.PlayerTactic,
		*Out.Emotion, *RequestedFact, Pressure, Out.PromptTokens, Out.GeneratedTokens, Out.Seconds);
	OnReply.Broadcast(Out);
}
