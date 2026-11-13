#include "NoirHUD.h"
#include "NoirLookSettings.h"
#include "NoirPlayerController.h"
#include "NoirRoom.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "TextureResource.h"
#include "ImageUtils.h"

namespace
{
	// UI is laid out in this space and scaled to the actual 1-bit resolution.
	constexpr float LX = 640.f;
	constexpr float LY = 360.f;
	const FLinearColor InkColor(1, 0, 0, 1);  // UI mask: becomes white
	const FLinearColor BoxColor(0, 1, 0, 1);  // UI mask: becomes black
	const FLinearColor LitColor(0, 0, 1, 1);  // UI mask: becomes white (hovered button fill)
	const FColor DarkInk(0, 255, 0, 255);     // text drawn on a LitColor fill: becomes black
	constexpr float Small = 0.8f;             // secondary text scale
	constexpr float Body = 1.0f;              // base text scale

	float BayerValue(int32 X, int32 Y, int32 Size)
	{
		// Recursive Bayer index, normalised to (0,1).
		int32 V = 0;
		for (int32 Bit = Size >> 1, Mul = 1; Bit > 0; Bit >>= 1, Mul <<= 2)
		{
			const int32 BX = (X & Bit) ? 1 : 0;
			const int32 BY = (Y & Bit) ? 1 : 0;
			V += Mul * (((BX ^ BY) << 1) | BY); // 2x2 Bayer cell: 0 2 / 3 1
		}
		return (V + 0.5f) / float(Size * Size);
	}
}

void ANoirHUD::EnsureResources(const FIntPoint& Res)
{
	const UNoirLookSettings& Look = UNoirLookSettings::Get();

	if (RX != Res.X || RY != Res.Y)
	{
		RX = Res.X;
		RY = Res.Y;
		Output = nullptr;
		if (UiTarget)
		{
			UiTarget->ResizeTarget(RX, RY);
		}
	}
	UiScale = RY / LY;

	if (!UiTarget)
	{
		UiTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, RX, RY, RTF_RGBA8, FLinearColor::Black, false);
	}
	if (!Output)
	{
		Output = UTexture2D::CreateTransient(RX, RY, PF_B8G8R8A8);
		Output->Filter = TF_Nearest;
		Output->SRGB = false;
		Output->UpdateResource();
	}
	if (!Font)
	{
		Font = GEngine->GetSmallFont();
	}

	// Rebuild the dither screen whenever its inputs change.
	const uint32 Key = HashCombine(HashCombine(GetTypeHash(RX), GetTypeHash(RY)),
		HashCombine(HashCombine(GetTypeHash(uint8(Look.Pattern)), GetTypeHash(Look.CellSize)), GetTypeHash(Look.ScreenAngle)));
	if (Key != ThresholdKey || Threshold.Num() != RX * RY)
	{
		ThresholdKey = Key;
		Threshold.SetNumUninitialized(RX * RY);
		const float A = FMath::DegreesToRadians(Look.ScreenAngle);
		const float K = 2.f * PI / FMath::Max(Look.CellSize, 1.f);
		const float CA = FMath::Cos(A), SA = FMath::Sin(A);
		for (int32 Y = 0; Y < RY; ++Y)
		{
			for (int32 X = 0; X < RX; ++X)
			{
				const float U = (X * CA + Y * SA) * K;
				const float V = (-X * SA + Y * CA) * K;
				float T = 0.5f;
				switch (Look.Pattern)
				{
				case ENoirDitherPattern::Halftone:  T = 0.5f + 0.25f * (FMath::Cos(U) + FMath::Cos(V)); break;
				case ENoirDitherPattern::Lines:     T = 0.5f + 0.5f * FMath::Cos(V); break;
				case ENoirDitherPattern::Bayer4:    T = BayerValue(X & 3, Y & 3, 4); break;
				case ENoirDitherPattern::Bayer8:    T = BayerValue(X & 7, Y & 7, 8); break;
				case ENoirDitherPattern::Threshold: T = 0.5f; break;
				}
				Threshold[Y * RX + X] = T;
			}
		}
	}
}

void ANoirHUD::DrawHUD()
{
	Super::DrawHUD();

	ANoirPlayerController* PC = Cast<ANoirPlayerController>(GetOwningPlayerController());
	if (!PC || !PC->Room || !Canvas)
	{
		return;
	}
	EnsureResources(ANoirRoom::DesiredResolution());

	// Who sits in the chair.
	const int32 Want = (PC->State == ENoirState::Briefing) ? INDEX_NONE : PC->Current;
	if (Want != ShownSuspect)
	{
		PC->Room->ShowSuspect(Want);
		ShownSuspect = Want;
	}

	// Where the image will land on screen (integer-scaled when the window allows for perfectly square pixels).
	const float SW = Canvas->SizeX, SH = Canvas->SizeY;
	float Scale = FMath::Min(SW / RX, SH / RY);
	if (Scale >= 1.f)
	{
		Scale = FMath::FloorToFloat(Scale);
	}
	const float DW = RX * Scale, DH = RY * Scale;
	DrawScale = Scale;
	DrawOrigin = FVector2D(FMath::RoundToFloat((SW - DW) * 0.5f), FMath::RoundToFloat((SH - DH) * 0.5f));
	float MX = 0.f, MY = 0.f;
	Mouse = PC->GetMousePosition(MX, MY) ? (FVector2D(MX, MY) - DrawOrigin) / (DrawScale * UiScale) : FVector2D(-1, -1);

	// 1. Scene.
	PC->Room->Capture();

	// 2. UI masks.
	UKismetRenderingLibrary::ClearRenderTarget2D(this, UiTarget, FLinearColor::Black);
	UCanvas* UiCanvas = nullptr;
	FVector2D UiSize;
	FDrawToRenderTargetContext Ctx;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, UiTarget, UiCanvas, UiSize, Ctx);
	if (UiCanvas)
	{
		DrawUi(UiCanvas, *PC);
	}
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Ctx);

	// 3. Read back + dither.
	PC->Room->GetCaptureTarget()->GameThread_GetRenderTargetResource()->ReadLinearColorPixels(ScenePixels);
	UiTarget->GameThread_GetRenderTargetResource()->ReadPixels(UiPixels);
	Compose();

	// 4. Show it.
	DrawRect(FLinearColor::Black, 0, 0, SW, SH);
	DrawTexture(Output, DrawOrigin.X, DrawOrigin.Y, DW, DH, 0, 0, 1, 1, FLinearColor::White, BLEND_Opaque);
}

ENoirAction ANoirHUD::ActionAt(const FVector2D& ScreenPos) const
{
	const FVector2D P = (ScreenPos - DrawOrigin) / (DrawScale * UiScale);
	for (int32 i = Buttons.Num() - 1; i >= 0; --i)
	{
		if (Buttons[i].Key.IsInsideOrOn(P))
		{
			return Buttons[i].Value;
		}
	}
	return ENoirAction::None;
}

void ANoirHUD::Compose()
{
	const int32 N = RX * RY;
	if (ScenePixels.Num() != N || UiPixels.Num() != N)
	{
		return; // a resize is in flight; next frame catches up
	}
	const UNoirLookSettings& Look = UNoirLookSettings::Get();

	// Auto-exposure: map the chosen brightness percentile to white.
	TArray<float> Samples;
	Samples.Reserve(N / 16 + 1);
	for (int32 i = 0; i < N; i += 16)
	{
		const FLinearColor& P = ScenePixels[i];
		const float L = 0.2126f * P.R + 0.7152f * P.G + 0.0722f * P.B;
		Samples.Add(FMath::IsFinite(L) ? L : 0.f);
	}
	Samples.Sort();
	const int32 PeakIdx = FMath::Clamp(int32(Samples.Num() * Look.WhitePercentile), 0, Samples.Num() - 1);
	const float Peak = FMath::Max(Samples[PeakIdx], 1e-6f);
	Exposure = FMath::Lerp(Exposure, 1.f / Peak, FMath::Clamp(Look.AdaptSpeed, 0.f, 1.f));
	const float Gain = Exposure * Look.ExposureBias;
	const float InvGamma = 1.f / FMath::Max(Look.Gamma, 0.01f);

	Frame.SetNumUninitialized(N);
	for (int32 i = 0; i < N; ++i)
	{
		const FColor& Ui = UiPixels[i];
		bool bWhite;
		if (Ui.R > 110)
		{
			bWhite = true;              // text / borders (never inverted, stays readable)
		}
		else if (Ui.G > 128)
		{
			bWhite = false;             // UI box background / text on a hovered button
		}
		else if (Ui.B > 128)
		{
			bWhite = true;              // hovered button fill
		}
		else
		{
			const FLinearColor& P = ScenePixels[i];
			float L = (0.2126f * P.R + 0.7152f * P.G + 0.0722f * P.B) * Gain;
			L = FMath::IsFinite(L) ? FMath::Pow(FMath::Clamp(L, 0.f, 1.f), InvGamma) : 0.f;
			const float C = FMath::Clamp((L - Look.Pivot) * Look.Contrast + 0.5f, 0.f, 1.f);
			bWhite = (C > Threshold[i]) != Look.bInvert;
		}
		const uint8 V = bWhite ? 255 : 0;
		Frame[i] = FColor(V, V, V, 255);
	}

	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, RX, RY);
	uint8* Data = new uint8[N * 4];
	FMemory::Memcpy(Data, Frame.GetData(), N * 4);
	Output->UpdateTextureRegions(0, 1, Region, RX * 4, 4, Data,
		[](uint8* SrcData, const FUpdateTextureRegion2D* Regions)
		{
			delete[] SrcData;
			delete Regions;
		});
}

// ---------------------------------------------------------------------------
// UI (laid out in 640x360 space)
// ---------------------------------------------------------------------------

void ANoirHUD::DrawUi(UCanvas* C, const ANoirPlayerController& PC)
{
	Buttons.Reset();
	const double Now = GetWorld()->GetRealTimeSeconds();

	switch (PC.State)
	{
	case ENoirState::Briefing:
	{
		Box(C, 70, 40, LX - 140, 200);
		InkCentered(C, TEXT("THE BLACK SPADE KILLING"), 52, Body * 1.6f);
		InkWrapped(C, ANoirPlayerController::GetBriefing(), 86, 88, LX - 172, Body);
		const FString Label = TEXT("BEGIN [SPACE]");
		Button(C, Label, (LX - ButtonW(Label, Body)) * 0.5f, 208, Body, ENoirAction::Begin);
		break;
	}

	case ENoirState::Interrogate:
	case ENoirState::ConfirmAccuse:
	{
		const FNoirSuspect& Sus = PC.Suspects[PC.Current];

		// Small name tag.
		const float TagW = FMath::Max(TextW(Sus.Name, Body), TextW(Sus.Role, Small)) + 10;
		Box(C, 8, 8, TagW, LineH(Body) + LineH(Small) + 6);
		Ink(C, Sus.Name, 13, 11, Body);
		Ink(C, Sus.Role, 13, 11 + LineH(Body), Small);

		if (PC.bTalking)
		{
			DrawTalkPanel(C, PC, Now);
		}
		else if (PC.State == ENoirState::Interrogate)
		{
			// A slim row of buttons along the bottom; the scene stays clear.
			const FString Count = FString::Printf(TEXT("%d/%d"), PC.Current + 1, PC.Suspects.Num());
			const FString TalkL = TEXT("TALK [T]");
			const FString AccuseL = TEXT("ACCUSE [X]");
			const float Gap = 4;
			const float RowW = ButtonW(TEXT("<"), Small) + ButtonW(Count, Small) + ButtonW(TEXT(">"), Small)
				+ ButtonW(TalkL, Small) + ButtonW(AccuseL, Small) + Gap * 4 + 8;
			const float Y = LY - 8 - ButtonH(Small);
			float X = (LX - RowW) * 0.5f;
			X += Button(C, TEXT("<"), X, Y, Small, ENoirAction::Prev) + Gap;
			Box(C, X, Y, ButtonW(Count, Small), ButtonH(Small));
			Ink(C, Count, X + 6, Y + 3, Small);
			X += ButtonW(Count, Small) + Gap;
			X += Button(C, TEXT(">"), X, Y, Small, ENoirAction::Next) + Gap + 8;
			X += Button(C, TalkL, X, Y, Small, ENoirAction::Talk) + Gap;
			Button(C, AccuseL, X, Y, Small, ENoirAction::Accuse);
		}

		if (PC.State == ENoirState::ConfirmAccuse)
		{
			Box(C, 120, 120, LX - 240, 76);
			InkCentered(C, FString::Printf(TEXT("ACCUSE %s?"), *Sus.Name.ToUpper()), 132, Body * 1.4f);
			const FString YesL = TEXT("YES [Y]");
			const FString NoL = TEXT("NOT YET [N]");
			const float W = ButtonW(YesL, Body) + 12 + ButtonW(NoL, Body);
			float X = (LX - W) * 0.5f;
			X += Button(C, YesL, X, 164, Body, ENoirAction::Yes) + 12;
			Button(C, NoL, X, 164, Body, ENoirAction::No);
		}
		break;
	}

	case ENoirState::Solved:
	case ENoirState::Failed:
	{
		const bool bWon = PC.State == ENoirState::Solved;
		Box(C, 0, 0, LX, LY);
		InkCentered(C, bWon ? TEXT("CASE CLOSED") : TEXT("WRONG MAN"), 70, Body * 2.2f);
		InkWrapped(C, PC.GetEnding(), 90, 130, LX - 180, Body);
		const FString Label = TEXT("REOPEN THE CASE [R]");
		Button(C, Label, (LX - ButtonW(Label, Body)) * 0.5f, 296, Body, ENoirAction::Restart);
		break;
	}
	}
}

void ANoirHUD::DrawTalkPanel(UCanvas* C, const ANoirPlayerController& PC, double Now)
{
	const FNoirSuspect& Sus = PC.Suspects[PC.Current];
	const TArray<FString>& Questions = ANoirPlayerController::GetQuestions();

	const float PX = 8, PW = LX - 16, PH = 140;
	const float PY = LY - 8 - PH;
	Box(C, PX, PY, PW, PH);

	// Header: who you're talking to, and a way out.
	FString First, Rest;
	if (!Sus.Name.Split(TEXT(" "), &First, &Rest))
	{
		First = Sus.Name;
	}
	First = First.ToUpper();
	const FString CloseL = TEXT("CLOSE [T]");
	Button(C, CloseL, PX + PW - 4 - ButtonW(CloseL, Small), PY + 4, Small, ENoirAction::Talk);
	const FString NavL = TEXT("<");
	const float NavX = PX + 6;
	const float AfterNav = NavX + Button(C, NavL, NavX, PY + 4, Small, ENoirAction::Prev) + 3;
	const float NameX = AfterNav + Button(C, TEXT(">"), AfterNav, PY + 4, Small, ENoirAction::Next) + 6;
	Ink(C, FString::Printf(TEXT("TALKING TO %s"), *Sus.Name.ToUpper()), NameX, PY + 7, Small);

	// Questions, along the bottom of the panel.
	const float RowH = LineH(Body) + 2;
	const float QY = PY + PH - 4 - RowH * Questions.Num();
	Fill(C, PX + 6, QY - 3, PW - 12, 1, InkColor);
	for (int32 i = 0; i < Questions.Num(); ++i)
	{
		const TCHAR* Mark = PC.WasAsked(PC.Current, i) ? TEXT("*") : TEXT(" ");
		Row(C, FString::Printf(TEXT("[%d]%s %s"), i + 1, Mark, *Questions[i]), PX + 6, QY + RowH * i, PW - 12, Body, ENoirAction(int32(ENoirAction::Ask1) + i));
	}

	// Transcript, newest at the bottom; older lines scroll off the top.
	const float TopY = PY + 6 + ButtonH(Small) + 2;
	const float LH = LineH(Body);
	const int32 MaxLines = FMath::Max(1, FMath::FloorToInt32((QY - 5 - TopY) / LH));
	const float TextX = PX + 10, TextWMax = PW - 20;

	TArray<FString> Lines;
	for (int32 e = 0; e < Sus.Log.Num(); ++e)
	{
		const FNoirLine& L = Sus.Log[e];
		const FString Full = L.bPlayer
			? FString::Printf(TEXT("YOU: \"%s\""), *L.Text)
			: FString::Printf(TEXT("%s: %s"), *First, *L.Text);
		TArray<FString> Wrapped = Wrap(Full, TextWMax, Body);
		if (!L.bPlayer && e == Sus.Log.Num() - 1)
		{
			// Typewriter on the latest answer. Lines are laid out first so the reveal doesn't reflow.
			int32 Remaining = FMath::FloorToInt32((Now - L.Time) * 50.0) + First.Len() + 2;
			for (FString& W : Wrapped)
			{
				const int32 Len = W.Len();
				W = W.Left(FMath::Max(Remaining, 0));
				Remaining -= Len + 1;
			}
		}
		Lines.Append(Wrapped);
	}
	const int32 Start = FMath::Max(0, Lines.Num() - MaxLines);
	float Y = TopY + (MaxLines - (Lines.Num() - Start)) * LH;
	for (int32 i = Start; i < Lines.Num(); ++i)
	{
		Ink(C, Lines[i], TextX, Y, Body);
		Y += LH;
	}
}

void ANoirHUD::Fill(UCanvas* C, float X, float Y, float W, float H, const FLinearColor& Col)
{
	const float U = UiScale;
	FCanvasTileItem Tile(FVector2D(FMath::RoundToFloat(X * U), FMath::RoundToFloat(Y * U)),
		FVector2D(FMath::Max(1.f, FMath::RoundToFloat(W * U)), FMath::Max(1.f, FMath::RoundToFloat(H * U))), Col);
	Tile.BlendMode = SE_BLEND_Opaque;
	C->DrawItem(Tile);
}

void ANoirHUD::Box(UCanvas* C, float X, float Y, float W, float H)
{
	const float T = 1.f / UiScale; // one real pixel, in layout units
	Fill(C, X, Y, W, H, BoxColor);
	Fill(C, X, Y, W, T, InkColor);
	Fill(C, X, Y + H - T, W, T, InkColor);
	Fill(C, X, Y, T, H, InkColor);
	Fill(C, X + W - T, Y, T, H, InkColor);
}

bool ANoirHUD::Hovered(float X, float Y, float W, float H) const
{
	return Mouse.X >= X && Mouse.X <= X + W && Mouse.Y >= Y && Mouse.Y <= Y + H;
}

float ANoirHUD::ButtonW(const FString& Label, float Scale) const
{
	return TextW(Label, Scale) + 12;
}

float ANoirHUD::ButtonH(float Scale) const
{
	return LineH(Scale) + 6;
}

float ANoirHUD::Button(UCanvas* C, const FString& Label, float X, float Y, float Scale, ENoirAction Action)
{
	const float W = ButtonW(Label, Scale), H = ButtonH(Scale);
	Buttons.Emplace(FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H)), Action);
	if (Hovered(X, Y, W, H))
	{
		Fill(C, X, Y, W, H, LitColor);
		Ink(C, Label, X + 6, Y + 3, Scale, DarkInk);
	}
	else
	{
		Box(C, X, Y, W, H);
		Ink(C, Label, X + 6, Y + 3, Scale);
	}
	return W;
}

void ANoirHUD::Row(UCanvas* C, const FString& Label, float X, float Y, float W, float Scale, ENoirAction Action)
{
	const float H = LineH(Scale) + 2;
	Buttons.Emplace(FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H)), Action);
	if (Hovered(X, Y, W, H))
	{
		Fill(C, X, Y, W, H, LitColor);
		Ink(C, Label, X + 4, Y + 1, Scale, DarkInk);
	}
	else
	{
		Ink(C, Label, X + 4, Y + 1, Scale);
	}
}

void ANoirHUD::Ink(UCanvas* C, const FString& Text, float X, float Y, float Scale, const FColor& Col)
{
	C->SetDrawColor(Col);
	C->DrawText(Font, Text, FMath::RoundToFloat(X * UiScale), FMath::RoundToFloat(Y * UiScale), Scale * UiScale, Scale * UiScale);
}

void ANoirHUD::InkCentered(UCanvas* C, const FString& Text, float Y, float Scale)
{
	Ink(C, Text, (LX - TextW(Text, Scale)) * 0.5f, Y, Scale);
}

float ANoirHUD::TextW(const FString& Text, float Scale) const
{
	return Font ? Font->GetStringSize(*Text) * Scale : 0.f;
}

float ANoirHUD::LineH(float Scale) const
{
	return Font ? Font->GetMaxCharHeight() * Scale : 12.f;
}

TArray<FString> ANoirHUD::Wrap(const FString& Text, float MaxW, float Scale) const
{
	TArray<FString> Words;
	Text.ParseIntoArray(Words, TEXT(" "));
	TArray<FString> Lines;
	FString Line;
	for (const FString& Word : Words)
	{
		const FString Candidate = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
		if (TextW(Candidate, Scale) > MaxW && !Line.IsEmpty())
		{
			Lines.Add(Line);
			Line = Word;
		}
		else
		{
			Line = Candidate;
		}
	}
	if (!Line.IsEmpty())
	{
		Lines.Add(Line);
	}
	return Lines;
}

float ANoirHUD::InkWrapped(UCanvas* C, const FString& Text, float X, float Y, float MaxW, float Scale, int32 MaxChars)
{
	int32 Remaining = MaxChars;
	for (const FString& L : Wrap(Text, MaxW, Scale))
	{
		if (Remaining > 0)
		{
			Ink(C, L.Left(Remaining), X, Y, Scale);
			Remaining -= L.Len() + 1;
		}
		Y += LineH(Scale);
	}
	return Y;
}

bool ANoirHUD::SaveFrame(const FString& Path) const
{
	if (Frame.Num() != RX * RY || RX <= 0)
	{
		return false;
	}
	const FImageView Image(Frame.GetData(), RX, RY, EGammaSpace::sRGB);
	return FImageUtils::SaveImageByExtension(*Path, Image);
}
