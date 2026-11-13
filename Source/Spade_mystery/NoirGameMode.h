#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NoirGameMode.generated.h"

/** Wires up the interrogation controller + HUD. No pawn, no level content needed. */
UCLASS()
class SPADE_MYSTERY_API ANoirGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANoirGameMode();
};
