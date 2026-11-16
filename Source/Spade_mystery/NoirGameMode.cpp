#include "NoirGameMode.h"
#include "NoirHUD.h"
#include "NoirPlayerController.h"

ANoirGameMode::ANoirGameMode()
{
	PlayerControllerClass = ANoirPlayerController::StaticClass();
	HUDClass = ANoirHUD::StaticClass();
	DefaultPawnClass = nullptr;
}
