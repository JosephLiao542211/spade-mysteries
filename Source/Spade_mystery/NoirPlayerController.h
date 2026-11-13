#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NoirPlayerController.generated.h"

class ANoirRoom;

struct FNoirLine
{
	bool bPlayer = false; // true = the detective, false = the suspect
	FString Text;
	double Time = 0.0;    // real time it was said (drives the typewriter reveal)
};

struct FNoirSuspect
{
	FString Name;
	FString Role;
	FString Greeting;        // what they say when you first sit down to talk
	TArray<FString> Answers; // one per question in GetQuestions()
	TArray<FNoirLine> Log;   // the conversation so far
};

/** Everything the player can do. Keys and clickable HUD buttons both map to these. */
enum class ENoirAction : uint8
{
	None,
	Begin,
	Talk,     // open / close the conversation panel
	Prev,
	Next,
	Accuse,
	Ask1,
	Ask2,
	Ask3,
	Yes,
	No,
	Restart
};

enum class ENoirState : uint8
{
	Briefing,
	Interrogate,
	ConfirmAccuse,
	Solved,
	Failed
};

/** Holds the whole case + game state and handles keyboard input. ANoirHUD just draws it. */
UCLASS()
class SPADE_MYSTERY_API ANoirPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ANoirPlayerController();

	static const TArray<FString>& GetQuestions();
	static FString GetBriefing();
	FString GetEnding() const;

	TArray<FNoirSuspect> Suspects;
	int32 Culprit = 1;

	ENoirState State = ENoirState::Briefing;
	int32 Current = 0;
	bool bTalking = false;  // conversation panel open
	TSet<int32> AskedKeys; // Suspect * 100 + Question

	UPROPERTY() TObjectPtr<ANoirRoom> Room;

	bool WasAsked(int32 Suspect, int32 Question) const { return AskedKeys.Contains(Suspect * 100 + Question); }

	void DoAction(ENoirAction Action);

	/** Editor tooling: skip to interrogating suspect Index (0-2). */
	UFUNCTION(BlueprintCallable, Category = "Noir")
	void DebugShowSuspect(int32 Index);

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	void Ask(int32 Question);
	void SelectSuspect(int32 Index);
	void Greet();
	void OnClick();
};
