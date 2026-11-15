#include "NoirRoom.h"
#include "NoirLookSettings.h"
#include "NoirPoseAnimInstance.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Animation/AnimationAsset.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "FileMediaSource.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "Misc/Paths.h"

// Room layout (cm, local space). Camera sits at -X looking down +X at the suspect.
namespace NoirLayout
{
	constexpr float BackWallX = 320.f;
	constexpr float FrontWallX = -420.f;
	constexpr float HalfWidth = 330.f;
	constexpr float Height = 320.f;
	constexpr float TableZ = 88.f;
	constexpr float TableHalfWidth = 85.f; // 170 cm wide desk
	constexpr float SuspectX = 70.f;
}

ANoirRoom::ANoirRoom()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	CubeMesh = Cube.Object;
	SphereMesh = Sphere.Object;
	CylinderMesh = Cylinder.Object;
	ConeMesh = Cone.Object;
	BaseMaterial = Mat.Object;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PassThrough(TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Opaque.Widget3DPassThrough_Opaque"));
	PlaneMesh = Plane.Object;
	VideoMaterial = PassThrough.Object;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ANoirRoom::BeginPlay()
{
	Super::BeginPlay();
	BuildRoom();
	BuildSuspects();
	ShowSuspect(INDEX_NONE);
	ApplySettings(0.f);
}

UStaticMeshComponent* ANoirRoom::AddShape(EShape Shape, const FVector& Location, const FVector& SizeCm, float Shade,
	USceneComponent* Parent, const FRotator& Rotation)
{
	UStaticMesh* Mesh = CubeMesh;
	switch (Shape)
	{
	case EShape::Sphere: Mesh = SphereMesh; break;
	case EShape::Cylinder: Mesh = CylinderMesh; break;
	case EShape::Cone: Mesh = ConeMesh; break;
	default: break;
	}

	UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
	Comp->SetMobility(EComponentMobility::Movable);
	Comp->SetStaticMesh(Mesh);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetupAttachment(Parent ? Parent : RootComponent.Get());
	Comp->SetRelativeLocation(Location);
	Comp->SetRelativeRotation(Rotation);
	Comp->SetRelativeScale3D(SizeCm / 100.f); // engine basic shapes are 100cm

	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(Shade, Shade, Shade));
	Comp->SetMaterial(0, MID);

	Comp->RegisterComponent();
	return Comp;
}

void ANoirRoom::BuildRoom()
{
	using namespace NoirLayout;
	const float Mid = (BackWallX + FrontWallX) * 0.5f;
	const float Len = BackWallX - FrontWallX;

	// Shell. The right wall has a window gap filled with blinds.
	AddShape(EShape::Cube, FVector(Mid, 0, -5), FVector(Len, HalfWidth * 2, 10), 0.35f);                 // floor
	AddShape(EShape::Cube, FVector(Mid, 0, Height + 5), FVector(Len, HalfWidth * 2, 10), 0.2f);          // ceiling
	AddShape(EShape::Cube, FVector(BackWallX + 5, 0, Height / 2), FVector(10, HalfWidth * 2, Height), 0.8f); // back wall
	AddShape(EShape::Cube, FVector(FrontWallX - 5, 0, Height / 2), FVector(10, HalfWidth * 2, Height), 0.5f);
	AddShape(EShape::Cube, FVector(Mid, -HalfWidth - 5, Height / 2), FVector(Len, 10, Height), 0.6f);    // left wall

	// Right wall: solid except a window from X 40..260, Z 90..270.
	constexpr float WinX0 = 40.f, WinX1 = 260.f, WinZ0 = 90.f, WinZ1 = 270.f;
	const float RY = HalfWidth + 5;
	AddShape(EShape::Cube, FVector((FrontWallX + WinX0) / 2, RY, Height / 2), FVector(WinX0 - FrontWallX, 10, Height), 0.6f);
	AddShape(EShape::Cube, FVector((WinX1 + BackWallX) / 2, RY, Height / 2), FVector(BackWallX - WinX1, 10, Height), 0.6f);
	AddShape(EShape::Cube, FVector((WinX0 + WinX1) / 2, RY, WinZ0 / 2), FVector(WinX1 - WinX0, 10, WinZ0), 0.6f);
	AddShape(EShape::Cube, FVector((WinX0 + WinX1) / 2, RY, (WinZ1 + Height) / 2), FVector(WinX1 - WinX0, 10, Height - WinZ1), 0.6f);
	for (float Z = WinZ0 + 6; Z < WinZ1; Z += 16.f)
	{
		AddShape(EShape::Cube, FVector((WinX0 + WinX1) / 2, RY, Z), FVector(WinX1 - WinX0, 2, 8), 0.4f);  // blind slats
	}

	// Streetlight outside the window, throwing blind stripes across the back wall and suspect.
	StreetLight = NewObject<USpotLightComponent>(this);
	StreetLight->SetupAttachment(RootComponent);
	StreetLight->SetRelativeLocation(FVector(60, HalfWidth + 420, 330));
	StreetLight->SetRelativeRotation((FVector(BackWallX, -120, 120) - FVector(60, HalfWidth + 420, 330)).Rotation());
	StreetLight->SetIntensityUnits(ELightUnits::Candelas);
	StreetLight->SetAttenuationRadius(2000.f);
	StreetLight->SetCastShadows(true);
	StreetLight->RegisterComponent();

	// Interrogation table at real-world scale (70 x 110 cm, 76 cm high). Its distance from the suspect
	// is a slider, so TableRoot is placed every tick in ApplySettings.
	TableRoot = NewObject<USceneComponent>(this);
	TableRoot->SetupAttachment(RootComponent);
	TableRoot->RegisterComponent();
	AddShape(EShape::Cube, FVector(0, 0, TableZ - 2), FVector(70, TableHalfWidth * 2, 4), 0.35f, TableRoot);
	for (const FVector2D& Leg : { FVector2D(-30, -(TableHalfWidth - 5)), FVector2D(-30, TableHalfWidth - 5), FVector2D(30, -(TableHalfWidth - 5)), FVector2D(30, TableHalfWidth - 5) })
	{
		AddShape(EShape::Cube, FVector(Leg.X, Leg.Y, (TableZ - 4) / 2), FVector(4, 4, TableZ - 4), 0.25f, TableRoot);
	}
	// Solid front panel so it reads as a desk and hides the suspect's legs (upper-body framing).
	AddShape(EShape::Cube, FVector(-33, 0, (TableZ - 4) / 2), FVector(2, TableHalfWidth * 2 - 4, TableZ - 4), 0.15f, TableRoot);
	// Props, life-size: ashtray, a whiskey glass, the detective's notebook.
	AddShape(EShape::Cylinder, FVector(10, -48, TableZ + 1.5f), FVector(13, 13, 3), 0.6f, TableRoot);
	AddShape(EShape::Cylinder, FVector(5, 50, TableZ + 4.5f), FVector(7, 7, 9), 0.8f, TableRoot);
	AddShape(EShape::Cube, FVector(-22, 8, TableZ + 0.75f), FVector(14, 20, 1.5f), 0.9f, TableRoot, FRotator(0, 12, 0));

	// Chair for the placeholder figure (hidden when a real character stands there).
	ChairRoot = NewObject<USceneComponent>(this);
	ChairRoot->SetupAttachment(RootComponent);
	ChairRoot->RegisterComponent();
	AddShape(EShape::Cube, FVector(SuspectX + 20, 0, 45), FVector(45, 50, 6), 0.4f, ChairRoot);
	AddShape(EShape::Cube, FVector(SuspectX + 42, 0, 85), FVector(5, 48, 80), 0.4f, ChairRoot);

	// Hanging lamp: cord + shade on a pivot that sways.
	LampPivot = NewObject<USceneComponent>(this);
	LampPivot->SetupAttachment(RootComponent);
	LampPivot->SetRelativeLocation(FVector(SuspectX, 0, Height));
	LampPivot->RegisterComponent();
	// Cord length, shade and bulb positions are set from the Lamp Height slider in ApplySettings.
	LampCord = AddShape(EShape::Cylinder, FVector::ZeroVector, FVector(1.2f, 1.2f, 100), 0.2f, LampPivot);
	LampShade = AddShape(EShape::Cone, FVector::ZeroVector, FVector(50, 50, 30), 0.25f, LampPivot);

	LampLight = NewObject<USpotLightComponent>(this);
	LampLight->SetupAttachment(LampPivot);
	LampLight->SetRelativeRotation(FRotator(-90, 0, 0));
	LampLight->SetIntensityUnits(ELightUnits::Candelas);
	LampLight->SetAttenuationRadius(800.f);
	LampLight->SetSourceRadius(2.f);
	LampLight->SetCastShadows(true);
	LampLight->RegisterComponent();

	// Thin volumetric haze so the light beams read.
	Fog = NewObject<UExponentialHeightFogComponent>(this);
	Fog->SetupAttachment(RootComponent);
	Fog->SetRelativeLocation(FVector::ZeroVector);
	Fog->SetFogHeightFalloff(0.001f);
	Fog->SetFogInscatteringColor(FLinearColor::Black);
	Fog->SetVolumetricFog(true);
	Fog->SetVolumetricFogExtinctionScale(1.f);
	Fog->RegisterComponent();

	// Camera: the detective's eyes, across the table.
	CameraRig = NewObject<USceneComponent>(this);
	CameraRig->SetupAttachment(RootComponent);
	CameraRig->RegisterComponent(); // placed every tick from the Camera sliders

	// Rim light: behind and above the suspect, aimed at their head and shoulders to edge-light the silhouette.
	RimLight = NewObject<USpotLightComponent>(this);
	RimLight->SetupAttachment(RootComponent);
	RimLight->SetRelativeLocation(FVector(SuspectX + 170, 0, 270));
	RimLight->SetRelativeRotation((FVector(SuspectX, 0, 150) - FVector(SuspectX + 170, 0, 270)).Rotation());
	RimLight->SetIntensityUnits(ELightUnits::Candelas);
	RimLight->SetAttenuationRadius(600.f);
	RimLight->SetCastShadows(true);
	RimLight->RegisterComponent();

	const FIntPoint Res = DesiredResolution();
	CaptureTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, Res.X, Res.Y, RTF_RGBA16f, FLinearColor::Black, false);

	CaptureComponent = NewObject<USceneCaptureComponent2D>(this);
	CaptureComponent->SetupAttachment(CameraRig);
	CaptureComponent->TextureTarget = CaptureTarget;
	CaptureComponent->CaptureSource = SCS_SceneColorHDR;
	CaptureComponent->bCaptureEveryFrame = false;
	CaptureComponent->bCaptureOnMovement = false;
	CaptureComponent->bAlwaysPersistRenderingState = true; // keeps volumetric fog / shadows stable between captures
	// Only our own lights: ignore whatever sun/sky the host level may have.
	CaptureComponent->ShowFlags.SetDirectionalLights(false);
	CaptureComponent->ShowFlags.SetSkyLighting(false);
	CaptureComponent->ShowFlags.SetAtmosphere(false);
	CaptureComponent->ShowFlags.SetCloud(false);
	CaptureComponent->RegisterComponent();
}

void ANoirRoom::BuildSuspects()
{
	using namespace NoirLayout;
	const float X = SuspectX;
	const float Z = TableZ;
	const float Skin = 0.85f;
	const float Cloth = 0.45f;
	const float Dark = 0.12f;

	auto MakeRoot = [this]()
	{
		USceneComponent* R = NewObject<USceneComponent>(this);
		R->SetupAttachment(RootComponent);
		R->RegisterComponent();
		SuspectRoots.Add(R);
		return R;
	};

	auto Body = [&](USceneComponent* R, float ShoulderW, float HeadZ, float Coat)
	{
		AddShape(EShape::Cylinder, FVector(X, 0, Z + 30), FVector(32, ShoulderW * 0.8f, 70), Coat, R);       // torso
		AddShape(EShape::Sphere, FVector(X, 0, Z + 62), FVector(34, ShoulderW, 22), Coat, R);                 // shoulders
		AddShape(EShape::Cylinder, FVector(X, 0, HeadZ - 20), FVector(10, 10, 14), Skin, R);                   // neck
		AddShape(EShape::Sphere, FVector(X, 0, HeadZ), FVector(22, 19, 26), Skin, R);                          // head
		for (float Side : { -1.f, 1.f })                                                                       // forearms on the table
		{
			AddShape(EShape::Cube, FVector(X - 28, Side * ShoulderW * 0.32f, Z + 6), FVector(50, 11, 10), Coat, R, FRotator(0, -Side * 18, 0));
			AddShape(EShape::Sphere, FVector(X - 54, Side * ShoulderW * 0.22f, Z + 6), FVector(10, 9, 7), Skin, R); // hands
		}
	};

	// 0: Vera Lane - slim, big curled hair, pale gown.
	{
		USceneComponent* R = MakeRoot();
		Body(R, 46, Z + 98, 0.9f);
		AddShape(EShape::Sphere, FVector(X + 6, 0, Z + 106), FVector(28, 30, 26), Dark, R);
		AddShape(EShape::Sphere, FVector(X + 4, -14, Z + 92), FVector(14, 12, 24), Dark, R);
		AddShape(EShape::Sphere, FVector(X + 4, 14, Z + 92), FVector(14, 12, 24), Dark, R);
		AddShape(EShape::Sphere, FVector(X - 9, 9, Z + 112), FVector(6, 6, 6), 1.f, R); // flower in her hair
	}
	// 1: Mickey Doyle - wide, white shirt, dark vest, bow tie, flat cap.
	{
		USceneComponent* R = MakeRoot();
		Body(R, 66, Z + 96, 0.95f);
		AddShape(EShape::Cylinder, FVector(X - 4, 0, Z + 30), FVector(28, 44, 66), Dark, R);                  // vest
		AddShape(EShape::Cone, FVector(X - 17, -5, Z + 74), FVector(7, 7, 9), Dark, R, FRotator(0, 0, 90));  // bow tie
		AddShape(EShape::Cone, FVector(X - 17, 5, Z + 74), FVector(7, 7, 9), Dark, R, FRotator(0, 0, -90));
		AddShape(EShape::Cylinder, FVector(X, 0, Z + 109), FVector(25, 23, 7), Dark, R);                      // cap
		AddShape(EShape::Cube, FVector(X - 14, 0, Z + 107), FVector(14, 20, 2), Dark, R, FRotator(-10, 0, 0));
	}
	// 2: Harlan Graves - dark suit, white shirt front, fedora.
	{
		USceneComponent* R = MakeRoot();
		Body(R, 56, Z + 100, Dark);
		AddShape(EShape::Cube, FVector(X - 15, 0, Z + 50), FVector(4, 14, 34), 1.f, R, FRotator(0, 0, 0));     // shirt front
		AddShape(EShape::Cube, FVector(X - 17, 0, Z + 48), FVector(3, 4, 28), Dark, R);                        // tie
		AddShape(EShape::Cylinder, FVector(X, 0, Z + 113), FVector(48, 48, 2.5f), Dark, R);                    // brim
		AddShape(EShape::Cylinder, FVector(X, 0, Z + 122), FVector(24, 22, 16), Dark, R);                      // crown
		AddShape(EShape::Cylinder, FVector(X, 0, Z + 116), FVector(25, 23, 4), 0.9f, R);                       // hat band
	}
	// 3: Tommy Mullen - broad shoulders, white tank top.
	{
		USceneComponent* R = MakeRoot();
		Body(R, 58, Z + 100, 0.9f);
	}
	// 4: Allison Wu - slim, pale gown.
	{
		USceneComponent* R = MakeRoot();
		Body(R, 42, Z + 98, 0.8f);
	}
	// 5: Logan Wu - tall, upright, striped shirt.
	{
		USceneComponent* R = MakeRoot();
		Body(R, 48, Z + 106, 0.7f);
	}
}

void ANoirRoom::ShowSuspect(int32 Index)
{
	ShownIndex = Index;
	RefreshVisibility();
}

void ANoirRoom::UpdateHolder(int32 Index, AActor* Real, USkeletalMeshComponent* Body, const FNoirSeatedPose& Pose)
{
	Holders.SetNum(SuspectRoots.Num() * 2);
	TObjectPtr<UStaticMeshComponent>& Stick = Holders[Index * 2];
	TObjectPtr<UStaticMeshComponent>& Tip = Holders[Index * 2 + 1];

	const bool bWant = Body && Pose.bShowHolder;
	if (!bWant)
	{
		if (Stick) { Stick->SetVisibility(false); }
		if (Tip) { Tip->SetVisibility(false); }
		return;
	}

	auto Make = [&](float Shade)
	{
		// Owned by the suspect actor so it hides/shows with her.
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(Real);
		Comp->SetStaticMesh(CylinderMesh);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetCastShadow(true);
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMaterial, Real);
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(Shade, Shade, Shade));
		Comp->SetMaterial(0, MID);
		Comp->SetupAttachment(Body, TEXT("hand_r"));
		Comp->RegisterComponent();
		Real->AddInstanceComponent(Comp);
		return Comp;
	};
	if (!Stick || Stick->GetOwner() != Real) { Stick = Make(0.12f); }
	if (!Tip || Tip->GetOwner() != Real) { Tip = Make(1.f); }

	// The holder runs along its own Z axis (engine cylinder); the cigarette sits at the far end.
	const FTransform Grip(Pose.HolderRotation, Pose.HolderOffset);
	const float Len = Pose.HolderLength;
	Stick->SetRelativeTransform(FTransform(FVector(0, 0, Len * 0.5f)) * Grip);
	Stick->SetRelativeScale3D(FVector(0.009f, 0.009f, Len / 100.f));
	Tip->SetRelativeTransform(FTransform(FVector(0, 0, Len + 3.5f)) * Grip);
	Tip->SetRelativeScale3D(FVector(0.011f, 0.011f, 0.07f));
	Stick->SetVisibility(true);
	Tip->SetVisibility(true);
}

void ANoirRoom::DefaultPerformance(int32 Index, TSoftObjectPtr<UAnimationAsset>& OutBody, TSoftObjectPtr<UAnimationAsset>& OutFace)
{
	// Paired body ("_b_") and face ("_f_") loops from the MetaHuman Creator Core Data.
	static const TCHAR* Root = TEXT("/MetaHumanCharacter/Optional/Animation/TemplateAnimations/Expression_Loops/");
	static const TCHAR* Pairs[][3] = {
		{ TEXT("SadA"),   TEXT("mhc_mh002_fmn_b_facialloop_sad_f_s001"),   TEXT("mhc_mh002_fmn_f_facialloop_sad_f_s001") },   // Vera: melancholy torch singer
		{ TEXT("FearB"),  TEXT("mhc_mh002_fmn_b_facialloop_fear_m_s001"),  TEXT("mhc_mh002_fmn_f_facialloop_fear_m_s001") },  // Mickey: nervous liar
		{ TEXT("AngerB"), TEXT("mhc_mh002_fmn_b_facialloop_angry_m_s001"), TEXT("mhc_mh002_fmn_f_facialloop_angry_m_s001") }, // Graves: sour partner
	};
	if (Index < 0 || Index >= UE_ARRAY_COUNT(Pairs))
	{
		return;
	}
	auto Make = [](const TCHAR* Folder, const TCHAR* Asset)
	{
		return TSoftObjectPtr<UAnimationAsset>(FSoftObjectPath(FString::Printf(TEXT("%s%s/%s.%s"), Root, Folder, Asset, Asset)));
	};
	OutBody = Make(Pairs[Index][0], Pairs[Index][1]);
	OutFace = Make(Pairs[Index][0], Pairs[Index][2]);
}

void ANoirRoom::RefreshVisibility()
{
	ChairRoot->SetVisibility(!HasVideo(ShownIndex), true); // a video suspect brings her own chair

	for (int32 i = 0; i < SuspectRoots.Num(); ++i)
	{
		AActor* Real = SuspectActors.IsValidIndex(i) ? SuspectActors[i].Get() : nullptr;
		SuspectRoots[i]->SetVisibility(i == ShownIndex && !Real && !HasVideo(i), true);
		if (VideoPanels.IsValidIndex(i) && VideoPanels[i])
		{
			VideoPanels[i]->SetVisibility(i == ShownIndex && HasVideo(i));
		}
		if (Real)
		{
			Real->SetActorHiddenInGame(i != ShownIndex);
		}
	}
}

void ANoirRoom::SyncSuspectCharacters()
{
	const UNoirLookSettings& Look = UNoirLookSettings::Get();
	const int32 Num = SuspectRoots.Num();
	SuspectActors.SetNum(Num);
	SuspectActorClassPaths.SetNum(Num);
	SuspectAnimPaths.SetNum(Num);

	bool bChanged = false;
	for (int32 i = 0; i < Num; ++i)
	{
		const FNoirSuspectSlot* Slot = Look.GetSuspectSlot(i);
		if (!Slot)
		{
			continue;
		}

		// (Re)spawn when the chosen character changes, so slots can be swapped live during Play.
		// A video suspect replaces the 3D character entirely.
		const FSoftObjectPath ClassPath = Slot->Video.FilePath.IsEmpty() ? Slot->Character.ToSoftObjectPath() : FSoftObjectPath();
		if (ClassPath != SuspectActorClassPaths[i])
		{
			SuspectActorClassPaths[i] = ClassPath;
			SuspectAnimPaths[i].Reset();
			if (SuspectActors[i])
			{
				SuspectActors[i]->Destroy();
				SuspectActors[i] = nullptr;
			}
			if (UClass* Class = ClassPath.IsValid() ? Slot->Character.LoadSynchronous() : nullptr)
			{
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				Params.Owner = this;
				if (AActor* Real = GetWorld()->SpawnActor<AActor>(Class, GetActorTransform(), Params))
				{
					Real->SetActorEnableCollision(false);
					Real->AttachToActor(this, FAttachmentTransformRules::KeepRelativeTransform);
					SuspectActors[i] = Real;
					SettleFrames.SetNum(Num);
					SettleFrames[i] = 45; // centre her for the first frames only; after that the mocap hips move freely

					// Clothes follow the body rigidly: cloth simulation explodes when the character is posed/moved procedurally.
					TArray<UActorComponent*> Comps;
					Real->GetComponents(Comps);
					for (UActorComponent* Comp : Comps)
					{
						UE_LOG(LogTemp, Display, TEXT("Noir: suspect %d component %s (%s)"), i + 1, *Comp->GetName(), *Comp->GetClass()->GetName());
						if (UChaosClothComponent* Cloth = Cast<UChaosClothComponent>(Comp))
						{
							Cloth->SetEnableSimulation(false);
							Cloth->SuspendSimulation();
							Cloth->ForceNextUpdateTeleportAndReset();
						}
						else if (USkeletalMeshComponent* Skel = Cast<USkeletalMeshComponent>(Comp))
						{
							Skel->bDisableClothSimulation = true;
						}
					}
				}
			}
			else if (!ClassPath.IsNull())
			{
				UE_LOG(LogTemp, Warning, TEXT("Noir: could not load suspect %d character %s"), i + 1, *ClassPath.ToString());
			}
			bChanged = true;
		}

		AActor* Real = SuspectActors[i];
		if (!Real)
		{
			continue;
		}

		// Placement follows the sliders live.
		CenterFix.SetNum(Num);
		Real->SetActorRelativeTransform(FTransform(
			FRotator(0, Slot->Yaw, 0),
			FVector(NoirLayout::SuspectX, 0, 0) + Slot->Offset + CenterFix[i],
			FVector(Slot->Scale)));

		TArray<USkeletalMeshComponent*> Meshes;
		Real->GetComponents(Meshes);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			Mesh->SetForcedLOD(Look.bForceHighestDetail ? 1 : 0); // 1 = LOD0, 0 = automatic
		}

		USkeletalMeshComponent* Body = nullptr;
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			if (Mesh->GetFName() == TEXT("Body")) { Body = Mesh; break; }
		}

		// Keep her hips centered behind the desk whatever the model's pivot is (Offset still nudges on top).
		SettleFrames.SetNum(Num);
		if (Body && Body->DoesSocketExist(TEXT("pelvis")) && SettleFrames[i] > 0)
		{
			--SettleFrames[i];
			const FVector Hips = GetActorTransform().InverseTransformPosition(Body->GetSocketLocation(TEXT("pelvis")));
			const FVector2D Error = FVector2D(NoirLayout::SuspectX + Slot->Offset.X - Hips.X, Slot->Offset.Y - Hips.Y);
			CenterFix[i].X = FMath::Clamp(CenterFix[i].X + Error.X * 0.25f, -150.f, 150.f);
			CenterFix[i].Y = FMath::Clamp(CenterFix[i].Y + Error.Y * 0.25f, -150.f, 150.f);
		}
		if (Body && Body->DoesSocketExist(TEXT("head")))
		{
			HeadHeights.SetNum(Num);
			HeadHeights[i] = GetActorTransform().InverseTransformPosition(Body->GetSocketLocation(TEXT("head"))).Z;
		}

		if (Slot->bSeated)
		{
			// Procedural seated performance (UNoirPoseAnimInstance) on the body; the MetaHuman face follows the body.
			if (Body && Body->GetAnimClass() != UNoirPoseAnimInstance::StaticClass())
			{
				Body->SetAnimationMode(EAnimationMode::AnimationBlueprint);
				Body->SetAnimInstanceClass(UNoirPoseAnimInstance::StaticClass());
				SuspectAnimPaths[i] = TEXT("seated");
			}
			if (UNoirPoseAnimInstance* PoseAnim = Body ? Cast<UNoirPoseAnimInstance>(Body->GetAnimInstance()) : nullptr)
			{
				PoseAnim->SuspectIndex = i;
			}
			UpdateHolder(i, Real, Body, Slot->Pose);
		}
		else
		{
			UpdateHolder(i, Real, Body, Slot->Pose);
			// Body animation. Empty slots fall back to a matched MetaHuman body loop per suspect.
			// (Face animations are only played when chosen explicitly: a face playing its own clip stops following the head.)
			TSoftObjectPtr<UAnimationAsset> BodyAnim = Slot->IdleAnimation;
			TSoftObjectPtr<UAnimationAsset> FaceAnim = Slot->FaceAnimation;
			if (BodyAnim.IsNull() && FaceAnim.IsNull())
			{
				TSoftObjectPtr<UAnimationAsset> Unused;
				DefaultPerformance(i, BodyAnim, Unused);
			}
			const FString AnimKey = BodyAnim.ToSoftObjectPath().ToString() + TEXT("|") + FaceAnim.ToSoftObjectPath().ToString();
			if (AnimKey != SuspectAnimPaths[i])
			{
				SuspectAnimPaths[i] = AnimKey;
				SettleFrames.SetNum(Num);
				SettleFrames[i] = 45;
				CenterFix.SetNum(Num);
				CenterFix[i] = FVector::ZeroVector;
				for (USkeletalMeshComponent* Mesh : Meshes)
				{
					const FName MeshName = Mesh->GetFName();
					UAnimationAsset* Anim = nullptr;
					if (MeshName == TEXT("Body")) { Anim = BodyAnim.LoadSynchronous(); }
					else if (MeshName == TEXT("Face")) { Anim = FaceAnim.LoadSynchronous(); }
					if (Anim)
					{
						Mesh->PlayAnimation(Anim, true);
					}
				}
			}
		}
	}

	if (bChanged)
	{
		RefreshVisibility();
	}
}

FIntPoint ANoirRoom::DesiredResolution()
{
	const int32 Y = FMath::Clamp(UNoirLookSettings::Get().VerticalResolution, 90, 1080);
	return FIntPoint(FMath::RoundToInt32(Y * 16.f / 9.f), Y);
}

void ANoirRoom::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ApplySettings(GetWorld()->GetTimeSeconds());
}

void ANoirRoom::ApplySettings(float T)
{
	const UNoirLookSettings& Look = UNoirLookSettings::Get();

	SyncSuspectCharacters();
	SyncVideoSuspects();

	// Table between detective and suspect.
	TableRoot->SetRelativeLocation(FVector(NoirLayout::SuspectX - Look.TableDistance, 0, 0));

	// Lamp hangs straight down over the suspect; the cord stretches to reach the chosen height.
	const float Cord = FMath::Max(NoirLayout::Height - Look.LampHeight, 5.f);
	LampPivot->SetRelativeLocation(FVector(NoirLayout::SuspectX + Look.LampForward, Look.LampSideways, NoirLayout::Height));
	LampCord->SetRelativeLocation(FVector(0, 0, -Cord * 0.5f));
	LampCord->SetRelativeScale3D(FVector(0.012f, 0.012f, Cord / 100.f));
	LampShade->SetRelativeLocation(FVector(0, 0, -Cord - 10.f));
	LampLight->SetRelativeLocation(FVector(0, 0, -Cord - 28.f)); // just under the shade's closed base

	// Lamp: sway + occasional stutter.
	LampPivot->SetRelativeRotation(FRotator(FMath::Sin(T * 0.9f) * Look.LampSway, 0, FMath::Sin(T * 0.6f) * Look.LampSway * 0.66f));
	const float Stutter = FMath::Sin(T * 23.f) * FMath::Sin(T * 3.1f);
	const bool bDip = Look.LampFlicker > 0.f && Stutter > 1.f - Look.LampFlicker * 0.5f;
	LampLight->SetIntensity(Look.LampIntensity * (bDip ? 0.35f : 1.f));
	LampLight->SetOuterConeAngle(Look.LampConeAngle);
	LampLight->SetInnerConeAngle(Look.LampConeAngle * (1.f - Look.LampSoftness));
	LampLight->SetVolumetricScatteringIntensity(Look.BeamStrength);

	// Window.
	StreetLight->SetIntensity(Look.StreetIntensity);
	StreetLight->SetOuterConeAngle(Look.StreetConeAngle);
	StreetLight->SetInnerConeAngle(Look.StreetConeAngle * 0.75f);
	StreetLight->SetVolumetricScatteringIntensity(Look.BeamStrength * 0.75f);

	// Haze.
	Fog->SetFogDensity(Look.HazeDensity);
	Fog->SetVolumetricFog(Look.HazeDensity > 0.f);

	// Camera.
	CaptureComponent->FOVAngle = Look.FieldOfView;
	// Auto Frame: aim just below the shown character's actual head (upper-body shot), smoothed.
	float AimZ = Look.CameraAimHeight;
	float CamZ = Look.CameraHeight;
	const bool bRealShown = SuspectActors.IsValidIndex(ShownIndex) && SuspectActors[ShownIndex] && HeadHeights.IsValidIndex(ShownIndex);
	if (Look.bAutoFrame && bRealShown)
	{
		SmoothedHead = FMath::FInterpTo(SmoothedHead > 0.f ? SmoothedHead : HeadHeights[ShownIndex], HeadHeights[ShownIndex], GetWorld()->GetDeltaSeconds(), 1.5f);
		AimZ = SmoothedHead - Look.AutoFrameDrop;
		CamZ = AimZ + 6.f;
	}
	const FVector CamPos(
		NoirLayout::SuspectX - Look.CameraDistance,
		FMath::Sin(T * 0.3f) * 6.f * Look.CameraDrift,
		CamZ + FMath::Sin(T * 0.45f) * 3.f * Look.CameraDrift);
	CameraRig->SetRelativeLocation(CamPos);
	CaptureComponent->SetRelativeRotation((FVector(NoirLayout::SuspectX, 0, AimZ) - CamPos).Rotation());

	// Rim light.
	RimLight->SetIntensity(Look.RimIntensity);
	RimLight->SetOuterConeAngle(Look.RimConeAngle);
	RimLight->SetInnerConeAngle(Look.RimConeAngle * 0.6f);
	RimLight->SetVolumetricScatteringIntensity(Look.BeamStrength * 0.5f);
}

bool ANoirRoom::HasVideo(int32 Index) const
{
	return VideoPaths.IsValidIndex(Index) && !VideoPaths[Index].IsEmpty();
}

void ANoirRoom::SyncVideoSuspects()
{
	const UNoirLookSettings& Look = UNoirLookSettings::Get();
	const int32 Num = SuspectRoots.Num();
	VideoPanels.SetNum(Num);
	VideoPlayers.SetNum(Num);
	VideoTextures.SetNum(Num);
	VideoPaths.SetNum(Num);
	ShadowCasters.SetNum(Num);
	HandsPanels.SetNum(Num);
	const FVector CamLocal = GetActorTransform().InverseTransformPosition(CaptureComponent->GetComponentLocation());

	bool bChanged = false;
	for (int32 i = 0; i < Num; ++i)
	{
		const FNoirSuspectSlot* Slot = Look.GetSuspectSlot(i);
		FString Path = Slot ? Slot->ActiveVideoPath() : FString();
		if (!Path.IsEmpty() && FPaths::IsRelative(Path))
		{
			Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), Path);
		}
		if (!Path.IsEmpty() && !FPaths::FileExists(Path))
		{
			Path.Reset(); // missing file: fall back to character / placeholder
		}

		if (Path != VideoPaths[i])
		{
			VideoPaths[i] = Path;
			bChanged = true;
			if (VideoPlayers[i])
			{
				VideoPlayers[i]->Close();
			}
			if (!Path.IsEmpty())
			{
				if (!VideoPlayers[i])
				{
					VideoPlayers[i] = NewObject<UMediaPlayer>(this);
					VideoPlayers[i]->SetLooping(true);
					VideoPlayers[i]->PlayOnOpen = true;
					VideoTextures[i] = NewObject<UMediaTexture>(this);
					VideoTextures[i]->AutoClear = true;
					VideoTextures[i]->SetMediaPlayer(VideoPlayers[i]);
					VideoTextures[i]->UpdateResource();

					UStaticMeshComponent* Panel = NewObject<UStaticMeshComponent>(this);
					Panel->SetStaticMesh(PlaneMesh);
					Panel->SetupAttachment(RootComponent);
					Panel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					Panel->RegisterComponent();
					// Cut-out card (stacked colour + matte clips) when the project material exists; plain picture otherwise.
					UMaterialInterface* CardMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Noir/M_NoirVideoCard.M_NoirVideoCard"));
					Panel->SetCastShadow(CardMaterial != nullptr); // the silhouette shadows the desk
					UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(CardMaterial ? CardMaterial : VideoMaterial.Get(), Panel);
					Mid->SetTextureParameterValue(TEXT("SlateUI"), VideoTextures[i]);
					Mid->SetTextureParameterValue(TEXT("Video"), VideoTextures[i]);
					Panel->SetMaterial(0, Mid);
					VideoPanels[i] = Panel;

					if (CardMaterial)
					{
						UStaticMeshComponent* Hands = NewObject<UStaticMeshComponent>(this);
						Hands->SetStaticMesh(PlaneMesh);
						Hands->SetupAttachment(RootComponent);
						Hands->SetCollisionEnabled(ECollisionEnabled::NoCollision);
						Hands->SetCastShadow(false);
						Hands->RegisterComponent();
						UMaterialInstanceDynamic* HandsMid = UMaterialInstanceDynamic::Create(CardMaterial, Hands);
						HandsMid->SetTextureParameterValue(TEXT("Video"), VideoTextures[i]);
						HandsMid->SetScalarParameterValue(TEXT("Rows"), 3.f);
						HandsMid->SetScalarParameterValue(TEXT("MatteRow"), 2.f);
						Hands->SetMaterial(0, HandsMid);
						Hands->SetVisibility(false);
						HandsPanels[i] = Hands;
					}
				}
				UFileMediaSource* Source = NewObject<UFileMediaSource>(this);
				Source->SetFilePath(FPaths::ConvertRelativePathToFull(Path));
				VideoPlayers[i]->OpenSource(Source);
				UE_LOG(LogTemp, Display, TEXT("Noir: suspect %d video %s"), i, *Path);
			}
		}

		// Invisible stand-in that blocks the lamp, so the suspect's shadow falls across the desk.
		if (!ShadowCasters[i])
		{
			UStaticMeshComponent* Caster = NewObject<UStaticMeshComponent>(this);
			Caster->SetStaticMesh(CubeMesh);
			Caster->SetupAttachment(RootComponent);
			Caster->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Caster->SetHiddenInGame(true);
			Caster->bCastHiddenShadow = true;
			Caster->RegisterComponent();
			ShadowCasters[i] = Caster;
		}
		{
			UStaticMeshComponent* Caster = ShadowCasters[i];
			const bool bOn = Slot && Slot->bDeskShadow && i == ShownIndex;
			Caster->SetCastShadow(bOn);
			if (bOn)
			{
				Caster->SetRelativeLocation(FVector(NoirLayout::SuspectX, 0.f, 0.f) + Slot->Offset * FVector(1.f, 1.f, 0.f) + Slot->ShadowOffset);
				Caster->SetRelativeScale3D(Slot->ShadowSize / 100.f);
			}
		}

		// Place and size the panel: standing upright in the chair spot, facing the camera.
		if (UStaticMeshComponent* Panel = VideoPanels[i])
		{
			const float H = Slot ? Slot->VideoHeight : 190.f;
			const float W = H * 16.f / 9.f;
			const FVector Offset = Slot ? Slot->Offset : FVector::ZeroVector;
			Panel->SetRelativeLocation(FVector(NoirLayout::SuspectX + (Slot ? Slot->VideoDepth : 30.f), 0.f, H * 0.5f) + Offset);
			// Plane faces +Z: turn it so its face points at the camera (-X), its X runs across the screen, then spin about the normal.
			const FQuat Base = FRotationMatrix::MakeFromXY(FVector(0.f, 1.f, 0.f), FVector(0.f, 0.f, -1.f)).ToQuat();
			const FQuat Spin(FVector(-1.f, 0.f, 0.f), FMath::DegreesToRadians(Slot ? Slot->VideoSpin : 0.f));
			Panel->SetRelativeRotation(Spin * Base);
			const float Mirror = (Slot && Slot->bVideoMirror) ? -1.f : 1.f;
			Panel->SetRelativeScale3D(FVector(Mirror * W / 100.f, H / 100.f, 1.f));
			if (UMaterialInstanceDynamic* Mid = Cast<UMaterialInstanceDynamic>(Panel->GetMaterial(0)))
			{
				const float B = Slot ? Slot->VideoBrightness : 40.f;
				Mid->SetVectorParameterValue(TEXT("TintColorAndOpacity"), FLinearColor(B, B, B, 1.f));
				Mid->SetScalarParameterValue(TEXT("Brightness"), B);
				Mid->SetScalarParameterValue(TEXT("Lit"), Slot ? Slot->VideoLit : 1.f);
				Mid->SetScalarParameterValue(TEXT("Rows"), (Slot && Slot->bHandsLayer) ? 3.f : 2.f);
				Mid->SetScalarParameterValue(TEXT("MatteRow"), 1.f);
			}
			Panel->SetCastShadow(Slot && Slot->bVideoShadow);

			// Hands card: the body card projected from the camera onto a plane just in front of the desk,
			// so it covers the exact same pixels but is never hidden by the desktop.
			if (UStaticMeshComponent* Hands = HandsPanels[i])
			{
				const bool bShow = Slot && Slot->bHandsLayer && Panel->IsVisible();
				Hands->SetVisibility(bShow);
				if (bShow)
				{
					// Just inside the desk's back edge: the lamp lights it like the body, and props nearer the camera stay in front.
					const float FrontX = NoirLayout::SuspectX - Look.TableDistance + 35.f - Slot->HandsInset;
					const FVector BodyLoc = Panel->GetRelativeLocation();
					const float K = (FrontX - CamLocal.X) / FMath::Max(BodyLoc.X - CamLocal.X, 1.f);
					Hands->SetRelativeLocation(CamLocal + (BodyLoc - CamLocal) * K);
					Hands->SetRelativeRotation(Panel->GetRelativeRotation());
					Hands->SetRelativeScale3D(Panel->GetRelativeScale3D() * FVector(K, K, 1.f));
					if (UMaterialInstanceDynamic* HandsMid = Cast<UMaterialInstanceDynamic>(Hands->GetMaterial(0)))
					{
						HandsMid->SetScalarParameterValue(TEXT("Brightness"), Slot->HandsBrightness);
						HandsMid->SetScalarParameterValue(TEXT("Lit"), Slot->VideoLit);
					}
				}
			}
		}
	}
	if (bChanged)
	{
		RefreshVisibility();
	}
}

void ANoirRoom::Capture()
{
	const FIntPoint Res = DesiredResolution();
	if (CaptureTarget->SizeX != Res.X || CaptureTarget->SizeY != Res.Y)
	{
		CaptureTarget->ResizeTarget(Res.X, Res.Y);
	}
	CaptureComponent->CaptureScene();
}
