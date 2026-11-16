#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "NoirPlayerController.generated.h"

class ANoirRoom;
class UInterrogationSession;
struct FInterrogationReply;
enum class ELlmServerState : uint8;

enum class ENoirLineKind : uint8
{
	Suspect, // what the suspect says
	Player,  // what the detective typed
	Note     // game feedback: clues, notebook entries, bluffs, errors
};

struct FNoirLine
{
	ENoirLineKind Kind = ENoirLineKind::Suspect;
	FString Text;
	double Time = 0.0; // real time it appears (drives the typewriter reveal)
};

/** What the HUD needs to draw one meter bar. */
struct FNoirMeter
{
	FString Label;
	float Value = 0.f; // 0-100
};

struct FNoirSuspect
{
	FString Id;              // suspect id in the case JSON
	FString Name;
	FString Role;
	FString Greeting;        // what they say when you first sit down to talk
	TArray<FNoirLine> Log;   // the conversation so far
	FString LastRead;        // how the last typed line was read ("THREATEN"), shown to the player
	bool bLeftNoted = false; // the "walked out" note has been added to the log
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
	Notebook, // open / close the notebook of statements
	Yes,
	No,
	Restart,
	Mood,     // cycle the shown suspect's video clip (relaxed / angry / sad ...)
	Present0  // must stay last: Present0 + i repeats notebook entry i to the current suspect
};

enum class ENoirState : uint8
{
	Briefing,
	Interrogate,
	ConfirmAccuse,
	Solved,
	Failed
};

/**
 * Game state and input. The case itself (suspects, secrets, meters, endings) lives in
 * Content/Llm/Cases/<Case> and is run by UInterrogationSession; ANoirHUD just draws what is here.
 */
UCLASS()
class SPADE_MYSTERY_API ANoirPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ANoirPlayerController();

	FString GetTitle() const;
	FString GetBriefing() const;
	/** One line about the local model: loading, ready, or why it failed. Empty when ready. */
	FString GetServerStatus() const;

	TArray<FNoirSuspect> Suspects;

	ENoirState State = ENoirState::Briefing;
	int32 Current = 0;
	bool bTalking = false;  // conversation panel open (typing goes to the suspect)
	bool bNotebook = false; // notebook panel open
	FString Input;          // the line being typed
	FString EndingTitle;
	FString EndingText;

	UPROPERTY() TObjectPtr<ANoirRoom> Room;
	UPROPERTY() TObjectPtr<UInterrogationSession> Session;

	/** Statements the player holds, in case order. Index = notebook entry. */
	TArray<FString> NotebookIds() const;
	FString EvidenceName(const FString& EvidenceId) const;
	FString EvidenceText(const FString& EvidenceId) const;
	/** True if the current suspect has already heard this statement. */
	bool HasHeard(const FString& EvidenceId) const;
	TArray<FNoirMeter> GetMeters(int32 Suspect) const;
	bool IsThinking(int32 Suspect) const;
	/** True once the suspect has walked out. */
	bool HasLeft(int32 Suspect) const;

	void DoAction(ENoirAction Action);

	/** Editor tooling: skip to interrogating suspect Index. */
	UFUNCTION(BlueprintCallable, Category = "Noir")
	void DebugShowSuspect(int32 Index);

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	void StartCase();
	void HandleKey(FKey Key, bool bRepeat);
	void Submit();
	void PresentEntry(int32 Index);
	void Accuse();
	void SelectSuspect(int32 Index);
	void Greet();
	void OnClick();
	void AddNote(FNoirSuspect& S, const FString& Text, double Time);
	void SetMoodFor(int32 Suspect, const FString& Emotion);
	void HandleReply(const FInterrogationReply& Reply);
	void HandleServerState(ELlmServerState NewState);

	FKey TalkOpener;
};
