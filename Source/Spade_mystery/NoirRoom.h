#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NoirRoom.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneCaptureComponent2D;
class UAnimationAsset;
class UExponentialHeightFogComponent;
class USpotLightComponent;
class UStaticMesh;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UTextureRenderTarget2D;

/**
 * The interrogation room, built entirely from engine primitive shapes at runtime.
 * Owns a low-res scene capture; ANoirHUD reads it back and dithers it to 1-bit.
 */
UCLASS()
class SPADE_MYSTERY_API ANoirRoom : public AActor
{
	GENERATED_BODY()

public:
	ANoirRoom();

	/** Shows suspect Index in the chair (INDEX_NONE = empty chair). */
	void ShowSuspect(int32 Index);

	/** Renders the scene into the capture target right now. */
	void Capture();

	UTextureRenderTarget2D* GetCaptureTarget() const { return CaptureTarget; }

	/** 16:9 resolution derived from UNoirLookSettings::VerticalResolution. */
	static FIntPoint DesiredResolution();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	enum class EShape : uint8 { Cube, Sphere, Cylinder, Cone };

	UStaticMeshComponent* AddShape(EShape Shape, const FVector& Location, const FVector& SizeCm, float Shade,
		USceneComponent* Parent = nullptr, const FRotator& Rotation = FRotator::ZeroRotator);
	void BuildRoom();
	void BuildSuspects();

	UPROPERTY() TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> SphereMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> CylinderMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> ConeMesh;
	UPROPERTY() TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY() TObjectPtr<USceneComponent> TableRoot;
	UPROPERTY() TObjectPtr<USceneComponent> ChairRoot;
	UPROPERTY() TObjectPtr<USceneComponent> MicRoot;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> MicPole;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> MicHead;
	UPROPERTY() TObjectPtr<USceneComponent> LampPivot;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> LampCord;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> LampShade;
	UPROPERTY() TObjectPtr<USpotLightComponent> LampLight;
	UPROPERTY() TObjectPtr<USpotLightComponent> StreetLight;
	UPROPERTY() TObjectPtr<USpotLightComponent> RimLight;
	UPROPERTY() TObjectPtr<UExponentialHeightFogComponent> Fog;
	UPROPERTY() TObjectPtr<USceneComponent> CameraRig;
	UPROPERTY() TObjectPtr<USceneCaptureComponent2D> CaptureComponent;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> CaptureTarget;

	/** One root per suspect; all figure parts hang off it. */
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> SuspectRoots;

	void ApplySettings(float Time);
	void SyncSuspectCharacters();
	void RefreshVisibility();
	void UpdateHolder(int32 Index, AActor* Real, USkeletalMeshComponent* Body, const struct FNoirSeatedPose& Pose);
	static void DefaultPerformance(int32 Index, TSoftObjectPtr<UAnimationAsset>& OutBody, TSoftObjectPtr<UAnimationAsset>& OutFace);

	/** Spawned real characters per suspect (null = use shape figure). */
	UPROPERTY() TArray<TObjectPtr<AActor>> SuspectActors;
	/** Cigarette holder parts per suspect: [i*2] = holder, [i*2+1] = lit tip. */
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Holders;
	TArray<FSoftObjectPath> SuspectActorClassPaths;
	TArray<FString> SuspectAnimPaths; // "body|face" anim paths last applied
	int32 ShownIndex = INDEX_NONE;
	TArray<FVector> CenterFix;     // auto-centering correction per suspect
	TArray<int32> SettleFrames;    // frames left to auto-centre after spawn / clip change
	TArray<float> HeadHeights;     // head bone height per suspect (room space)
	float SmoothedHead = 0.f;
};
