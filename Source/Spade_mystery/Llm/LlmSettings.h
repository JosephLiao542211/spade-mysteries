// Settings for the local LLM sidecar. Edit in Project Settings > Game > Local LLM, saved to DefaultGame.ini.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "LlmSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Local LLM"))
class SPADE_MYSTERY_API ULlmSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	ULlmSettings() { CategoryName = TEXT("Game"); }

	/** Start llama-server with the game. Turn off to use a server you started by hand (faster prompt iteration in the editor). */
	UPROPERTY(Config, EditAnywhere, Category = "Server")
	bool bLaunchServer = true;

	/** Used only when bLaunchServer is false. */
	UPROPERTY(Config, EditAnywhere, Category = "Server")
	FString ExternalServerUrl = TEXT("http://127.0.0.1:8080");

	/** First port to try; the next 20 are tried if it is busy. */
	UPROPERTY(Config, EditAnywhere, Category = "Server")
	int32 PreferredPort = 39281;

	/** Folders relative to the project directory, filled by Tools/fetch-models.ps1. */
	UPROPERTY(Config, EditAnywhere, Category = "Server")
	FString GpuBuildDir = TEXT("ThirdParty/llama/b12368-vulkan");

	UPROPERTY(Config, EditAnywhere, Category = "Server")
	FString CpuBuildDir = TEXT("ThirdParty/llama/b12368-cpu");

	/** Model for the GPU attempt. */
	UPROPERTY(Config, EditAnywhere, Category = "Server")
	FString GpuModelFile = TEXT("Models/gemma-4-E4B-it-Q4_K_M.gguf");

	/** Model for the CPU fallback (smaller, because CPU generation is slow). */
	UPROPERTY(Config, EditAnywhere, Category = "Server")
	FString CpuModelFile = TEXT("Models/gemma-4-E2B-it-Q4_K_M.gguf");

	UPROPERTY(Config, EditAnywhere, Category = "Server")
	int32 GpuLayers = 99;

	/** Total context; split evenly across ParallelSlots (one slot per suspect). */
	UPROPERTY(Config, EditAnywhere, Category = "Server")
	int32 ContextSize = 8192;

	UPROPERTY(Config, EditAnywhere, Category = "Server")
	int32 ParallelSlots = 4;

	/** Seconds to wait for /health before falling back to CPU, then failing. */
	UPROPERTY(Config, EditAnywhere, Category = "Server")
	float HealthTimeoutSeconds = 120.f;

	UPROPERTY(Config, EditAnywhere, Category = "Chat")
	int32 MaxTokens = 120;

	UPROPERTY(Config, EditAnywhere, Category = "Chat")
	float Temperature = 0.8f;

	UPROPERTY(Config, EditAnywhere, Category = "Chat")
	int32 MaxPlayerChars = 300;

	/** When history reaches HistoryMax exchanges, cut it to the last HistoryKeep in one go (keeps the prompt cache valid between cuts). */
	UPROPERTY(Config, EditAnywhere, Category = "Chat")
	int32 HistoryMax = 16;

	UPROPERTY(Config, EditAnywhere, Category = "Chat")
	int32 HistoryKeep = 8;
};
