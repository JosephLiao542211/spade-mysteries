#include "Llm/LlmDebugChatSubsystem.h"

#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Llm/InterrogationSession.h"
#include "Llm/LlmServerSubsystem.h"
#include "Llm/LlmSettings.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SWrapBox.h"
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

	Session = NewObject<UInterrogationSession>(this);
	FString Error;
	if (!Session->Init(Server, GetDefault<ULlmSettings>()->CaseDir, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("LlmDebugChat: %s"), *Error);
	}
	else if (Session->GetCase().Suspects.Num() > 0)
	{
		CurrentSuspect = Session->GetCase().Suspects[0].Id;
	}
	Session->OnReply.AddUObject(this, &ULlmDebugChatSubsystem::OnReply);

	Command = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Llm.DebugChat"),
		TEXT("Open the LLM debug interrogation window for the case in Project Settings > Local LLM"),
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

	TWeakObjectPtr<ULlmDebugChatSubsystem> WeakThis(this);
	TSharedRef<SHorizontalBox> SuspectRow = SNew(SHorizontalBox);
	for (const FLlmSuspectDef& S : Session->GetCase().Suspects)
	{
		const FString Id = S.Id;
		SuspectRow->AddSlot().AutoWidth().Padding(2)
		[
			SNew(SButton)
			.Text(FText::FromString(S.Name))
			.OnClicked_Lambda([WeakThis, Id]()
			{
				if (ULlmDebugChatSubsystem* Self = WeakThis.Get()) Self->SelectSuspect(Id);
				return FReply::Handled();
			})
		];
	}
	SuspectRow->AddSlot().FillWidth(1.f);
	SuspectRow->AddSlot().AutoWidth().Padding(2)
	[
		SNew(SButton)
		.Text(LOCTEXT("Accuse", "Accuse this suspect"))
		.OnClicked_Lambda([WeakThis]()
		{
			if (ULlmDebugChatSubsystem* Self = WeakThis.Get()) Self->OnAccuse();
			return FReply::Handled();
		})
	];

	Window = SNew(SWindow)
		.Title(FText::FromString(Session->GetCase().Title.IsEmpty() ? TEXT("LLM debug interrogation") : Session->GetCase().Title))
		.ClientSize(FVector2D(900, 680))
		.SupportsMinimize(false)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(6, 6, 6, 0) [ SuspectRow ]
			+ SVerticalBox::Slot().AutoHeight().Padding(6, 4) [ SAssignNew(HeaderText, STextBlock) ]
			+ SVerticalBox::Slot().FillHeight(1.f).Padding(6, 0)
			[
				SAssignNew(TranscriptBox, SMultiLineEditableTextBox).IsReadOnly(true).AutoWrapText(true)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(6, 4) [ SAssignNew(StatusText, STextBlock).AutoWrapText(true) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(4, 0) [ SAssignNew(EvidenceBox, SWrapBox).UseAllottedSize(true) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(6)
			[
				SAssignNew(InputBox, SEditableTextBox)
				.HintText(LOCTEXT("Hint", "Say something to the suspect, then press Enter"))
				.ClearKeyboardFocusOnCommit(false)
				.OnTextCommitted(FOnTextCommitted::CreateUObject(this, &ULlmDebugChatSubsystem::OnSubmit))
			]
		];

	Window->SetOnWindowClosed(FOnWindowClosed::CreateLambda([WeakThis](const TSharedRef<SWindow>&)
	{
		if (ULlmDebugChatSubsystem* Self = WeakThis.Get()) Self->Window.Reset();
	}));

	FSlateApplication::Get().AddWindow(Window.ToSharedRef());
	RebuildEvidenceButtons();
	RefreshTranscript();
	RefreshStatus();
}

void ULlmDebugChatSubsystem::SelectSuspect(const FString& SuspectId)
{
	CurrentSuspect = SuspectId;
	RefreshTranscript();
	RefreshStatus();
}

void ULlmDebugChatSubsystem::OnSubmit(const FText& Text, ETextCommit::Type CommitType)
{
	if (CommitType != ETextCommit::OnEnter || Text.IsEmpty()) return;
	if (!Session->Ask(CurrentSuspect, Text.ToString()))
	{
		Append(CurrentSuspect, TEXT("(not sent: server not ready or a reply is still coming)"));
		return;
	}
	Append(CurrentSuspect, TEXT("DETECTIVE: ") + Text.ToString());
	InputBox->SetText(FText::GetEmpty());
	RefreshStatus();
}

void ULlmDebugChatSubsystem::OnPresent(const FString& EvidenceId)
{
	const FLlmEvidence* E = Session->GetCase().FindEvidence(EvidenceId);
	if (!Session->Present(CurrentSuspect, EvidenceId))
	{
		Append(CurrentSuspect, TEXT("(not shown: server not ready or a reply is still coming)"));
		return;
	}
	Append(CurrentSuspect, FString::Printf(TEXT("DETECTIVE shows: %s"), E ? *E->Name : *EvidenceId));
	RefreshStatus();
}

void ULlmDebugChatSubsystem::OnAccuse()
{
	const FLlmAccusationResult R = Session->Accuse(CurrentSuspect);
	Append(CurrentSuspect, FString::Printf(TEXT("== ACCUSATION of %s: %s %s"), *SuspectName(CurrentSuspect), R.bWin ? TEXT("CASE SOLVED.") : TEXT("FAILED."), *R.Text));
}

void ULlmDebugChatSubsystem::OnReply(const FInterrogationReply& Reply)
{
	const FString& Sid = Reply.SuspectId;
	if (!Reply.bOk)
	{
		Append(Sid, TEXT("(error, try again) ") + Reply.Error);
	}
	else if (Reply.bInterviewClosed && Reply.PlayerTactic.IsEmpty())
	{
		Append(Sid, Reply.Reply);
	}
	else
	{
		Append(Sid, FString::Printf(TEXT("%s [%s]: %s"), *SuspectName(Sid).ToUpper(), *Reply.Emotion, *Reply.Reply));
		// Show the player how their line landed: misreads become feedback instead of a hidden penalty.
		FString Meta = Reply.bEvidenceEvent ? TEXT("    (reacting to evidence)") : FString::Printf(TEXT("    read as: %s"), *Reply.PlayerTactic);
		if (Reply.bBluffDetected) Meta += TEXT("  |  BLUFF: you don't have that");
		if (!Reply.RepeatedSuspect.IsEmpty() && Reply.RepeatedSuspect != TEXT("none")) Meta += TEXT("  |  you repeated ") + SuspectName(Reply.RepeatedSuspect) + TEXT("'s words");
		Meta += FString::Printf(TEXT("  |  read %d tok, gen %d tok, %.2fs"), Reply.PromptTokens, Reply.GeneratedTokens, Reply.Seconds);
		Append(Sid, Meta);
		if (!Reply.RevealedText.IsEmpty()) Append(Sid, TEXT("    *** CLUE: ") + Reply.RevealedText);
		if (const FLlmEvidence* E = Session->GetCase().FindEvidence(Reply.GrantedEvidenceId)) Append(Sid, TEXT("    +++ NEW EVIDENCE: ") + E->Name);
		for (const FString& N : Reply.TriggeredNotes) Append(Sid, TEXT("    !!! ") + N);
	}
	RebuildEvidenceButtons();
	RefreshStatus();
}

void ULlmDebugChatSubsystem::OnServerState(ELlmServerState State)
{
	if (State == ELlmServerState::Ready) Session->PreloadAll();
	RefreshStatus();
}

void ULlmDebugChatSubsystem::Append(const FString& SuspectId, const FString& Line)
{
	Transcripts.FindOrAdd(SuspectId) += Line + TEXT("\n");
	if (SuspectId == CurrentSuspect) RefreshTranscript();
}

void ULlmDebugChatSubsystem::RefreshTranscript()
{
	if (TranscriptBox.IsValid()) TranscriptBox->SetText(FText::FromString(Transcripts.FindRef(CurrentSuspect)));
	if (HeaderText.IsValid()) HeaderText->SetText(FText::FromString(TEXT("Questioning: ") + SuspectName(CurrentSuspect)));
}

void ULlmDebugChatSubsystem::RefreshStatus()
{
	if (!StatusText.IsValid()) return;
	const ULlmServerSubsystem* Server = GetGameInstance()->GetSubsystem<ULlmServerSubsystem>();
	FString S;
	switch (Server->GetState())
	{
	case ELlmServerState::Stopped:  S = TEXT("Server: stopped"); break;
	case ELlmServerState::Starting: S = TEXT("Server: loading model..."); break;
	case ELlmServerState::Ready:    S = FString::Printf(TEXT("Server: ready at %s%s"), *Server->GetBaseUrl(), Server->IsCpuFallback() ? TEXT(" (CPU fallback)") : TEXT("")); break;
	case ELlmServerState::Failed:   S = TEXT("Server: FAILED - ") + Server->GetLastError(); break;
	}

	// Meters are debug-only: the real UI should show feelings through the suspect's face and voice, not numbers.
	const FLlmSuspectDef* Def = Session->GetCase().FindSuspect(CurrentSuspect);
	const FLlmSuspectState* St = Session->GetState().Suspects.Find(CurrentSuspect);
	if (Def && St)
	{
		S += TEXT("\nMeters:");
		for (const FLlmMeterDef& M : Def->Meters) S += FString::Printf(TEXT("  %s %.0f"), *M.Name, St->Meter(M.Name));
		if (St->ShutdownLeft > 0) S += FString::Printf(TEXT("  |  SHUT DOWN for %d more turns"), St->ShutdownLeft);
		if (St->bClosed) S += TEXT("  |  INTERVIEW CLOSED");
		S += Session->IsBusy(CurrentSuspect) ? TEXT("  |  thinking...") : TEXT("  |  your turn");
	}
	StatusText->SetText(FText::FromString(S));
}

void ULlmDebugChatSubsystem::RebuildEvidenceButtons()
{
	if (!EvidenceBox.IsValid()) return;
	EvidenceBox->ClearChildren();
	TWeakObjectPtr<ULlmDebugChatSubsystem> WeakThis(this);
	for (const FLlmEvidence& E : Session->GetCase().Evidence)
	{
		if (!Session->GetState().Inventory.Contains(E.Id)) continue;
		const FString Id = E.Id;
		EvidenceBox->AddSlot().Padding(2)
		[
			SNew(SButton)
			.Text(FText::FromString(TEXT("Show: ") + E.Name))
			.ToolTipText(FText::FromString(E.Description))
			.OnClicked_Lambda([WeakThis, Id]()
			{
				if (ULlmDebugChatSubsystem* Self = WeakThis.Get()) Self->OnPresent(Id);
				return FReply::Handled();
			})
		];
	}
}

FString ULlmDebugChatSubsystem::SuspectName(const FString& SuspectId) const
{
	const FLlmSuspectDef* S = Session ? Session->GetCase().FindSuspect(SuspectId) : nullptr;
	return S ? S->Name : SuspectId;
}

#undef LOCTEXT_NAMESPACE
