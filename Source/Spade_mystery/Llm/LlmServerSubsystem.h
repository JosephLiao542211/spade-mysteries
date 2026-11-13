// Starts, watches and stops the llama-server sidecar.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformProcess.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LlmServerSubsystem.generated.h"

UENUM(BlueprintType)
enum class ELlmServerState : uint8
{
	Stopped,
	Starting,
	Ready,
	Failed
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnLlmServerStateChanged, ELlmServerState /*NewState*/);

UCLASS()
class SPADE_MYSTERY_API ULlmServerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	ELlmServerState GetState() const { return State; }
	bool IsReady() const { return State == ELlmServerState::Ready; }

	/** e.g. http://127.0.0.1:39281 */
	FString GetBaseUrl() const { return BaseUrl; }

	/** True when the GPU attempt failed and the CPU build is running. */
	bool IsCpuFallback() const { return bCpuFallback; }

	/** Human-readable reason when State is Failed. */
	const FString& GetLastError() const { return LastError; }

	FOnLlmServerStateChanged OnStateChanged;

private:
	bool LaunchServer(bool bCpu);
	void StopServer();
	bool TickHealth(float DeltaTime);
	void OnLaunchAttemptFailed(const FString& Reason);
	void SetState(ELlmServerState NewState);
	static int32 FindFreePort(int32 Preferred);

	ELlmServerState State = ELlmServerState::Stopped;
	FString BaseUrl;
	FString LastError;
	bool bCpuFallback = false;
	bool bHealthRequestInFlight = false;
	double LaunchTime = 0.0;

	FProcHandle Proc;
	void* JobHandle = nullptr; // Windows job object: kills the server if the game dies.
	FTSTicker::FDelegateHandle HealthTicker;
};
