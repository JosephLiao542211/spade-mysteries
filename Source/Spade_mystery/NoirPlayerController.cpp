#include "NoirPlayerController.h"
#include "NoirRoom.h"
#include "NoirHUD.h"
#include "Components/InputComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"

ANoirPlayerController::ANoirPlayerController()
{
	bShowMouseCursor = true; // buttons are clickable; keys work too

	// ---- The case. Edit these to write your own mystery. ----
	{
		FNoirSuspect& S = Suspects.AddDefaulted_GetRef();
		S.Name = TEXT("Vera Lane");
		S.Role = TEXT("Torch singer, the Black Spade Club");
		S.Greeting = TEXT("Make it quick, detective. I've got a second set at one, and I'd rather sing to an empty room than to you.");
		S.Answers = {
			TEXT("On stage, from half past ten till midnight. A hundred people were looking right at me, detective. Ask any of them."),
			TEXT("Ace owned my contract. He liked to think that meant he owned me. It didn't."),
			TEXT("The lights died at eleven. The band kept playing in the dark. When they came back on, Mickey wasn't behind the bar.")
		};
	}
	{
		FNoirSuspect& S = Suspects.AddDefaulted_GetRef();
		S.Name = TEXT("Mickey Doyle");
		S.Role = TEXT("Bartender, twelve years at the club");
		S.Greeting = TEXT("Get you something, detective? On the house. Tonight everything's on the house.");
		S.Answers = {
			TEXT("Behind the bar, pal. All night. Never left it once. Somebody's gotta pour the drinks."),
			TEXT("Best boss I ever had. Paid on time, mostly. Who'd want to hurt a guy like that?"),
			TEXT("Mr. Graves went up to the office around half ten. Came back down looking like he'd swallowed a lemon. If I were you, I'd start there.")
		};
	}
	{
		FNoirSuspect& S = Suspects.AddDefaulted_GetRef();
		S.Name = TEXT("Harlan Graves");
		S.Role = TEXT("Ace's business partner");
		S.Greeting = TEXT("Sit down if you must. My lawyer will hear every word of this.");
		S.Answers = {
			TEXT("I left his office at a quarter to eleven. He was alive and shouting at me. Then I sat at my table and drank until you people showed up."),
			TEXT("Partners. He was cooking the books and I'd found out. I wanted him in court, not in the ground. A dead man can't pay me back."),
			TEXT("When the lights went out I heard someone on the back stairs, the ones up to the office. Only staff carry a key to that door.")
		};
	}
	Culprit = 1; // Mickey: says he never left the bar, but Vera saw the empty bar and Graves heard a staff member on the back stairs.
}

const TArray<FString>& ANoirPlayerController::GetQuestions()
{
	static const TArray<FString> Questions = {
		TEXT("Where were you at eleven?"),
		TEXT("What was Ace to you?"),
		TEXT("See anything strange tonight?")
	};
	return Questions;
}

FString ANoirPlayerController::GetBriefing()
{
	return TEXT("11:02 PM. Jimmy \"Ace\" Calloway, owner of the Black Spade Club, is found shot dead in his upstairs office. "
		"The lights went out at eleven sharp. Nobody heard the gun over the band. "
		"Three people had a reason to want him gone. Sit them under the lamp, listen close, and name the killer.");
}

FString ANoirPlayerController::GetEnding() const
{
	if (State == ENoirState::Solved)
	{
		return TEXT("Mickey stops polishing a glass that isn't there. \"Twelve years. And he was gonna throw me out like the trash.\" "
			"When the lights died, he left the bar, took the back stairs with his staff key, and was pouring again before anyone missed him. "
			"Case closed.");
	}
	return FString::Printf(TEXT("You put the cuffs on %s. The papers love it. "
		"Three weeks later a bartender skips town with a revolver in his luggage. "
		"Some cases close. Some just stop."), *Suspects[Current].Name);
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
}

void ANoirPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	auto Bind = [this](const FKey& Key, ENoirAction Action)
	{
		FInputKeyBinding Binding(FInputChord(Key), IE_Pressed);
		Binding.KeyDelegate.GetDelegateForManualSet().BindWeakLambda(this, [this, Action]() { DoAction(Action); });
		InputComponent->KeyBindings.Add(Binding);
	};
	Bind(EKeys::SpaceBar, ENoirAction::Begin);
	Bind(EKeys::Enter, ENoirAction::Begin);
	Bind(EKeys::T, ENoirAction::Talk);
	Bind(EKeys::One, ENoirAction::Ask1);
	Bind(EKeys::Two, ENoirAction::Ask2);
	Bind(EKeys::Three, ENoirAction::Ask3);
	Bind(EKeys::Right, ENoirAction::Next);
	Bind(EKeys::D, ENoirAction::Next);
	Bind(EKeys::Left, ENoirAction::Prev);
	Bind(EKeys::A, ENoirAction::Prev);
	Bind(EKeys::X, ENoirAction::Accuse);
	Bind(EKeys::Y, ENoirAction::Yes);
	Bind(EKeys::N, ENoirAction::No);
	Bind(EKeys::R, ENoirAction::Restart);

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

void ANoirPlayerController::DoAction(ENoirAction Action)
{
	switch (Action)
	{
	case ENoirAction::Begin:
		if (State == ENoirState::Briefing)
		{
			State = ENoirState::Interrogate;
		}
		break;
	case ENoirAction::Talk:
		if (State == ENoirState::Interrogate)
		{
			bTalking = !bTalking;
			Greet();
		}
		break;
	case ENoirAction::Prev:   SelectSuspect(Current - 1); break;
	case ENoirAction::Next:   SelectSuspect(Current + 1); break;
	case ENoirAction::Ask1:   Ask(0); break;
	case ENoirAction::Ask2:   Ask(1); break;
	case ENoirAction::Ask3:   Ask(2); break;
	case ENoirAction::Accuse:
		if (State == ENoirState::Interrogate)
		{
			State = ENoirState::ConfirmAccuse;
			bTalking = false;
		}
		break;
	case ENoirAction::Yes:
		if (State == ENoirState::ConfirmAccuse)
		{
			State = (Current == Culprit) ? ENoirState::Solved : ENoirState::Failed;
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
			AskedKeys.Reset();
			for (FNoirSuspect& S : Suspects)
			{
				S.Log.Reset();
			}
		}
		break;
	default:
		break;
	}
}

void ANoirPlayerController::Greet()
{
	FNoirSuspect& S = Suspects[Current];
	if (bTalking && S.Log.IsEmpty() && !S.Greeting.IsEmpty())
	{
		S.Log.Add({ false, S.Greeting, GetWorld()->GetRealTimeSeconds() });
	}
}

void ANoirPlayerController::Ask(int32 Question)
{
	FNoirSuspect& S = Suspects[Current];
	if (State != ENoirState::Interrogate || !bTalking || !GetQuestions().IsValidIndex(Question) || !S.Answers.IsValidIndex(Question))
	{
		return;
	}
	const double Now = GetWorld()->GetRealTimeSeconds();
	const bool bRepeat = WasAsked(Current, Question);
	S.Log.Add({ true, GetQuestions()[Question], Now });
	S.Log.Add({ false, bRepeat ? TEXT("I already told you. ") + S.Answers[Question] : S.Answers[Question], Now });
	AskedKeys.Add(Current * 100 + Question);
}

void ANoirPlayerController::SelectSuspect(int32 Index)
{
	if (State != ENoirState::Interrogate)
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
	}
}
