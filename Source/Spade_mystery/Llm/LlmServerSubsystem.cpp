#include "Llm/LlmServerSubsystem.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "IPAddress.h"
#include "Llm/LlmSettings.h"
#include "Misc/Paths.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogLlmServer, Log, All);

void ULlmServerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const ULlmSettings* Settings = GetDefault<ULlmSettings>();
	if (!Settings->bLaunchServer)
	{
		BaseUrl = Settings->ExternalServerUrl;
		UE_LOG(LogLlmServer, Log, TEXT("Using external llama-server at %s"), *BaseUrl);
		SetState(ELlmServerState::Starting);
		LaunchTime = FPlatformTime::Seconds();
		HealthTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ULlmServerSubsystem::TickHealth), 0.5f);
		return;
	}

	if (!LaunchServer(/*bCpu*/ false))
	{
		OnLaunchAttemptFailed(TEXT("GPU build did not start"));
	}
}

void ULlmServerSubsystem::Deinitialize()
{
	StopServer();
	Super::Deinitialize();
}

bool ULlmServerSubsystem::LaunchServer(bool bCpu)
{
	const ULlmSettings* Settings = GetDefault<ULlmSettings>();
	bCpuFallback = bCpu;

	const FString BuildDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / (bCpu ? Settings->CpuBuildDir : Settings->GpuBuildDir));
	const FString Exe = BuildDir / TEXT("llama-server.exe");
	const FString Model = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / (bCpu ? Settings->CpuModelFile : Settings->GpuModelFile));
	const FString LogFile = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Logs") / (bCpu ? TEXT("llama-server-cpu.log") : TEXT("llama-server.log")));

	if (!FPaths::FileExists(Exe) || !FPaths::FileExists(Model))
	{
		LastError = FString::Printf(TEXT("Missing %s or %s. Run Tools/fetch-models.ps1."), *Exe, *Model);
		UE_LOG(LogLlmServer, Error, TEXT("%s"), *LastError);
		return false;
	}

	const int32 Port = FindFreePort(Settings->PreferredPort);
	if (Port <= 0)
	{
		LastError = TEXT("No free local port");
		return false;
	}
	BaseUrl = FString::Printf(TEXT("http://127.0.0.1:%d"), Port);

	// Leave two cores for the game thread and render thread on CPU-only machines.
	const int32 Threads = FMath::Max(2, FPlatformMisc::NumberOfCores() - 2);

	// --reasoning off --reasoning-budget 0: Gemma 4 otherwise sometimes opens a "thought" block on its own,
	// which uses up max_tokens and returns an empty reply. In the harness, "off" alone was 11/15 valid; with
	// budget 0 it was 15/15.
	const FString Args = FString::Printf(
		TEXT("-m \"%s\" --host 127.0.0.1 --port %d -ngl %d -c %d --parallel %d -t %d ")
		TEXT("-fa on -ctk q8_0 -ctv q8_0 --jinja --no-webui --reasoning off --reasoning-budget 0 --log-file \"%s\""),
		*Model, Port, bCpu ? 0 : Settings->GpuLayers, Settings->ContextSize, Settings->ParallelSlots, Threads, *LogFile);

	UE_LOG(LogLlmServer, Log, TEXT("Launching %s %s"), *Exe, *Args);

	uint32 Pid = 0;
	Proc = FPlatformProcess::CreateProc(*Exe, *Args, /*bLaunchDetached*/ false, /*bLaunchHidden*/ true,
		/*bLaunchReallyHidden*/ true, &Pid, /*PriorityModifier*/ 0, *BuildDir, nullptr, nullptr);

	if (!Proc.IsValid())
	{
		LastError = TEXT("CreateProc failed");
		return false;
	}

#if PLATFORM_WINDOWS
	// Kill the server even if the game crashes: closing the last handle to this job ends its processes.
	HANDLE Job = ::CreateJobObjectW(nullptr, nullptr);
	if (Job)
	{
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION Info = {};
		Info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
		::SetInformationJobObject(Job, JobObjectExtendedLimitInformation, &Info, sizeof(Info));
		if (!::AssignProcessToJobObject(Job, Proc.Get()))
		{
			UE_LOG(LogLlmServer, Warning, TEXT("AssignProcessToJobObject failed (%u); server may outlive a crash"), ::GetLastError());
		}
		JobHandle = Job;
	}
#endif

	SetState(ELlmServerState::Starting);
	LaunchTime = FPlatformTime::Seconds();
	if (!HealthTicker.IsValid())
	{
		HealthTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ULlmServerSubsystem::TickHealth), 0.5f);
	}
	return true;
}

void ULlmServerSubsystem::StopServer()
{
	if (HealthTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(HealthTicker);
		HealthTicker.Reset();
	}
	if (Proc.IsValid())
	{
		FPlatformProcess::TerminateProc(Proc, /*KillTree*/ true);
		FPlatformProcess::CloseProc(Proc);
		Proc.Reset();
	}
#if PLATFORM_WINDOWS
	if (JobHandle)
	{
		::CloseHandle(static_cast<HANDLE>(JobHandle));
		JobHandle = nullptr;
	}
#endif
	SetState(ELlmServerState::Stopped);
}

bool ULlmServerSubsystem::TickHealth(float DeltaTime)
{
	if (State != ELlmServerState::Starting)
	{
		HealthTicker.Reset();
		return false; // stop ticking
	}

	if (Proc.IsValid() && !FPlatformProcess::IsProcRunning(Proc))
	{
		HealthTicker.Reset();
		OnLaunchAttemptFailed(TEXT("llama-server exited during startup (see Saved/Logs/llama-server*.log)"));
		return false;
	}

	const ULlmSettings* Settings = GetDefault<ULlmSettings>();
	if (FPlatformTime::Seconds() - LaunchTime > Settings->HealthTimeoutSeconds)
	{
		HealthTicker.Reset();
		OnLaunchAttemptFailed(TEXT("llama-server did not become healthy in time"));
		return false;
	}

	if (!bHealthRequestInFlight)
	{
		bHealthRequestInFlight = true;
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
		Req->SetURL(BaseUrl / TEXT("health"));
		Req->SetVerb(TEXT("GET"));
		Req->SetTimeout(2.f);
		TWeakObjectPtr<ULlmServerSubsystem> WeakThis(this);
		Req->OnProcessRequestComplete().BindLambda([WeakThis](FHttpRequestPtr, FHttpResponsePtr Resp, bool bConnected)
		{
			ULlmServerSubsystem* Self = WeakThis.Get();
			if (!Self) return;
			Self->bHealthRequestInFlight = false;
			// 503 means "loading model"; only 200 means ready.
			if (bConnected && Resp.IsValid() && Resp->GetResponseCode() == 200 && Self->State == ELlmServerState::Starting)
			{
				UE_LOG(LogLlmServer, Log, TEXT("llama-server ready at %s after %.1fs%s"), *Self->BaseUrl,
					FPlatformTime::Seconds() - Self->LaunchTime, Self->bCpuFallback ? TEXT(" (CPU fallback)") : TEXT(""));
				Self->SetState(ELlmServerState::Ready);
			}
		});
		Req->ProcessRequest();
	}
	return true; // keep ticking
}

void ULlmServerSubsystem::OnLaunchAttemptFailed(const FString& Reason)
{
	UE_LOG(LogLlmServer, Warning, TEXT("%s"), *Reason);
	const bool bWasCpu = bCpuFallback;
	const bool bExternal = !GetDefault<ULlmSettings>()->bLaunchServer;
	StopServer();

	if (!bWasCpu && !bExternal)
	{
		UE_LOG(LogLlmServer, Warning, TEXT("Retrying with the CPU build"));
		if (LaunchServer(/*bCpu*/ true))
		{
			return;
		}
	}
	if (LastError.IsEmpty())
	{
		LastError = Reason;
	}
	SetState(ELlmServerState::Failed);
}

void ULlmServerSubsystem::SetState(ELlmServerState NewState)
{
	if (State != NewState)
	{
		State = NewState;
		OnStateChanged.Broadcast(State);
	}
}

int32 ULlmServerSubsystem::FindFreePort(int32 Preferred)
{
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SS)
	{
		return Preferred;
	}
	for (int32 P = Preferred; P < Preferred + 20; ++P)
	{
		FSocket* Sock = SS->CreateSocket(NAME_Stream, TEXT("LlmPortProbe"), false);
		if (!Sock) continue;
		TSharedRef<FInternetAddr> Addr = SS->CreateInternetAddr();
		bool bValidIp = false;
		Addr->SetIp(TEXT("127.0.0.1"), bValidIp);
		Addr->SetPort(P);
		const bool bFree = bValidIp && Sock->Bind(*Addr);
		Sock->Close();
		SS->DestroySocket(Sock);
		if (bFree)
		{
			return P;
		}
	}
	return 0;
}
