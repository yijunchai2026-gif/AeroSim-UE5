#pragma once

#include "CoreMinimal.h"
#include "Phase7Product.h"

/** Presentation-only navigation; mission/simulation truth remains with their existing owners. */
enum class EProductPage : uint8
{
    Home, Hangar, Mission, Preflight, Flight, Settings, VisualLab, Result, Replay
};

enum class EProductSettingsCategory : uint8
{
    Display, Graphics, Audio, Controls, Camera, Accessibility
};

enum class EProductSettingField : uint8
{
    Resolution, WindowMode, Quality, MasterVolume, VerticalFov, HudScale, TextScale, ReducedMotion
};

enum class EProductSettingsAction : uint8 { Preview, Apply, Cancel, Rollback };
enum class EProductInputKind : uint8 { KeyboardMouse, Gamepad };

struct AEROSIM_API FProductShellState final
{
    EProductPage Page = EProductPage::Home;
    EProductPage ReturnPage = EProductPage::Home;
    EProductSettingsCategory SettingsCategory = EProductSettingsCategory::Display;
    EProductInputKind InputKind = EProductInputKind::KeyboardMouse;
    FString Status;

    bool Open(EProductPage Next, bool bMissionActive);
    void CloseOverlay();
    static bool IsOverlay(EProductPage Value);
    static bool IsValidPage(EProductPage Value);
};

struct AEROSIM_API FProductLabState final
{
    bool bReferenceLocked = false;
    bool bShowingReference = false;
    EPhase7QualityTier ReferenceQuality = EPhase7QualityTier::DesktopHigh;
    EPhase7QualityTier ComparisonQuality = EPhase7QualityTier::DesktopLow;
    int32 EnvironmentRevision = 0;
    int32 Seed = 21630;
    float VerticalFov = 60.0F;
    FString Status = TEXT("NOT_CAPTURED - LOCK A REFERENCE FIRST");
};

/** Bounded real diagnostic sample, not the Shipping qualification protocol. */
struct AEROSIM_API FProductDiagnosticSample final
{
    double FrameMs = 0.0;
    double CpuGameMs = 0.0;
    double CpuRenderMs = 0.0;
    double GpuMs = 0.0;
    double ResidentMemoryMiB = 0.0;
    int32 DrawCalls = 0;
    double TextureStreamingMiB = 0.0;
    double TexturePoolMiB = 0.0;
    int32 Samples = 0;
    FString Summary = TEXT("NOT_MEASURED - WAIT FOR DIAGNOSTIC SAMPLES");
};
