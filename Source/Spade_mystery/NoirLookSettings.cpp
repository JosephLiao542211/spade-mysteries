#include "NoirLookSettings.h"

void UNoirLookSettings::SetSuspectCharacter(int32 Index, TSoftClassPtr<AActor> Character)
{
	UNoirLookSettings* Settings = GetMutableDefault<UNoirLookSettings>();
	FNoirSuspectSlot* Slot = const_cast<FNoirSuspectSlot*>(Settings->GetSuspectSlot(Index));
	if (!Slot)
	{
		return;
	}
	Slot->Character = Character;
#if WITH_EDITOR
	Settings->TryUpdateDefaultConfigFile();
#endif
}

void UNoirLookSettings::SaveNoirLook()
{
#if WITH_EDITOR
	GetMutableDefault<UNoirLookSettings>()->TryUpdateDefaultConfigFile();
#endif
}
