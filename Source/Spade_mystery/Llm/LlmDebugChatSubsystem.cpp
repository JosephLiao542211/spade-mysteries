#include "Llm/LlmDebugChatSubsystem.h"

#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Llm/LlmServerSubsystem.h"
#include "Llm/SuspectChatSession.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "LlmDebugChat"

void ULlmDebugChatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<ULlmServerSubsystem>();
	Super::Initialize(Collection);

	ULlmServerSubsystem* Server = GetGameInstance()->GetSubsystem<ULlmServerSubsystem>();
	Server->OnStateChanged.AddUObject(this, &ULlmDebugChatSubsystem::OnServerState);

	Session = NewObject<USuspectChatSession>(this);
	FString Error;
	if (!Session->Init(Server, TEXT("Llm/case.json"), TEXT("Llm/Suspects/edith_vane.json"), Error))
	{
		UE_LOG(LogTemp, Error, TEXT("LlmDebugChat: %s"), *Error);
	}
	Session->OnReply.AddUObject(this, &ULlmDebugChatSubsystem::OnReply);

	Command = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Llm.DebugChat"),
		TEXT("Open the LLM debug chat window with the test suspect"),
		FConsoleCommandDelegate::CreateUObject(this, &ULlmDebugChatSubsystem::OpenWindow),
		ECVF_Default);
}

void ULlmDebugChatSubsystem::Deinitialize()
{
	if (Command)
	{
		IConsoleManager::Get().UnregisterConsoleObject(Command);
		Command = nullptr;
	}
	if (Window.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());
	}
	Window.Reset();
	Super::Deinitialize();
}

void ULlmDebugChatSubsystem::OpenWindow()
{
	if (Window.IsValid())
	{
		Window->BringToFront();
		return;
	}

	Window = SNew(SWindow)
		.Title(FText::Format(LOCTEXT("Title", "LLM debug chat: {0}"), FText::FromString(Session->GetSuspect().Name)))
		.ClientSize(FVector2D(760, 560))
		.SupportsMinimize(false)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(1.f).Padding(6)
			[
				SAssignNew(TranscriptBox, SMultiLineEditableTextBox)
				.IsReadOnly(true)
				.AutoWrapText(true)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(6, 0)
			[
				SAssignNew(StatusText, STextBlock)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(6)
			[
				SAssignNew(InputBox, SEditableTextBox)
				.HintText(LOCTEXT("Hint", "Ask the suspect something, then press Enter"))
				.ClearKeyboardFocusOnCommit(false)
				.OnTextCommitted(FOnTextCommitted::CreateUObject(this, &ULlmDebugChatSubsystem::OnSubmit))
			]
		];

	TWeakObjectPtr<ULlmDebugChatSubsystem> WeakThis(this);
	Window->SetOnWindowClosed(FOnWindowClosed::CreateLambda([WeakThis](const TSharedRef<SWindow>&)
	{
		if (ULlmDebugChatSubsystem* Self = WeakThis.Get())
		{
			Self->Window.Reset();
		}
	}));

	FSlateApplication::Get().AddWindow(Window.ToSharedRef());
	TranscriptBox->SetText(FText::FromString(Transcript));
	RefreshStatus();
}

void ULlmDebugChatSubsystem::OnSubmit(const FText& Text, ETextCommit::Type CommitType)
{
	if (CommitType != ETextCommit::OnEnter || Text.IsEmpty())
	{
		return;
	}
	if (!Session->Send(Text.ToString()))
	{
		Append(TEXT("(not sent: server not ready or a reply is still coming)"));
		return;
	}
	Append(TEXT("DETECTIVE: ") + Text.ToString());
	InputBox->SetText(FText::GetEmpty());
	RefreshStatus();
}

void ULlmDebugChatSubsystem::OnReply(const FSuspectReply& Reply)
{
	if (!Reply.bOk)
	{
		Append(TEXT("(error) ") + Reply.Error);
	}
	else
	{
		Append(FString::Printf(TEXT("%s [%s]: %s"), *Session->GetSuspect().Name.ToUpper(), *Reply.Emotion, *Reply.Reply));
		FString Meta = FString::Printf(TEXT("    tactic=%s  pressure=%.0f  read=%d tok  gen=%d tok  %.2fs"),
			*Reply.PlayerTactic, Reply.PressureAfter, Reply.PromptTokens, Reply.GeneratedTokens, Reply.Seconds);
		if (!Reply.RevealedFactId.IsEmpty())
		{
			Meta += TEXT("  REVEALED: ") + Reply.RevealedFactId;
		}
		Append(Meta);
	}
	RefreshStatus();
}

void ULlmDebugChatSubsystem::OnServerState(ELlmServerState State)
{
	if (State == ELlmServerState::Ready)
	{
		Session->Preload();
	}
	RefreshStatus();
}

void ULlmDebugChatSubsystem::Append(const FString& Line)
{
	Transcript += Line + TEXT("\n");
	if (TranscriptBox.IsValid())
	{
		TranscriptBox->SetText(FText::FromString(Transcript));
	}
}

void ULlmDebugChatSubsystem::RefreshStatus()
{
	if (!StatusText.IsValid())
	{
		return;
	}
	const ULlmServerSubsystem* Server = GetGameInstance()->GetSubsystem<ULlmServerSubsystem>();
	FString S;
	switch (Server->GetState())
	{
	case ELlmServerState::Stopped:  S = TEXT("Server: stopped"); break;
	case ELlmServerState::Starting: S = TEXT("Server: loading model..."); break;
	case ELlmServerState::Ready:    S = FString::Printf(TEXT("Server: ready at %s%s"), *Server->GetBaseUrl(), Server->IsCpuFallback() ? TEXT(" (CPU fallback)") : TEXT("")); break;
	case ELlmServerState::Failed:   S = TEXT("Server: FAILED - ") + Server->GetLastError(); break;
	}
	S += FString::Printf(TEXT("   |   pressure %.0f   |   %s"), Session->GetPressure(), Session->IsBusy() ? TEXT("thinking...") : TEXT("your turn"));
	StatusText->SetText(FText::FromString(S));
}

#undef LOCTEXT_NAMESPACE
