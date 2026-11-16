#include "NoirPoseAnimInstance.h"
#include "Animation/AnimNodeBase.h"
#include "BonePose.h"
#include "Engine/World.h"
#include "TwoBoneIK.h"

void FNoirPoseProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

	const UNoirPoseAnimInstance* Instance = Cast<UNoirPoseAnimInstance>(InAnimInstance);
	const int32 Index = Instance ? Instance->SuspectIndex : 0;
	if (const FNoirSuspectSlot* Slot = UNoirLookSettings::Get().GetSuspectSlot(Index))
	{
		Pose = Slot->Pose;
	}
	const UWorld* World = InAnimInstance->GetWorld();
	Time = (World ? World->GetTimeSeconds() : 0.0) + Index * 3.7; // de-sync suspects
}

namespace
{
	/** Smooth 0..1..0 weight for "holder at her lips" within one smoke cycle. */
	float SmokeWeight(double Time, float Period, float Hold)
	{
		const float P = FMath::Frac(float(Time / FMath::Max(Period, 1.f)));
		const float Rise = 0.12f, Fall = 0.16f;
		Hold = FMath::Clamp(Hold, 0.f, 1.f - Rise - Fall);
		if (P < Rise) return FMath::SmoothStep(0.f, Rise, P);
		if (P < Rise + Hold) return 1.f;
		if (P < Rise + Hold + Fall) return 1.f - FMath::SmoothStep(Rise + Hold, Rise + Hold + Fall, P);
		return 0.f;
	}
}

bool FNoirPoseProxy::Evaluate(FPoseContext& Output)
{
	Output.ResetToRefPose();
	FCompactPose& Local = Output.Pose;
	const FBoneContainer& Bones = Local.GetBoneContainer();

	auto Find = [&Bones](const TCHAR* Name) -> FCompactPoseBoneIndex
	{
		const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(FName(Name));
		return MeshIndex == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
	};
	const FCompactPoseBoneIndex Pelvis = Find(TEXT("pelvis"));
	const FCompactPoseBoneIndex Spine1 = Find(TEXT("spine_01"));
	const FCompactPoseBoneIndex Spine3 = Find(TEXT("spine_03"));
	const FCompactPoseBoneIndex Spine5 = Find(TEXT("spine_05"));
	const FCompactPoseBoneIndex Neck1 = Find(TEXT("neck_01"));
	const FCompactPoseBoneIndex Neck2 = Find(TEXT("neck_02"));
	const FCompactPoseBoneIndex Head = Find(TEXT("head"));
	const FCompactPoseBoneIndex ClavL = Find(TEXT("clavicle_l")), ClavR = Find(TEXT("clavicle_r"));
	const FCompactPoseBoneIndex UpL = Find(TEXT("upperarm_l")), UpR = Find(TEXT("upperarm_r"));
	const FCompactPoseBoneIndex LowL = Find(TEXT("lowerarm_l")), LowR = Find(TEXT("lowerarm_r"));
	const FCompactPoseBoneIndex HandL = Find(TEXT("hand_l")), HandR = Find(TEXT("hand_r"));
	if (!Pelvis.IsValid() || !Head.IsValid() || !UpL.IsValid() || !UpR.IsValid() || !LowL.IsValid() || !LowR.IsValid() || !HandL.IsValid() || !HandR.IsValid())
	{
		return true; // not a MetaHuman-style skeleton: leave the rest pose
	}

	const float S = FMath::Clamp(Pose.PoseStrength, 0.f, 1.f);
	auto Amount = [S](float Full) { return FMath::Lerp(1.f, Full, S); };

	// 1. Exaggerated proportions (local space: lengthen bones by pushing children out).
	auto Stretch = [&Local](FCompactPoseBoneIndex I, float F) { if (I.IsValid()) { Local[I].SetTranslation(Local[I].GetTranslation() * F); } };
	Local[Head].SetScale3D(Local[Head].GetScale3D() * Amount(Pose.HeadScale));
	Stretch(Neck2, Amount(Pose.NeckLength));
	Stretch(Head, Amount(Pose.NeckLength));
	Stretch(UpL, Amount(Pose.ShoulderWidth));
	Stretch(UpR, Amount(Pose.ShoulderWidth));
	Stretch(LowL, Amount(Pose.ArmLength));
	Stretch(LowR, Amount(Pose.ArmLength));
	Stretch(HandL, Amount(Pose.ArmLength));
	Stretch(HandR, Amount(Pose.ArmLength));

	// 2. Component space, with her own forward/right/up worked out from the skeleton.
	FCSPose<FCompactPose> CS;
	CS.InitPose(Local);
	const FVector Up = FVector::UpVector;
	const FVector Right = FVector::VectorPlaneProject(CS.GetComponentSpaceTransform(UpR).GetLocation() - CS.GetComponentSpaceTransform(UpL).GetLocation(), Up).GetSafeNormal();
	const FVector Forward = FVector::CrossProduct(Right, Up);
	auto Rel = [&](const FVector& V) { return Forward * V.X + Right * V.Y + Up * V.Z; };

	auto SetCS = [&CS](FCompactPoseBoneIndex I, const FTransform& T)
	{
		const FBoneTransform BT(I, T);
		CS.LocalBlendCSBoneTransforms(MakeArrayView(&BT, 1), 1.f);
	};
	// Rotate a bone (and everything below it) about Axis through the bone's own pivot.
	auto RotateCS = [&](FCompactPoseBoneIndex I, const FVector& Axis, float Degrees)
	{
		if (!I.IsValid() || FMath::IsNearlyZero(Degrees)) return;
		FTransform T = CS.GetComponentSpaceTransform(I);
		T.SetRotation(FQuat(Axis, FMath::DegreesToRadians(Degrees)) * T.GetRotation());
		SetCS(I, T);
	};

	// 3. Hips down into the chair.
	{
		FTransform T = CS.GetComponentSpaceTransform(Pelvis);
		T.AddToTranslation(-Up * Pose.SeatDrop * S);
		SetCS(Pelvis, T);
	}

	// 4. Slow lean back and forward, spread through the spine; neck counters so her gaze stays on you.
	const double TwoPi = 2.0 * PI;
	const float Smoke = SmokeWeight(Time, Pose.SmokePeriod, Pose.SmokeHold) * S;
	const float Lean = (Pose.BaseLean + Pose.LeanAmount * float(FMath::Sin(TwoPi * Time / FMath::Max(Pose.LeanPeriod, 1.f))) - 3.f * Smoke) * S;
	// Positive lean = top of the spine toward the camera (Forward).
	const float Sign = (FQuat(Right, 0.1f).RotateVector(Up) | Forward) > 0.f ? 1.f : -1.f;
	RotateCS(Spine1, Right, Sign * Lean * 0.45f);
	RotateCS(Spine3, Right, Sign * Lean * 0.3f);
	RotateCS(Spine5, Right, Sign * Lean * 0.25f);
	RotateCS(Neck1, Right, -Sign * Lean * 0.5f);

	// 5. Breathing in the shoulders.
	const float BreathUp = Pose.Breath * 0.6f * float(FMath::Sin(TwoPi * Time / 4.6)) * S;
	for (FCompactPoseBoneIndex Clav : { ClavL, ClavR })
	{
		if (Clav.IsValid())
		{
			FTransform T = CS.GetComponentSpaceTransform(Clav);
			T.AddToTranslation(Up * BreathUp);
			SetCS(Clav, T);
		}
	}

	// 6. Head: coy side tilt, a lazy sway, and tipping back a touch while she draws on the holder.
	RotateCS(Head, Forward, (Pose.HeadTilt + 1.5f * float(FMath::Sin(TwoPi * Time / 7.3))) * S);
	RotateCS(Head, Right, -Sign * 5.f * Smoke);

	// 7. Arms by two-bone IK.
	auto SolveArm = [&](FCompactPoseBoneIndex A, FCompactPoseBoneIndex B, FCompactPoseBoneIndex C, const FVector& Effector, const FVector& Pole)
	{
		FTransform TA = CS.GetComponentSpaceTransform(A);
		FTransform TB = CS.GetComponentSpaceTransform(B);
		FTransform TC = CS.GetComponentSpaceTransform(C);
		const FVector Target = FMath::Lerp(TC.GetLocation(), Effector, S);
		const FVector Joint = FMath::Lerp(TB.GetLocation(), Pole, S);
		AnimationCore::SolveTwoBoneIK(TA, TB, TC, Joint, Target, false, 1.0, 1.0);
		const FBoneTransform Out[] = { FBoneTransform(A, TA), FBoneTransform(B, TB), FBoneTransform(C, TC) };
		CS.LocalBlendCSBoneTransforms(Out, 1.f);
	};
	const FVector HeadPos = CS.GetComponentSpaceTransform(Head).GetLocation();
	const FVector ShoulderL = CS.GetComponentSpaceTransform(UpL).GetLocation();
	const FVector ShoulderR = CS.GetComponentSpaceTransform(UpR).GetLocation();

	// Left: elbow planted on the desk, hand up under her chin.
	SolveArm(UpL, LowL, HandL, HeadPos + Rel(Pose.LeftHand), ShoulderL + Rel(Pose.LeftElbow));

	// Right: holder hand drifts slowly between rest and her lips.
	const FVector Rest = ShoulderR + Rel(Pose.RightHandRest);
	const FVector AtLips = HeadPos + Rel(Pose.RightHandSmoke);
	SolveArm(UpR, LowR, HandR, FMath::Lerp(Rest, AtLips, Smoke), ShoulderR + Rel(Pose.RightElbow));

	Local.CopyBonesFrom(CS.GetPose());
	return true;
}
