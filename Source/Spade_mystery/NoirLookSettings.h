#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NoirLookSettings.generated.h"

class UAnimationAsset;

/**
 * Procedural seated performance for a MetaHuman (see UNoirPoseAnimInstance).
 * Hand targets are in cm relative to the character: Forward = toward the camera, Right = her right, Up = up.
 */
USTRUCT()
struct FNoirSeatedPose
{
	GENERATED_BODY()

	/** 0 = her default rest pose, 1 = full pose. Slide it to see what the pose is doing. */
	UPROPERTY(EditAnywhere, Category = "Pose", meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float PoseStrength = 1.f;

	/** How far her hips drop into the chair, in cm. */
	UPROPERTY(EditAnywhere, Category = "Pose|Seat", meta = (UIMin = "0", UIMax = "70", Units = "cm"))
	float SeatDrop = 42.f;

	/** Resting forward(+)/back(-) tilt of her upper body, in degrees. */
	UPROPERTY(EditAnywhere, Category = "Pose|Seat", meta = (UIMin = "-30", UIMax = "30"))
	float BaseLean = 6.f;

	/** Head tilt to the side, in degrees (a coy tilt). */
	UPROPERTY(EditAnywhere, Category = "Pose|Seat", meta = (UIMin = "-30", UIMax = "30"))
	float HeadTilt = 8.f;

	// --- Left arm: elbow on the desk, hand up by her chin ---

	/** Left hand position relative to her head. */
	UPROPERTY(EditAnywhere, Category = "Pose|Left Arm (elbow on desk)", meta = (UIMin = "-60", UIMax = "60", Units = "cm"))
	FVector LeftHand = FVector(16.f, -4.f, -16.f);

	/** Where the left elbow points (relative to her left shoulder); keep it down on the desk. */
	UPROPERTY(EditAnywhere, Category = "Pose|Left Arm (elbow on desk)", meta = (UIMin = "-80", UIMax = "80", Units = "cm"))
	FVector LeftElbow = FVector(30.f, 4.f, -40.f);

	// --- Right arm: holds the cigarette holder ---

	/** Right hand at rest, relative to her right shoulder. */
	UPROPERTY(EditAnywhere, Category = "Pose|Right Arm (cigarette)", meta = (UIMin = "-60", UIMax = "60", Units = "cm"))
	FVector RightHandRest = FVector(28.f, 10.f, -12.f);

	/** Right hand while drawing on the holder, relative to her head. */
	UPROPERTY(EditAnywhere, Category = "Pose|Right Arm (cigarette)", meta = (UIMin = "-60", UIMax = "60", Units = "cm"))
	FVector RightHandSmoke = FVector(26.f, 10.f, -4.f);

	/** Where the right elbow points (relative to her right shoulder). */
	UPROPERTY(EditAnywhere, Category = "Pose|Right Arm (cigarette)", meta = (UIMin = "-80", UIMax = "80", Units = "cm"))
	FVector RightElbow = FVector(5.f, 25.f, -35.f);

	// --- Slow, relaxed motion ---

	/** Seconds for one slow lean back and forward. */
	UPROPERTY(EditAnywhere, Category = "Pose|Motion", meta = (UIMin = "3", UIMax = "30", Units = "s"))
	float LeanPeriod = 11.f;

	/** How far she sways back and forward, in degrees. */
	UPROPERTY(EditAnywhere, Category = "Pose|Motion", meta = (UIMin = "0", UIMax = "20"))
	float LeanAmount = 7.f;

	/** Seconds between draws on the cigarette holder. */
	UPROPERTY(EditAnywhere, Category = "Pose|Motion", meta = (UIMin = "4", UIMax = "40", Units = "s"))
	float SmokePeriod = 16.f;

	/** Fraction of the smoke cycle the holder is at her lips (0.1-0.5). */
	UPROPERTY(EditAnywhere, Category = "Pose|Motion", meta = (UIMin = "0.05", UIMax = "0.6"))
	float SmokeHold = 0.22f;

	/** Gentle breathing in the shoulders. */
	UPROPERTY(EditAnywhere, Category = "Pose|Motion", meta = (UIMin = "0", UIMax = "3"))
	float Breath = 1.f;

	// --- Exaggerated proportions (stylised film-star look) ---

	UPROPERTY(EditAnywhere, Category = "Pose|Proportions", meta = (UIMin = "0.8", UIMax = "1.4"))
	float HeadScale = 1.06f;

	/** Long, elegant neck. */
	UPROPERTY(EditAnywhere, Category = "Pose|Proportions", meta = (UIMin = "0.8", UIMax = "1.8"))
	float NeckLength = 1.3f;

	/** Long arms and fingers. */
	UPROPERTY(EditAnywhere, Category = "Pose|Proportions", meta = (UIMin = "0.8", UIMax = "1.4"))
	float ArmLength = 1.08f;

	/** Narrow (<1) or broad (>1) shoulders. */
	UPROPERTY(EditAnywhere, Category = "Pose|Proportions", meta = (UIMin = "0.7", UIMax = "1.3"))
	float ShoulderWidth = 0.92f;

	// --- Cigarette holder prop (attached to her right hand) ---

	UPROPERTY(EditAnywhere, Category = "Pose|Cigarette Holder")
	bool bShowHolder = true;

	UPROPERTY(EditAnywhere, Category = "Pose|Cigarette Holder", meta = (UIMin = "10", UIMax = "45", Units = "cm"))
	float HolderLength = 28.f;

	/** Holder position in the hand bone's space. */
	UPROPERTY(EditAnywhere, Category = "Pose|Cigarette Holder", meta = (UIMin = "-20", UIMax = "20", Units = "cm"))
	FVector HolderOffset = FVector(9.f, 2.f, -3.f);

	/** Holder rotation in the hand bone's space; turn it until it lies between her fingers toward her lips. */
	UPROPERTY(EditAnywhere, Category = "Pose|Cigarette Holder")
	FRotator HolderRotation = FRotator(0.f, 0.f, 90.f);
};

/** A real character (e.g. a MetaHuman Blueprint) to seat in place of a suspect's placeholder figure. */
USTRUCT()
struct FNoirSuspectSlot
{
	GENERATED_BODY()

	/** Actor Blueprint to use (e.g. BP_YourMetaHuman). Empty = keep the simple shape figure. */
	UPROPERTY(EditAnywhere, Category = "Suspect")
	TSoftClassPtr<AActor> Character;

	/** Looping body animation (MetaHuman body skeleton). Leave both animations empty for the built-in default performance. */
	UPROPERTY(EditAnywhere, Category = "Suspect")
	TSoftObjectPtr<UAnimationAsset> IdleAnimation;

	/** Looping face animation (MetaHuman face skeleton), e.g. an Expression_Loops "_f_" clip. */
	UPROPERTY(EditAnywhere, Category = "Suspect")
	TSoftObjectPtr<UAnimationAsset> FaceAnimation;

	/** Nudge from the chair position, in cm (X = away from camera, Y = sideways, Z = up). */
	UPROPERTY(EditAnywhere, Category = "Suspect", meta = (UIMin = "-150", UIMax = "150"))
	FVector Offset = FVector::ZeroVector;

	/** Which way they face. 90 = a MetaHuman looking at the camera. */
	UPROPERTY(EditAnywhere, Category = "Suspect", meta = (ClampMin = "0", ClampMax = "360", UIMin = "0", UIMax = "360"))
	float Yaw = 90.f;

	UPROPERTY(EditAnywhere, Category = "Suspect", meta = (ClampMin = "0.1", ClampMax = "3", UIMin = "0.5", UIMax = "1.5"))
	float Scale = 1.f;

	/** Experimental procedural pose (ignores the animation slots). Off = play the Idle Animation (e.g. retargeted Rokoko mocap). */
	UPROPERTY(EditAnywhere, Category = "Suspect")
	bool bSeated = false;

	UPROPERTY(EditAnywhere, Category = "Suspect", meta = (EditCondition = "bSeated"))
	FNoirSeatedPose Pose;
};

UENUM()
enum class ENoirDitherPattern : uint8
{
	Halftone   UMETA(DisplayName = "Halftone dots"),
	Lines      UMETA(DisplayName = "Halftone lines"),
	Bayer4     UMETA(DisplayName = "Bayer 4x4 (fine)"),
	Bayer8     UMETA(DisplayName = "Bayer 8x8"),
	Threshold  UMETA(DisplayName = "Hard threshold (no dither)")
};

/**
 * Every visual knob for the 1-bit noir look.
 * Edit Project Settings > Game > Noir Look. Changes apply live during Play.
 * Saved to Config/DefaultGame.ini.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Noir Look"))
class SPADE_MYSTERY_API UNoirLookSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UNoirLookSettings& Get() { return *GetDefault<UNoirLookSettings>(); }

	/** Puts a character Blueprint in suspect slot Index (0-2) and saves it to DefaultGame.ini. Used by the editor tooling. */
	UFUNCTION(BlueprintCallable, Category = "Noir")
	static void SetSuspectCharacter(int32 Index, TSoftClassPtr<AActor> Character);

	/** Saves the current Noir Look values to DefaultGame.ini. Used by the editor tooling. */
	UFUNCTION(BlueprintCallable, Category = "Noir")
	static void SaveNoirLook();

	const FNoirSuspectSlot* GetSuspectSlot(int32 Index) const
	{
		switch (Index)
		{
		case 0: return &Suspect1;
		case 1: return &Suspect2;
		case 2: return &Suspect3;
		default: return nullptr;
		}
	}

	// ---------------- Suspects ----------------

	UPROPERTY(Config, EditAnywhere, Category = "Suspects", meta = (DisplayName = "Suspect 1: Vera Lane (singer)"))
	FNoirSuspectSlot Suspect1;

	UPROPERTY(Config, EditAnywhere, Category = "Suspects", meta = (DisplayName = "Suspect 2: Mickey Doyle (bartender)"))
	FNoirSuspectSlot Suspect2;

	UPROPERTY(Config, EditAnywhere, Category = "Suspects", meta = (DisplayName = "Suspect 3: Harlan Graves (partner)"))
	FNoirSuspectSlot Suspect3;

	/** Keep characters at full detail even though the capture is low-res (faces stay sharp). */
	UPROPERTY(Config, EditAnywhere, Category = "Suspects")
	bool bForceHighestDetail = true;
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// ---------------- Pixels ----------------

	/** Vertical resolution of the 1-bit image (width follows 16:9). Lower = chunkier pixels. 360 at 1080p = 3x3 screen pixels each. */
	UPROPERTY(Config, EditAnywhere, Category = "Pixels", meta = (ClampMin = "90", ClampMax = "1080", UIMin = "90", UIMax = "720"))
	int32 VerticalResolution = 360;

	// ---------------- Dither ----------------

	UPROPERTY(Config, EditAnywhere, Category = "Dither")
	ENoirDitherPattern Pattern = ENoirDitherPattern::Halftone;

	/** Size of one halftone dot/line cell, in low-res pixels. */
	UPROPERTY(Config, EditAnywhere, Category = "Dither", meta = (ClampMin = "2", ClampMax = "24", UIMin = "2", UIMax = "16"))
	float CellSize = 6.f;

	/** Rotation of the halftone screen in degrees (45 = classic newspaper). */
	UPROPERTY(Config, EditAnywhere, Category = "Dither", meta = (ClampMin = "0", ClampMax = "90", UIMin = "0", UIMax = "90"))
	float ScreenAngle = 45.f;

	/** How hard mid-tones get pushed to pure black or pure white. */
	UPROPERTY(Config, EditAnywhere, Category = "Dither", meta = (ClampMin = "0.25", ClampMax = "10", UIMin = "0.25", UIMax = "6"))
	float Contrast = 2.4f;

	/** Brightness that sits exactly at 50% dither. Lower = more white, higher = more black. */
	UPROPERTY(Config, EditAnywhere, Category = "Dither", meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float Pivot = 0.42f;

	/** Tone curve before contrast. >1 opens up shadows, <1 crushes them. */
	UPROPERTY(Config, EditAnywhere, Category = "Dither", meta = (ClampMin = "0.2", ClampMax = "4", UIMin = "0.3", UIMax = "3"))
	float Gamma = 2.2f;

	/** Swap black and white. */
	UPROPERTY(Config, EditAnywhere, Category = "Dither")
	bool bInvert = false;

	// ---------------- Exposure ----------------

	/** Brightness multiplier on top of auto exposure. */
	UPROPERTY(Config, EditAnywhere, Category = "Exposure", meta = (ClampMin = "0.05", ClampMax = "10", UIMin = "0.1", UIMax = "4"))
	float ExposureBias = 1.f;

	/** Auto exposure maps this brightness percentile of the frame to white (0.995 = top 0.5% blows out). */
	UPROPERTY(Config, EditAnywhere, Category = "Exposure", meta = (ClampMin = "0.5", ClampMax = "1", UIMin = "0.8", UIMax = "1"))
	float WhitePercentile = 0.995f;

	/** How fast auto exposure adapts (0 = frozen, 1 = instant). */
	UPROPERTY(Config, EditAnywhere, Category = "Exposure", meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float AdaptSpeed = 0.08f;

	// ---------------- Lamp ----------------

	UPROPERTY(Config, EditAnywhere, Category = "Lamp", meta = (ClampMin = "0", ClampMax = "20000", UIMin = "0", UIMax = "6000", Units = "cd"))
	float LampIntensity = 1500.f;

	UPROPERTY(Config, EditAnywhere, Category = "Lamp", meta = (ClampMin = "5", ClampMax = "80", UIMin = "10", UIMax = "80"))
	float LampConeAngle = 52.f;

	/** 0 = hard-edged pool of light, 1 = soft falloff to the edge. */
	UPROPERTY(Config, EditAnywhere, Category = "Lamp", meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float LampSoftness = 0.4f;

	/** Height of the lamp shade above the floor, in cm. The suspect's head is around 170. Lower = tighter, harsher top light. */
	UPROPERTY(Config, EditAnywhere, Category = "Lamp", meta = (ClampMin = "120", ClampMax = "310", UIMin = "150", UIMax = "310", Units = "cm"))
	float LampHeight = 235.f;

	/** Move the lamp toward (-) or away from (+) the camera, relative to directly above the suspect. */
	UPROPERTY(Config, EditAnywhere, Category = "Lamp", meta = (UIMin = "-150", UIMax = "150", Units = "cm"))
	float LampForward = 0.f;

	/** Move the lamp left (-) / right (+) of the suspect. */
	UPROPERTY(Config, EditAnywhere, Category = "Lamp", meta = (UIMin = "-150", UIMax = "150", Units = "cm"))
	float LampSideways = 0.f;

	/** Swing of the hanging lamp in degrees. */
	UPROPERTY(Config, EditAnywhere, Category = "Lamp", meta = (ClampMin = "0", ClampMax = "20", UIMin = "0", UIMax = "10"))
	float LampSway = 3.f;

	/** How often the bulb stutters. 0 = never. */
	UPROPERTY(Config, EditAnywhere, Category = "Lamp", meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float LampFlicker = 0.1f;

	// ---------------- Window ----------------

	/** Streetlight through the venetian blinds. */
	UPROPERTY(Config, EditAnywhere, Category = "Window", meta = (ClampMin = "0", ClampMax = "50000", UIMin = "0", UIMax = "20000", Units = "cd"))
	float StreetIntensity = 9000.f;

	UPROPERTY(Config, EditAnywhere, Category = "Window", meta = (ClampMin = "5", ClampMax = "60", UIMin = "5", UIMax = "45"))
	float StreetConeAngle = 24.f;

	// ---------------- Set ----------------

	/** How far the table sits in front of the suspect (toward the camera), in cm. */
	UPROPERTY(Config, EditAnywhere, Category = "Set", meta = (ClampMin = "40", ClampMax = "300", UIMin = "50", UIMax = "200", Units = "cm"))
	float TableDistance = 60.f;

	/** Vera's microphone stand: distance in front of her, in cm. */
	UPROPERTY(Config, EditAnywhere, Category = "Set", meta = (ClampMin = "10", ClampMax = "120", UIMin = "15", UIMax = "80", Units = "cm"))
	float MicDistance = 32.f;

	/** Microphone height (should sit just below her mouth, ~145-155 cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Set", meta = (ClampMin = "80", ClampMax = "200", UIMin = "100", UIMax = "180", Units = "cm"))
	float MicHeight = 148.f;

	// ---------------- Rim light ----------------

	/** Backlight behind the suspect that outlines hair and shoulders against the dark. 0 = off. */
	UPROPERTY(Config, EditAnywhere, Category = "Rim Light", meta = (ClampMin = "0", ClampMax = "20000", UIMin = "0", UIMax = "8000", Units = "cd"))
	float RimIntensity = 2500.f;

	UPROPERTY(Config, EditAnywhere, Category = "Rim Light", meta = (ClampMin = "5", ClampMax = "80", UIMin = "10", UIMax = "60"))
	float RimConeAngle = 25.f;

	// ---------------- Haze ----------------

	/** Smoke in the air. Makes light beams visible. */
	UPROPERTY(Config, EditAnywhere, Category = "Haze", meta = (ClampMin = "0", ClampMax = "0.2", UIMin = "0", UIMax = "0.1"))
	float HazeDensity = 0.02f;

	/** How strongly the lights scatter in the haze (light shafts). */
	UPROPERTY(Config, EditAnywhere, Category = "Haze", meta = (ClampMin = "0", ClampMax = "10", UIMin = "0", UIMax = "5"))
	float BeamStrength = 2.f;

	// ---------------- Camera ----------------

	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "20", ClampMax = "110", UIMin = "25", UIMax = "90"))
	float FieldOfView = 58.f;

	/** Aim at the real character's head automatically (upper-body shot). Camera Height / Aim Height are then ignored. */
	UPROPERTY(Config, EditAnywhere, Category = "Camera")
	bool bAutoFrame = true;

	/** With Auto Frame: how far below the head to aim, in cm. More = more of her body and the desk in frame. */
	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (EditCondition = "bAutoFrame", UIMin = "-20", UIMax = "60", Units = "cm"))
	float AutoFrameDrop = 18.f;

	/** How far the camera sits from the suspect, in cm. Smaller = bigger face. */
	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "60", ClampMax = "600", UIMin = "100", UIMax = "450", Units = "cm"))
	float CameraDistance = 165.f;

	/** Camera height above the floor (a seated detective's eyes are about 120-130). */
	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "40", ClampMax = "300", UIMin = "80", UIMax = "250", Units = "cm"))
	float CameraHeight = 130.f;

	/** Height the camera aims at on the suspect. Raise it to push the face up, above the dialogue box. */
	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "40", ClampMax = "250", UIMin = "80", UIMax = "220", Units = "cm"))
	float CameraAimHeight = 115.f;

	/** Handheld drift amount. */
	UPROPERTY(Config, EditAnywhere, Category = "Camera", meta = (ClampMin = "0", ClampMax = "5", UIMin = "0", UIMax = "3"))
	float CameraDrift = 1.f;
};
