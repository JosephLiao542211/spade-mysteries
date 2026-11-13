#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "NoirLookSettings.h"
#include "NoirPoseAnimInstance.generated.h"

/**
 * Evaluates the seated pose on the worker thread: proportions, hips into the chair, slow lean,
 * head tilt, breathing, and two-bone IK for the elbow-on-desk arm and the cigarette-holder arm.
 */
struct FNoirPoseProxy : public FAnimInstanceProxy
{
	FNoirPoseProxy() = default;
	explicit FNoirPoseProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

	FNoirSeatedPose Pose;
	double Time = 0.0;
};

/** Anim instance put on a MetaHuman's Body by ANoirRoom when its suspect slot is "Seated". No asset needed. */
UCLASS(Transient, NotBlueprintable)
class SPADE_MYSTERY_API UNoirPoseAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Which Noir Look suspect slot to read the pose settings from. */
	int32 SuspectIndex = 0;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FNoirPoseProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete InProxy; }
};
