#include "NoirPlayerController.h"
#include "NoirRoom.h"
#include "NoirHUD.h"
#include "NoirLookSettings.h"
#include "Components/InputComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Llm/InterrogationSession.h"
#include "Llm/LlmServerSubsystem.h"
#include "Llm/LlmSettings.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogNoir, Log, All);

namespace
{
	/** What the table shows before a suspect has said anything. Everything else about them is in the case JSON. */
	void CastSheet(const FString& Id, FString& OutRole, FString& OutGreeting)
	{
		if (Id == TEXT("vera_lane"))
		{
			OutRole = TEXT("Torch singer, the Black Spade Club");
			OutGreeting = TEXT("Make it quick, detective. I've got a second set at one, and I'd rather sing to an empty room than to you.");
		}
		else if (Id == TEXT("mickey_doyle"))
		{
			OutRole = TEXT("Bartender, the Black Spade Club");
			OutGreeting = TEXT("Get you something, detective? On the house. Tonight everything's on the house. Twelve years behind that bar and I never seen a night like it.");
		}
		else if (Id == TEXT("harlan_graves"))
		{
			OutRole = TEXT("Ace's business partner");
			OutGreeting = TEXT("Sit down if you must. My lawyer will hear every word of this.");
		}
		else if (Id == TEXT("tommy_mullen"))
		{
			OutRole = TEXT("Ace's driver and mechanic");
			OutGreeting = TEXT("Detective! Pull up a chair. Rotten night, sure, but you look like a fella who could use a smile.");
		}
		else if (Id == TEXT("allison_wu"))
		{
			OutRole = TEXT("Singer, the Black Spade Club");
			OutGreeting = TEXT("Detective. Sit, if you like. I don't bite, whatever they told you downstairs.");
		}
		else if (Id == TEXT("logan_wu"))
		{
			OutRole = TEXT("Importer, Allison's husband");
			OutGreeting = TEXT("Detective. Forgive the handkerchief. This room is warm, and the evening has been... long.");
		}
		else
		{
			OutRole = TEXT("Held for questioning");
			OutGreeting = TEXT("Detective.");
		}
	}

	/** Keys that type a character: value = unshifted then shifted character (US layout). */
	const TMap<FKey, FString>& CharKeys()
	{
		static const TMap<FKey, FString> Map = []
		{
			TMap<FKey, FString> M;
			const FKey Letters[] = {
				EKeys::A, EKeys::B, EKeys::C, EKeys::D, EKeys::E, EKeys::F, EKeys::G, EKeys::H, EKeys::I, EKeys::J, EKeys::K, EKeys::L, EKeys::M,
				EKeys::N, EKeys::O, EKeys::P, EKeys::Q, EKeys::R, EKeys::S, EKeys::T, EKeys::U, EKeys::V, EKeys::W, EKeys::X, EKeys::Y, EKeys::Z };
			for (int32 i = 0; i < 26; ++i)
			{
				FString Pair;
				Pair.AppendChar(TCHAR(TEXT('a') + i));
				Pair.AppendChar(TCHAR(TEXT('A') + i));
				M.Add(Letters[i], Pair);
			}
			M.Add(EKeys::One, TEXT("1!"));
			M.Add(EKeys::Two, TEXT("2@"));
			M.Add(EKeys::Three, TEXT("3#"));
			M.Add(EKeys::Four, TEXT("4$"));
			M.Add(EKeys::Five, TEXT("5%"));
			M.Add(EKeys::Six, TEXT("6^"));
			M.Add(EKeys::Seven, TEXT("7&"));
			M.Add(EKeys::Eight, TEXT("8*"));
			M.Add(EKeys::Nine, TEXT("9("));
			M.Add(EKeys::Zero, TEXT("0)"));
			M.Add(EKeys::SpaceBar, TEXT("  "));
			M.Add(EKeys::Period, TEXT(".>"));
			M.Add(EKeys::Comma, TEXT(",<"));
			M.Add(EKeys::Slash, TEXT("/?"));
			M.Add(EKeys::Apostrophe, TEXT("'\""));
			M.Add(EKeys::Hyphen, TEXT("-_"));
			M.Add(EKeys::Semicolon, TEXT(";:"));
			return M;
		}();
		return Map;
	}

	/** Notebook entry for a number key, or INDEX_NONE. */
	int32 DigitEntry(const FKey& Key)
	{
		const FKey Digits[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
		for (int32 i = 0; i < 9; ++i)
		{
			if (Key == Digits[i])
			{
				return i;
			}
		}
		return INDEX_NONE;
	}
}

ANoirPlayerController::ANoirPlayerController()
{
	bShowMouseCursor = true; // buttons are clickable; keys work too
}

FString ANoirPlayerController::GetTitle() const
{
	const FString Title = Session ? Session->GetCase().Title : FString();
	return Title.IsEmpty() ? FString(TEXT("THE BLACK SPADE KILLING")) : Title.ToUpper();
}

FString ANoirPlayerController::GetBriefing() const
{
	if (Suspects.IsEmpty())
	{
		return TEXT("The case file could not be read (Content/Llm/Cases). See the log.");
	}
	return TEXT("11:10 PM. Jimmy \"Ace\" Calloway, owner of the Black Spade Club, is found shot dead at the desk of his upstairs office. "
		"At eleven sharp the lights went out for four minutes. The band played on in the dark, and nobody heard the shot. No gun has been found. "
		"Six people from the back of the club are waiting downstairs. Sit them under the lamp and question them in your own words: "
		"be kind, be clever, or lean on them, and watch how each one takes it. "
		"What a suspect gives up goes in your notebook. Repeat it to the others and see who flinches. Then name the killer.");
}

FString ANoirPlayerController::GetServerStatus() const
{
	const UGameInstance* GI = GetGameInstance();
	const ULlmServerSubsystem* Server = GI ? GI->GetSubsystem<ULlmServerSubsystem>() : nullptr;
	if (!Server)
	{
		return TEXT("No language model: the suspects cannot answer.");
	}
	switch (Server->GetState())
	{
	case ELlmServerState::Ready:
		if (Session && !Session->GetPreloadError().IsEmpty()) return TEXT("Interview preparation failed: ") + Session->GetPreloadError();
		if (Session && Session->IsPreloading()) return TEXT("Preparing the six interviews...");
		return FString();
	case ELlmServerState::Starting: return TEXT("The suspects are being brought in... (loading the language model)");
	case ELlmServerState::Failed:   return TEXT("The language model is unavailable: ") + Server->GetLastError();
	default:                        return TEXT("The language model is not running: the suspects cannot answer.");
	}
}

void ANoirPlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);

	// Build the room far from the level origin so it never overlaps whatever map we're in.
	if (IsLocalController())
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Room = GetWorld()->SpawnActor<ANoirRoom>(FVector(0, 0, -50000), FRotator::ZeroRotator, Params);
	}

	// The HUD shows the dithered scene capture, so the normal 3D view is not needed.
	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		Viewport->bDisableWorldRendering = true;
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (ULlmServerSubsystem* Server = GI->GetSubsystem<ULlmServerSubsystem>())
		{
			Server->OnStateChanged.AddUObject(this, &ANoirPlayerController::HandleServerState);
		}
	}
	StartCase();
}

void ANoirPlayerController::StartCase()
{
	UGameInstance* GI = GetGameInstance();
	ULlmServerSubsystem* Server = GI ? GI->GetSubsystem<ULlmServerSubsystem>() : nullptr;

	// A fresh session is a fresh playthrough: meters, secrets, notebook and histories all start over.
	if (Session)
	{
		Session->OnReply.RemoveAll(this);
	}
	Session = NewObject<UInterrogationSession>(this);
	Suspects.Reset();

	FString Error;
	if (!Session->Init(Server, GetDefault<ULlmSettings>()->CaseDir, Error))
	{
		UE_LOG(LogNoir, Error, TEXT("Could not load the case: %s"), *Error);
		return;
	}
	Session->OnReply.AddUObject(this, &ANoirPlayerController::HandleReply);

	// Suspects sit in case order, which is also their Noir Look slot (Suspect1..6).
	for (const FLlmSuspectDef& Def : Session->GetCase().Suspects)
	{
		FNoirSuspect& S = Suspects.AddDefaulted_GetRef();
		S.Id = Def.Id;
		S.Name = Def.Name;
		CastSheet(Def.Id, S.Role, S.Greeting);
	}

	if (Server && Server->IsReady())
	{
		Session->PreloadAll();
	}
}

void ANoirPlayerController::HandleServerState(ELlmServerState NewState)
{
	if (NewState == ELlmServerState::Ready && Session)
	{
		Session->PreloadAll(); // cache every suspect's character sheet so first answers are fast
	}
}

void ANoirPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	auto Bind = [this](const FKey& Key, EInputEvent Event)
	{
		FInputKeyBinding Binding(FInputChord(Key), Event);
		const bool bRepeat = (Event == IE_Repeat);
		Binding.KeyDelegate.GetDelegateForManualSet().BindWeakLambda(this, [this, Key, bRepeat]() { HandleKey(Key, bRepeat); });
		InputComponent->KeyBindings.Add(Binding);
	};

	TArray<FKey> Keys;
	CharKeys().GetKeys(Keys);
	Keys.Add(EKeys::Enter);
	Keys.Add(EKeys::Escape);
	Keys.Add(EKeys::BackSpace);
	Keys.Add(EKeys::Tab);
	Keys.Add(EKeys::Up);
	Keys.Add(EKeys::Down);
	Keys.Add(EKeys::Left);
	Keys.Add(EKeys::Right);
	for (const FKey& Key : Keys)
	{
		Bind(Key, IE_Pressed);
		Bind(Key, IE_Repeat);
	}

	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ThisClass::OnClick);
}

void ANoirPlayerController::OnClick()
{
	float MX = 0.f, MY = 0.f;
	const ANoirHUD* Hud = Cast<ANoirHUD>(GetHUD());
	if (Hud && GetMousePosition(MX, MY))
	{
		DoAction(Hud->ActionAt(FVector2D(MX, MY)));
	}
}

void ANoirPlayerController::HandleKey(FKey Key, bool bRepeat)
{
	// Talking: the keyboard types the detective's line.
	if (State == ENoirState::Interrogate && bTalking && !bNotebook)
	{
		if (Key == TalkOpener)
		{
			if (bRepeat) return;
			TalkOpener = FKey(); // A new press of the same letter should type immediately.
		}
		if (Key == EKeys::Enter || Key == EKeys::Tab || Key == EKeys::Escape || Key == EKeys::Down || Key == EKeys::Up || Key == EKeys::Left || Key == EKeys::Right)
		{
			if (bRepeat)
			{
				return;
			}
			if (Key == EKeys::Enter)      Submit();
			else if (Key == EKeys::Tab)   DoAction(ENoirAction::Notebook);
			else if (Key == EKeys::Down || Key == EKeys::Escape) DoAction(ENoirAction::Talk);
			else if (Key == EKeys::Left)  DoAction(ENoirAction::Prev);
			else if (Key == EKeys::Right) DoAction(ENoirAction::Next);
			return;
		}
		if (HasLeft(Current) || !GetServerStatus().IsEmpty()) return;
		if (Key == EKeys::BackSpace)
		{
			if (!Input.IsEmpty()) Input = Input.LeftChop(1);
			return;
		}
		if (const FString* Pair = CharKeys().Find(Key))
		{
			const bool bShift = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
			if (Input.Len() < GetDefault<ULlmSettings>()->MaxPlayerChars)
			{
				Input.AppendChar((*Pair)[bShift ? 1 : 0]);
			}
		}
		return;
	}

	if (bRepeat)
	{
		return;
	}

	// Notebook: numbers repeat a statement to whoever is in the chair.
	if (State == ENoirState::Interrogate && bNotebook)
	{
		const int32 Entry = DigitEntry(Key);
		if (Entry != INDEX_NONE)
		{
			PresentEntry(Entry);
			TalkOpener = Key;
		}
		else if (Key == EKeys::Tab || Key == EKeys::N || Key == EKeys::Down || Key == EKeys::Escape)
		{
			DoAction(ENoirAction::Notebook);
			TalkOpener = Key;
		}
		else if (Key == EKeys::Left || Key == EKeys::A)
		{
			DoAction(ENoirAction::Prev);
		}
		else if (Key == EKeys::Right || Key == EKeys::D)
		{
			DoAction(ENoirAction::Next);
		}
		return;
	}

	if (Key == EKeys::SpaceBar || Key == EKeys::Enter) DoAction(ENoirAction::Begin);
	else if (Key == EKeys::T || Key == EKeys::Up)
	{
		DoAction(ENoirAction::Talk);
		TalkOpener = Key;
	}
	else if (Key == EKeys::Right || Key == EKeys::D)   DoAction(ENoirAction::Next);
	else if (Key == EKeys::Left || Key == EKeys::A)    DoAction(ENoirAction::Prev);
	else if (Key == EKeys::X)                          DoAction(ENoirAction::Accuse);
	else if (Key == EKeys::Y)                          DoAction(ENoirAction::Yes);
	else if (Key == EKeys::N)                          DoAction(State == ENoirState::ConfirmAccuse ? ENoirAction::No : ENoirAction::Notebook);
	else if (Key == EKeys::Tab)                        DoAction(ENoirAction::Notebook);
	else if (Key == EKeys::R)                          DoAction(ENoirAction::Restart);
	else if (Key == EKeys::M)                          DoAction(ENoirAction::Mood);
}

void ANoirPlayerController::DoAction(ENoirAction Action)
{
	switch (Action)
	{
	case ENoirAction::None:
		break;
	case ENoirAction::Begin:
		if (State == ENoirState::Briefing && Suspects.Num() > 0 && GetServerStatus().IsEmpty())
		{
			State = ENoirState::Interrogate;
		}
		break;
	case ENoirAction::Talk:
		if (State == ENoirState::Interrogate)
		{
			bTalking = !bTalking;
			bNotebook = false;
			TalkOpener = FKey();
			Greet();
		}
		break;
	case ENoirAction::Prev:   SelectSuspect(Current - 1); break;
	case ENoirAction::Next:   SelectSuspect(Current + 1); break;
	case ENoirAction::Notebook:
		if (State == ENoirState::Interrogate)
		{
			bNotebook = !bNotebook;
		}
		break;
	case ENoirAction::Mood:
		if (State == ENoirState::Interrogate)
		{
			if (FNoirSuspectSlot* Slot = GetMutableDefault<UNoirLookSettings>()->GetMutableSuspectSlot(Current))
			{
				Slot->ActiveMood = (Slot->ActiveMood + 1) % (Slot->MoodVideos.Num() + 1);
			}
		}
		break;
	case ENoirAction::Accuse:
		if (State == ENoirState::Interrogate)
		{
			State = ENoirState::ConfirmAccuse;
			bTalking = false;
			bNotebook = false;
		}
		break;
	case ENoirAction::Yes:
		if (State == ENoirState::ConfirmAccuse)
		{
			Accuse();
		}
		break;
	case ENoirAction::No:
		if (State == ENoirState::ConfirmAccuse)
		{
			State = ENoirState::Interrogate;
		}
		break;
	case ENoirAction::Restart:
		if (State == ENoirState::Solved || State == ENoirState::Failed)
		{
			State = ENoirState::Briefing;
			Current = 0;
			bTalking = false;
			bNotebook = false;
			Input.Reset();
			EndingTitle.Reset();
			EndingText.Reset();
			StartCase();
		}
		break;
	default:
	{
		const int32 Entry = int32(Action) - int32(ENoirAction::Present0);
		if (Entry >= 0)
		{
			PresentEntry(Entry);
		}
		break;
	}
	}
}

void ANoirPlayerController::AddNote(FNoirSuspect& S, const FString& Text, double Time)
{
	FNoirLine Line;
	Line.Kind = ENoirLineKind::Note;
	Line.Text = Text;
	Line.Time = Time;
	S.Log.Add(Line);
}

void ANoirPlayerController::Greet()
{
	if (!Suspects.IsValidIndex(Current))
	{
		return;
	}
	FNoirSuspect& S = Suspects[Current];
	if (bTalking && !HasLeft(Current) && S.Log.IsEmpty() && !S.Greeting.IsEmpty())
	{
		FNoirLine Line;
		Line.Kind = ENoirLineKind::Suspect;
		Line.Text = S.Greeting;
		Line.Time = GetWorld()->GetRealTimeSeconds();
		S.Log.Add(Line);
	}
}

void ANoirPlayerController::Submit()
{
	const FString Said = Input.TrimStartAndEnd();
	if (Said.IsEmpty() || !Session || !Suspects.IsValidIndex(Current) || HasLeft(Current))
	{
		return;
	}
	FNoirSuspect& S = Suspects[Current];
	const double Now = GetWorld()->GetRealTimeSeconds();

	if (Session->IsBusy(S.Id))
	{
		return; // still answering the last line; keep what was typed
	}
	const FString Status = GetServerStatus();
	if (!Status.IsEmpty())
	{
		AddNote(S, Status, Now);
		return;
	}
	if (!Session->Ask(S.Id, Said))
	{
		AddNote(S, TEXT("(No answer. Try that again.)"), Now);
		return;
	}
	FNoirLine Line;
	Line.Kind = ENoirLineKind::Player;
	Line.Text = Said;
	Line.Time = Now;
	S.Log.Add(Line);
	S.LastRead.Reset();
	Input.Reset();
}

void ANoirPlayerController::PresentEntry(int32 Index)
{
	const TArray<FString> Ids = NotebookIds();
	if (State != ENoirState::Interrogate || !Session || !Ids.IsValidIndex(Index) || !Suspects.IsValidIndex(Current) || HasLeft(Current))
	{
		return;
	}
	FNoirSuspect& S = Suspects[Current];
	const double Now = GetWorld()->GetRealTimeSeconds();
	if (Session->IsBusy(S.Id))
	{
		return;
	}

	// Show the answer: close the notebook and open the conversation.
	bNotebook = false;
	if (!bTalking)
	{
		bTalking = true;
		TalkOpener = FKey();
		Greet();
	}

	const FString Status = GetServerStatus();
	if (!Status.IsEmpty())
	{
		AddNote(S, Status, Now);
		return;
	}
	if (!Session->Present(S.Id, Ids[Index]))
	{
		AddNote(S, TEXT("(No answer. Try that again.)"), Now);
		return;
	}
	FNoirLine Line;
	Line.Kind = ENoirLineKind::Note;
	Line.Text = TEXT("You repeat it to them: ") + EvidenceText(Ids[Index]);
	Line.Time = Now;
	S.Log.Add(Line);
	S.LastRead.Reset();
}

void ANoirPlayerController::Accuse()
{
	if (!Session || !Suspects.IsValidIndex(Current))
	{
		State = ENoirState::Interrogate;
		return;
	}
	const FLlmAccusationResult Result = Session->Accuse(Suspects[Current].Id);
	State = Result.bWin ? ENoirState::Solved : ENoirState::Failed;
	EndingTitle = Result.Title;
	EndingText = Result.Text;
}

void ANoirPlayerController::HandleReply(const FInterrogationReply& Reply)
{
	const int32 Index = Suspects.IndexOfByPredicate([&](const FNoirSuspect& X) { return X.Id == Reply.SuspectId; });
	if (Index == INDEX_NONE)
	{
		return;
	}
	FNoirSuspect& S = Suspects[Index];
	const double Now = GetWorld()->GetRealTimeSeconds();

	if (!Reply.bOk)
	{
		// Nothing changed in the case; the player can simply say it again.
		AddNote(S, TEXT("(They did not catch that. Say it again.)"), Now);
		return;
	}
	if (Reply.bInterviewClosed && Reply.PlayerTactic.IsEmpty())
	{
		AddNote(S, Reply.Reply, Now); // already gone: nobody is in the chair to answer
		return;
	}

	FNoirLine Line;
	Line.Kind = ENoirLineKind::Suspect;
	Line.Text = Reply.Reply;
	Line.Time = Now;
	S.Log.Add(Line);

	// Feedback appears once the spoken line has been typed out.
	const double After = Now + Reply.Reply.Len() / 50.0 + 0.3;
	S.LastRead = Reply.bEvidenceEvent ? FString() : Reply.PlayerTactic.Replace(TEXT("_"), TEXT(" ")).ToUpper();
	if (Reply.bBluffDetected)
	{
		AddNote(S, TEXT("BLUFF CAUGHT: you claimed a statement that is not in your notebook."), After);
	}
	if (!Reply.RevealedText.IsEmpty())
	{
		AddNote(S, TEXT("CLUE: ") + Reply.RevealedText, After);
	}
	for (const FString& Note : Reply.TriggeredNotes)
	{
		AddNote(S, Note, After);
	}
	for (const FString& Granted : Reply.GrantedEvidenceIds)
	{
		AddNote(S, TEXT("NOTEBOOK + ") + EvidenceName(Granted), After);
	}
	SetMoodFor(Index, Reply.Emotion);

	// Someone walking out can take others with them, so check everyone.
	for (int32 i = 0; i < Suspects.Num(); ++i)
	{
		if (!Suspects[i].bLeftNoted && HasLeft(i))
		{
			Suspects[i].bLeftNoted = true;
			if (i == Current) Input.Reset();
			AddNote(Suspects[i], Suspects[i].Name + TEXT(" has walked out. This interview is over."), After);
		}
	}
}

void ANoirPlayerController::SetMoodFor(int32 Suspect, const FString& Emotion)
{
	FNoirSuspectSlot* Slot = GetMutableDefault<UNoirLookSettings>()->GetMutableSuspectSlot(Suspect);
	if (!Slot || Slot->MoodVideos.Num() == 0)
	{
		return;
	}
	// Mood clips are named <Name>_<mood>.mp4. Pick the nearest one; no match = the slot's base clip.
	FString Want = TEXT("relaxed");
	if (Emotion == TEXT("angry"))
	{
		Want = TEXT("angry");
	}
	else if (Emotion == TEXT("sad") || Emotion == TEXT("ashamed") || Emotion == TEXT("afraid") || Emotion == TEXT("nervous"))
	{
		Want = TEXT("sad");
	}
	int32 Mood = 0;
	for (int32 i = 0; i < Slot->MoodVideos.Num(); ++i)
	{
		if (FPaths::GetBaseFilename(Slot->MoodVideos[i].FilePath).Contains(Want))
		{
			Mood = i + 1;
			break;
		}
	}
	Slot->ActiveMood = Mood;
}

TArray<FString> ANoirPlayerController::NotebookIds() const
{
	TArray<FString> Out;
	if (Session)
	{
		for (const FLlmEvidence& E : Session->GetCase().Evidence)
		{
			if (Session->GetState().Inventory.Contains(E.Id))
			{
				Out.Add(E.Id);
			}
		}
	}
	return Out;
}

FString ANoirPlayerController::EvidenceName(const FString& EvidenceId) const
{
	const FLlmEvidence* E = Session ? Session->GetCase().FindEvidence(EvidenceId) : nullptr;
	return E ? E->Name : EvidenceId;
}

FString ANoirPlayerController::EvidenceText(const FString& EvidenceId) const
{
	const FLlmEvidence* E = Session ? Session->GetCase().FindEvidence(EvidenceId) : nullptr;
	return E ? E->Description : EvidenceId;
}

bool ANoirPlayerController::HasHeard(const FString& EvidenceId) const
{
	if (!Session || !Suspects.IsValidIndex(Current))
	{
		return false;
	}
	const FLlmSuspectState* St = Session->GetState().Suspects.Find(Suspects[Current].Id);
	return St && St->EvidenceSeen.Contains(EvidenceId);
}

TArray<FNoirMeter> ANoirPlayerController::GetMeters(int32 Suspect) const
{
	TArray<FNoirMeter> Out;
	if (!Session || !Suspects.IsValidIndex(Suspect))
	{
		return Out;
	}
	const FLlmSuspectDef* Def = Session->GetCase().FindSuspect(Suspects[Suspect].Id);
	const FLlmSuspectState* St = Session->GetState().Suspects.Find(Suspects[Suspect].Id);
	if (Def && St)
	{
		for (const FLlmMeterDef& M : Def->Meters)
		{
			FNoirMeter Meter;
			Meter.Label = (M.Label.IsEmpty() ? M.Name : M.Label).ToUpper();
			Meter.Value = St->Meter(M.Name);
			Out.Add(Meter);
		}
	}
	return Out;
}

bool ANoirPlayerController::IsThinking(int32 Suspect) const
{
	return Session && Suspects.IsValidIndex(Suspect) && Session->IsBusy(Suspects[Suspect].Id);
}

bool ANoirPlayerController::HasLeft(int32 Suspect) const
{
	if (!Session || !Suspects.IsValidIndex(Suspect))
	{
		return false;
	}
	const FLlmSuspectState* St = Session->GetState().Suspects.Find(Suspects[Suspect].Id);
	return St && St->bClosed;
}

void ANoirPlayerController::SelectSuspect(int32 Index)
{
	if (State != ENoirState::Interrogate || Suspects.IsEmpty())
	{
		return;
	}
	Current = (Index + Suspects.Num()) % Suspects.Num();
	Greet();
}

void ANoirPlayerController::DebugShowSuspect(int32 Index)
{
	if (Suspects.IsValidIndex(Index))
	{
		State = ENoirState::Interrogate;
		Current = Index;
		bTalking = false;
		bNotebook = false;
	}
}
