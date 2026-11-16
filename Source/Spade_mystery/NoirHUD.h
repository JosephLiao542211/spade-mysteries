#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "NoirHUD.generated.h"

enum class ENoirAction : uint8;

class ANoirPlayerController;
class UCanvas;
class UFont;
class UTexture2D;
class UTextureRenderTarget2D;

/**
 * Produces every frame as a strict 1-bit image (pure black / pure white):
 *   1. ANoirRoom renders the 3D scene into a low-res HDR target.
 *   2. The UI is drawn into a second low-res target as masks (red = white ink, green = black box,
 *      blue = white box; hovered buttons are a white box with green = black text).
 *   3. Both are read back; the scene gets harsh contrast + a dither screen, the UI is thresholded.
 *   4. The result is uploaded to a texture and drawn full screen with nearest-neighbour scaling.
 * All look parameters live in UNoirLookSettings (Project Settings > Game > Noir Look).
 */
UCLASS()
class SPADE_MYSTERY_API ANoirHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/** Writes the last composited 1-bit frame to a PNG (used by the editor tooling to check the look). */
	UFUNCTION(BlueprintCallable, Category = "Noir")
	bool SaveFrame(const FString& Path) const;

	/** Which button (if any) is under this viewport pixel position. */
	ENoirAction ActionAt(const FVector2D& ScreenPos) const;

private:
	void EnsureResources(const FIntPoint& Res);
	void DrawUi(UCanvas* C, const ANoirPlayerController& PC);
	void Compose();

	void DrawTalkPanel(UCanvas* C, const ANoirPlayerController& PC, double Now);

	// UI helpers. Coordinates are in a fixed 640x360 layout space, scaled to the real resolution.
	void Fill(UCanvas* C, float X, float Y, float W, float H, const FLinearColor& Col);
	void Box(UCanvas* C, float X, float Y, float W, float H);
	void Ink(UCanvas* C, const FString& Text, float X, float Y, float Scale, const FColor& Col = FColor::Red);
	/** Bordered clickable button; inverts when hovered. Returns its width. */
	float Button(UCanvas* C, const FString& Label, float X, float Y, float Scale, ENoirAction Action);
	float ButtonW(const FString& Label, float Scale) const;
	float ButtonH(float Scale) const;
	/** Borderless clickable row spanning W; inverts when hovered. */
	void Row(UCanvas* C, const FString& Label, float X, float Y, float W, float Scale, ENoirAction Action);
	bool Hovered(float X, float Y, float W, float H) const;
	TArray<FString> Wrap(const FString& Text, float MaxW, float Scale) const;
	void InkCentered(UCanvas* C, const FString& Text, float Y, float Scale);
	float InkWrapped(UCanvas* C, const FString& Text, float X, float Y, float MaxW, float Scale, int32 MaxChars = MAX_int32);
	float TextW(const FString& Text, float Scale) const;
	float LineH(float Scale) const;

	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> UiTarget;
	UPROPERTY() TObjectPtr<UTexture2D> Output;
	UPROPERTY() TObjectPtr<UFont> Font;

	TArray<FLinearColor> ScenePixels;
	TArray<FColor> UiPixels;
	TArray<FColor> Frame;
	TArray<float> Threshold;
	uint32 ThresholdKey = 0;
	int32 RX = 0;
	int32 RY = 0;
	float UiScale = 1.f;
	float Exposure = 1.f;
	int32 ShownSuspect = -2;

	// Where the 1-bit image lands on screen, for mouse hit-testing.
	FVector2D DrawOrigin = FVector2D::ZeroVector;
	float DrawScale = 1.f;
	FVector2D Mouse = FVector2D(-1, -1); // in layout space
	TArray<TPair<FBox2D, ENoirAction>> Buttons; // in layout space, rebuilt each frame
};
