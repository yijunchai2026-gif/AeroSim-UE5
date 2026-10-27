#include "AeroSimGameMode.h"
#include "AeroSimControlBridge.h"

#include "FlightHUD.h"
#include "FlightPawn.h"
#include "AeroSimProductWidget.h"
#include "AeroSimPlayerController.h"
#include "AeroSimRuntimeContract.h"
#include "ProductEvidence.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "ImageUtils.h"
#include "Engine/GameViewportClient.h"
#include "AudioDevice.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMemory.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "DynamicRHI.h"
#include "RHIStats.h"
#include "RenderTimer.h"
#include "PacificCoastGraybox.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/SpringArmComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "ConvexVolume.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "ProceduralMeshComponent.h"
#include "UObject/UObjectIterator.h"

namespace
{
// Deterministic CPU bounds test against the captured view: an upper bound on base-pass geometry, not RHI draw calls.
void AddFrustumGeometryBudget(UWorld* World, FMinimalViewInfo View, const int32 Width, const int32 Height, const TSharedRef<FJsonObject>& Manifest)
{
    if (World == nullptr || Width <= 0 || Height <= 0) return;
    View.AspectRatio = static_cast<float>(Width) / Height;
    FMatrix ViewMatrix, ProjectionMatrix, ViewProjection;
    UGameplayStatics::GetViewProjectionMatrix(View, ViewMatrix, ProjectionMatrix, ViewProjection);
    FConvexVolume Frustum;
    GetViewFrustumBounds(Frustum, ViewProjection, false);
    int64 Components = 0, Instances = 0, Sections = 0, Triangles = 0, Other = 0;
    auto WithinDrawDistance = [&View](const FVector& Center, const float MaxDistance)
    {
        return MaxDistance <= 0.0F || FVector::Dist(View.Location, Center) <= MaxDistance;
    };
    for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
    {
        UPrimitiveComponent* Primitive = *It;
        if (Primitive->GetWorld() != World || !Primitive->IsRegistered() || !Primitive->IsVisible() || Primitive->bHiddenInGame) continue;
        if (const AActor* Owner = Primitive->GetOwner(); Owner != nullptr && Owner->IsHidden()) continue;
        if (!Frustum.IntersectBox(Primitive->Bounds.Origin, Primitive->Bounds.BoxExtent)
            || !WithinDrawDistance(Primitive->Bounds.Origin, Primitive->CachedMaxDrawDistance)) continue;
        if (const UInstancedStaticMeshComponent* Instanced = Cast<UInstancedStaticMeshComponent>(Primitive))
        {
            const UStaticMesh* Mesh = Instanced->GetStaticMesh();
            if (Mesh == nullptr) continue;
            const FBox MeshBox = Mesh->GetBounds().GetBox();
            int32 StartCull = 0, EndCull = 0;
            Instanced->GetCullDistances(StartCull, EndCull);
            int64 Visible = 0;
            for (int32 Index = 0; Index < Instanced->GetInstanceCount(); ++Index)
            {
                FTransform Transform;
                if (!Instanced->GetInstanceTransform(Index, Transform, true)) continue;
                const FBox Box = MeshBox.TransformBy(Transform);
                if (Frustum.IntersectBox(Box.GetCenter(), Box.GetExtent()) && WithinDrawDistance(Box.GetCenter(), static_cast<float>(EndCull))) ++Visible;
            }
            if (Visible == 0) continue;
            ++Components; Instances += Visible;
            Sections += Mesh->GetNumSections(0);
            Triangles += Visible * Mesh->GetNumTriangles(0);
        }
        else if (const UStaticMeshComponent* Static = Cast<UStaticMeshComponent>(Primitive))
        {
            const UStaticMesh* Mesh = Static->GetStaticMesh();
            if (Mesh == nullptr) continue;
            ++Components; ++Instances;
            Sections += Mesh->GetNumSections(0);
            Triangles += Mesh->GetNumTriangles(0);
        }
        else if (UProceduralMeshComponent* Procedural = Cast<UProceduralMeshComponent>(Primitive))
        {
            ++Components; ++Instances;
            for (int32 Index = 0; Index < Procedural->GetNumSections(); ++Index)
            {
                const FProcMeshSection* Section = Procedural->GetProcMeshSection(Index);
                if (Section == nullptr || !Section->bSectionVisible || Section->ProcIndexBuffer.Num() == 0) continue;
                ++Sections;
                Triangles += Section->ProcIndexBuffer.Num() / 3;
            }
        }
        else
        {
            ++Other;
        }
    }
    Manifest->SetNumberField(TEXT("scene_frustum_mesh_components"), static_cast<double>(Components));
    Manifest->SetNumberField(TEXT("scene_frustum_mesh_instances"), static_cast<double>(Instances));
    Manifest->SetNumberField(TEXT("scene_frustum_mesh_sections"), static_cast<double>(Sections));
    Manifest->SetNumberField(TEXT("scene_frustum_triangles_lod0"), static_cast<double>(Triangles));
    Manifest->SetNumberField(TEXT("scene_frustum_other_primitives"), static_cast<double>(Other));
    Manifest->SetStringField(TEXT("scene_geometry_scope"),
        TEXT("CPU frustum test of component/instance bounds; LOD0 triangles; no occlusion, shadow or Lumen passes; not RHI draw calls"));
}
}

AAeroSimGameMode::AAeroSimGameMode()
{
    ControlBridge = CreateDefaultSubobject<UAeroSimControlBridge>(TEXT("LocalControlBridge"));
    Phase4Mission.Reset();
    DefaultPawnClass = AFlightPawn::StaticClass();
    HUDClass = AFlightHUD::StaticClass();
    PlayerControllerClass = AAeroSimPlayerController::StaticClass();
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bTickEvenWhenPaused = true;
}

void AAeroSimGameMode::StartPlay()
{
    Super::StartPlay();
    Phase4Mission.Reset();
    LoadProductSettings();
    LastProductTickSeconds = FPlatformTime::Seconds();

    FString WeatherName;
    if (FParse::Value(FCommandLine::Get(), TEXT("AeroSimWeather="), WeatherName))
    {
        if (WeatherName.Equals(TEXT("Overcast"), ESearchCase::IgnoreCase))
        {
            EnvironmentState.SetWeather(EAeroSimWeather::Overcast);
        }
        else if (WeatherName.Equals(TEXT("Rain"), ESearchCase::IgnoreCase))
        {
            EnvironmentState.SetWeather(EAeroSimWeather::Rain);
        }
    }

    if (UWorld* World = GetWorld())
    {
        Graybox = World->SpawnActor<APacificCoastGraybox>();
        if (Graybox != nullptr)
        {
            Graybox->ApplyEnvironmentState(EnvironmentState);
            if(APlayerController* Controller=World->GetFirstPlayerController())
                if(AFlightPawn* Pawn=Cast<AFlightPawn>(Controller->GetPawn()))
                    Pawn->InitializeGroundedSpawn(FVector(55,79,APacificCoastGraybox::GetCoastalLandHeight(55,79)+1.8F));
        }

        FString ReviewName;
        if (FParse::Value(FCommandLine::Get(), TEXT("AeroSimReview="), ReviewName))
        {
            APlayerController* Controller = World->GetFirstPlayerController();
            AFlightPawn* Pawn = Controller != nullptr ? Cast<AFlightPawn>(Controller->GetPawn()) : nullptr;
            if (Pawn != nullptr)
            {
                FTransform View(FRotator::ZeroRotator, FVector(-5000.0F, -5000.0F, 1800.0F));
                EFlightCameraMode CameraMode = EFlightCameraMode::Chase;
                float ReviewArmLength = -1.0F;
                if (ReviewName.Equals(TEXT("Aircraft"), ESearchCase::IgnoreCase))
                {
                    View = FTransform(FRotator(0.0F, 28.0F, 0.0F), FVector(-5100.0F, -4900.0F, 60.0F));
                    ReviewArmLength = 105.0F;
                }
                else if (ReviewName.Equals(TEXT("FPV"), ESearchCase::IgnoreCase))
                {
                    View = FTransform(FRotator(-3.0F, 18.0F, 0.0F), FVector(-3500.0F, -4500.0F, 700.0F));
                    CameraMode = EFlightCameraMode::FPV;
                }
                else if (ReviewName.Equals(TEXT("Gimbal"), ESearchCase::IgnoreCase))
                {
                    View = FTransform(FRotator(0.0F, 22.0F, 0.0F), FVector(-2500.0F, -5200.0F, 1700.0F));
                    CameraMode = EFlightCameraMode::Gimbal;
                    Pawn->SetGimbalTarget(-78.0F, 0.0F);
                }
                else if (ReviewName.Equals(TEXT("NearFollow"), ESearchCase::IgnoreCase))
                {
                    View = FTransform(FRotator(0.0F, 22.0F, 0.0F), FVector(-5000.0F, -5000.0F, 400.0F));
                    ReviewArmLength = 250.0F;
                }
                else if (ReviewName.Equals(TEXT("FarFollow"), ESearchCase::IgnoreCase))
                {
                    View = FTransform(FRotator(0.0F, 22.0F, 0.0F), FVector(-5000.0F, -5000.0F, 800.0F));
                    CameraMode = EFlightCameraMode::FarFollow;
                    ReviewArmLength = 700.0F;
                }
                else if (ReviewName.Equals(TEXT("ScaleSupport"), ESearchCase::IgnoreCase))
                {
                    View = FTransform(FRotator(0.0F, 35.0F, 0.0F), FVector(-6200.0F, -4300.0F, 60.0F));
                    ReviewArmLength = 155.0F;
                }
                else if (ReviewName.Equals(TEXT("Rock"), ESearchCase::IgnoreCase))
                {
                    View = FTransform(FRotator(-6.0F, 8.0F, 0.0F), FVector(1500.0F, -9000.0F, 1700.0F));
                    CameraMode = EFlightCameraMode::FPV;
                }
                else if (ReviewName.Equals(TEXT("Boardwalk"), ESearchCase::IgnoreCase))
                {
                    View = FTransform(FRotator(-7.0F, 20.0F, 0.0F), FVector(-17500.0F, 5000.0F, 1800.0F));
                }
                else if (ReviewName.Equals(TEXT("Overview"), ESearchCase::IgnoreCase))
                {
                    View = FTransform(FRotator(-18.0F, 32.0F, 0.0F), FVector(-22000.0F, -12000.0F, 6000.0F));
                    CameraMode = EFlightCameraMode::FarFollow;
                }
                Pawn->ConfigureReviewView(View, CameraMode);
                if (ReviewArmLength > 0.0F && Pawn->GetFollowSpringArm() != nullptr)
                {
                    Pawn->GetFollowSpringArm()->TargetArmLength = ReviewArmLength;
                }
            }
        }

        FString Phase4View;
        if (FParse::Value(FCommandLine::Get(), TEXT("AeroSimPhase4View="), Phase4View))
        {
            EPhase4MissionStage EvidenceStage = EPhase4MissionStage::Hangar;
            if (Phase4View.Equals(TEXT("Preflight"), ESearchCase::IgnoreCase)) EvidenceStage = EPhase4MissionStage::Preflight;
            else if (Phase4View.Equals(TEXT("FlightHint"), ESearchCase::IgnoreCase)) EvidenceStage = EPhase4MissionStage::Inspection;
            else if (Phase4View.Equals(TEXT("BoundaryRecovery"), ESearchCase::IgnoreCase)) EvidenceStage = EPhase4MissionStage::ReturnToHome;
            else if (Phase4View.Equals(TEXT("Result"), ESearchCase::IgnoreCase)) EvidenceStage = EPhase4MissionStage::Result;
            else if (Phase4View.Equals(TEXT("Replay"), ESearchCase::IgnoreCase)) EvidenceStage = EPhase4MissionStage::Replay;
            Phase4Mission.ConfigureEvidenceState(EvidenceStage);
            bEvidenceStateLocked = true;

            APlayerController* Controller = World->GetFirstPlayerController();
            AFlightPawn* Pawn = Controller != nullptr ? Cast<AFlightPawn>(Controller->GetPawn()) : nullptr;
            if (Pawn != nullptr)
            {
                const bool bFlightView = EvidenceStage == EPhase4MissionStage::Inspection || EvidenceStage == EPhase4MissionStage::ReturnToHome;
                const FTransform EvidenceTransform(
                    FRotator(0.0F, bFlightView ? 22.0F : 28.0F, 0.0F),
                    bFlightView ? FVector(3500.0F, -2100.0F, 850.0F) : FVector(-5100.0F, -4900.0F, 60.0F));
                Pawn->ConfigureReviewView(EvidenceTransform, bFlightView ? EFlightCameraMode::FPV : EFlightCameraMode::Chase);
                if (!bFlightView && Pawn->GetFollowSpringArm() != nullptr) Pawn->GetFollowSpringArm()->TargetArmLength = 115.0F;
            }
        }

        FString ProductUxView;
        if (FParse::Value(FCommandLine::Get(), TEXT("AeroSimProductUxView="), ProductUxView))
        {
            if (ProductUxView.Equals(TEXT("Hangar"), ESearchCase::IgnoreCase))
            {
                Phase4Mission.ConfigureEvidenceState(EPhase4MissionStage::Hangar);
            }
            else if (ProductUxView.Equals(TEXT("Preflight"), ESearchCase::IgnoreCase))
            {
                Phase4Mission.ConfigureEvidenceState(EPhase4MissionStage::Preflight);
            }
            else if (ProductUxView.Equals(TEXT("Flight"), ESearchCase::IgnoreCase))
            {
                Phase4Mission.ConfigureEvidenceState(EPhase4MissionStage::Inspection);
                if (AFlightPawn* Pawn = Cast<AFlightPawn>(World->GetFirstPlayerController() != nullptr ? World->GetFirstPlayerController()->GetPawn() : nullptr))
                {
                    Pawn->ConfigureReviewFlightState(1800.0F, 900.0F);
                }
            }
            else if (ProductUxView.Equals(TEXT("DisplaySettings"), ESearchCase::IgnoreCase))
            {
                DisplaySettings.Open(AppliedSettings);
                if (AFlightPawn* Pawn = Cast<AFlightPawn>(World->GetFirstPlayerController() != nullptr ? World->GetFirstPlayerController()->GetPawn() : nullptr)) Pawn->SetSafePaused(true);
            }
            bEvidenceStateLocked = true;
            if (ProductUxView.Equals(TEXT("Hangar"), ESearchCase::IgnoreCase)) Shell.Page = EProductPage::Hangar;
            else if (ProductUxView.Equals(TEXT("Preflight"), ESearchCase::IgnoreCase)) Shell.Page = EProductPage::Preflight;
            else if (ProductUxView.Equals(TEXT("Flight"), ESearchCase::IgnoreCase)) Shell.Page = EProductPage::Flight;
            else if (ProductUxView.Equals(TEXT("Result"), ESearchCase::IgnoreCase))
            {
                Phase4Mission.ConfigureEvidenceState(EPhase4MissionStage::Result);
                Shell.Page = EProductPage::Result;
            }
            else if (ProductUxView.Equals(TEXT("Replay"), ESearchCase::IgnoreCase))
            {
                Phase4Mission.ConfigureEvidenceState(EPhase4MissionStage::Replay);
                Shell.Page = EProductPage::Replay;
            }
            else if (ProductUxView.Equals(TEXT("Mission"), ESearchCase::IgnoreCase)) Shell.Page = EProductPage::Mission;
            else if (ProductUxView.Equals(TEXT("DisplaySettings"), ESearchCase::IgnoreCase)) Shell.Page = EProductPage::Settings;
            else if (ProductUxView.Equals(TEXT("VisualLab"), ESearchCase::IgnoreCase)) NavigateProductPage(EProductPage::VisualLab);
            else if (ProductUxView.Equals(TEXT("Home"), ESearchCase::IgnoreCase)) Shell.Page = EProductPage::Home;
            else
            {
                if (ProductUxView.Equals(TEXT("Graphics"), ESearchCase::IgnoreCase)) Shell.SettingsCategory = EProductSettingsCategory::Graphics;
                else if (ProductUxView.Equals(TEXT("Audio"), ESearchCase::IgnoreCase)) Shell.SettingsCategory = EProductSettingsCategory::Audio;
                else if (ProductUxView.Equals(TEXT("Controls"), ESearchCase::IgnoreCase)) Shell.SettingsCategory = EProductSettingsCategory::Controls;
                else if (ProductUxView.Equals(TEXT("Camera"), ESearchCase::IgnoreCase)) Shell.SettingsCategory = EProductSettingsCategory::Camera;
                else if (ProductUxView.Equals(TEXT("Accessibility"), ESearchCase::IgnoreCase)) Shell.SettingsCategory = EProductSettingsCategory::Accessibility;
                NavigateProductPage(EProductPage::Settings);
            }
        }

        // Packaged play opens in a ready-to-control flight instead of hiding controls behind four
        // menu confirmations. Explicit product-view captures and -AeroSimStartMenu retain the shell.
        if (!GIsAutomationTesting && !FApp::IsUnattended() && ProductUxView.IsEmpty()
            && !FParse::Param(FCommandLine::Get(), TEXT("AeroSimStartMenu")))
            StartFlightFromControlIntent();

        if (APlayerController* Controller = World->GetFirstPlayerController())
        {
            ProductWidget = CreateWidget<UAeroSimProductWidget>(Controller, UAeroSimProductWidget::StaticClass());
            if (ProductWidget != nullptr)
            {
                ProductWidget->AddToViewport(50);
                ProductWidget->ActivateWidget();
                RefreshProductWidget();
            }
            ConfigureProductInputMode();
        }
        ApplyProductConfiguration(AppliedSettings, false, true);

        FString ScreenshotPath;
        if (FParse::Value(FCommandLine::Get(), TEXT("AeroSimScreenshot="), ScreenshotPath))
        {
            // Wall-clock deadlines continue while a settings/lab page safely pauses the World.
            EvidenceCaptureDeadline = FPlatformTime::Seconds() + 8.0;
            EvidenceExitDeadline = EvidenceCaptureDeadline + 4.0;
        }
#if !UE_BUILD_SHIPPING
        if (FParse::Param(FCommandLine::Get(), TEXT("AeroSimTemporalCapture")))
        {
            bTemporalCapture = true;
            bCoastalOverviewCapture = FParse::Param(FCommandLine::Get(), TEXT("AeroSimCoastalOverview"));
            if (bCoastalOverviewCapture && World->GetFirstPlayerController()->GetHUD() != nullptr)
                World->GetFirstPlayerController()->GetHUD()->bShowHUD = false;
            bEvidenceStateLocked = true;
            Shell.Page = EProductPage::Flight;
            if (APlayerController* Controller = World->GetFirstPlayerController())
            {
                if (AFlightPawn* Pawn = Cast<AFlightPawn>(Controller->GetPawn())) Pawn->SetSafePaused(true);
                // Freeze aircraft/input while the environment clock advances: water animation must be sampled, not a paused material.
                Controller->SetPause(false);
                UE_LOG(LogTemp,Display,TEXT("TEMPORAL_ENVIRONMENT_CLOCK advancing=1 aircraft_paused=1"));
                TemporalCaptureCamera = World->SpawnActor<ACameraActor>();
                if (TemporalCaptureCamera != nullptr)
                {
                    TemporalCaptureCamera->SetActorLocationAndRotation(FVector(-5000, -5000, 350), FRotator(-8, 28, 0));
                    TemporalCaptureCamera->GetCameraComponent()->SetFieldOfView(
                        FVerticalFovTransaction::VerticalToHorizontal(60.0F, 1440.0F / 900.0F));
                    Controller->SetViewTarget(TemporalCaptureCamera);
                }
            }
            TemporalCaptureRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots/Product"),
                TEXT("temporal-") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
            TemporalCaptureStartTime = FPlatformTime::Seconds();
            TemporalCaptureNextTime = TemporalCaptureStartTime + 8.0;
            EvidenceCaptureDeadline = 0.0;
            EvidenceExitDeadline = TemporalCaptureStartTime + 60.0;
            RefreshProductWidget();
            UE_LOG(LogTemp, Display, TEXT("TEMPORAL_CAPTURE_BEGIN map=%s frames=24"), *World->GetPackage()->GetName());
        }
#endif
    }
}

bool AAeroSimGameMode::AdvanceMission()
{
    AFlightPawn* Pawn = GetWorld() != nullptr ? Cast<AFlightPawn>(GetWorld()->GetFirstPlayerController() != nullptr ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr) : nullptr;
    if (Phase4Mission.Stage == EPhase4MissionStage::Hangar)
    {
        Phase4Mission.EnterPreflight();
        return MissionFlow.Transition(EAeroSimMissionState::Preflight);
    }
    if (Phase4Mission.Stage == EPhase4MissionStage::Preflight)
    {
        Phase4Mission.SetPreflightComplete(Pawn != nullptr && Pawn->IsPhysicalInputNeutral());
        if (!Phase4Mission.BeginMission()) return false;
        return MissionFlow.Transition(EAeroSimMissionState::Flight);
    }
    if (Phase4Mission.Stage == EPhase4MissionStage::Result)
    {
        if (!Phase4Mission.OpenReplay()) return false;
        return MissionFlow.Transition(EAeroSimMissionState::Replay);
    }
    return false;
}

void AAeroSimGameMode::ObserveMission(
    const float DeltaSeconds,
    const EFlightPhase FlightPhase,
    const FVector& Position,
    const FVector& Velocity,
    const FRotator& Rotation,
    const bool bCollision,
    const bool bConfirmedHomeSupport,
    const bool bWaterContact)
{
    if (bEvidenceStateLocked)
    {
        return;
    }
    if (bCollision)
    {
        Phase4Mission.ReportCollision(Position);
    }
    else Phase4Mission.ReleaseCollisionContact();
    Phase4Mission.Observe(DeltaSeconds, FlightPhase, Position, Velocity, Rotation, bConfirmedHomeSupport);
    if (bWaterContact) Phase4Mission.ReportWaterContact(Position);
    if (Phase4Mission.Stage == EPhase4MissionStage::Result && MissionFlow.State == EAeroSimMissionState::Flight)
    {
        MissionFlow.Transition(EAeroSimMissionState::Result);
        Shell.Page = EProductPage::Result;
        if (AFlightPawn* Pawn = Cast<AFlightPawn>(GetWorld()->GetFirstPlayerController()->GetPawn())) Pawn->SetSafePaused(true);
        RefreshProductWidget();
        ConfigureProductInputMode();
    }
}

void AAeroSimGameMode::RequestReturnToHome()
{
    Phase4Mission.RequestReturnToHome();
}

void AAeroSimGameMode::SetWeather(const EAeroSimWeather Weather)
{
    EnvironmentState.SetWeather(Weather);
    if (Graybox != nullptr)
    {
        Graybox->ApplyEnvironmentState(EnvironmentState);
    }
}

EAeroSimProductSurface AAeroSimGameMode::GetProductSurface() const
{
    if (DisplaySettings.bOpen) return EAeroSimProductSurface::DisplaySettings;
    if (Phase4Mission.Stage == EPhase4MissionStage::Hangar) return EAeroSimProductSurface::Hangar;
    if (Phase4Mission.Stage == EPhase4MissionStage::Preflight) return EAeroSimProductSurface::Preflight;
    return EAeroSimProductSurface::Flight;
}

void AAeroSimGameMode::ApplyDisplayConfiguration(const FPhase7Settings& Settings, const bool bSaveEngineSettings,
    const bool bCheckForCommandLineOverrides)
{
    if (!Settings.IsValid() || GEngine == nullptr || GEngine->GetGameUserSettings() == nullptr) return;
    UGameUserSettings* UserSettings = GEngine->GetGameUserSettings();
    EWindowMode::Type Mode = EWindowMode::Windowed;
    if (Settings.WindowMode == EPhase7WindowMode::Borderless)
    {
#if PLATFORM_MAC
        Mode = EWindowMode::Windowed;
#else
        Mode = EWindowMode::WindowedFullscreen;
#endif
    }
    else if (Settings.WindowMode == EPhase7WindowMode::Fullscreen)
    {
#if PLATFORM_MAC
        Mode = EWindowMode::Windowed;
#else
        Mode = EWindowMode::Fullscreen;
#endif
    }
    FIntPoint Resolution(Settings.ResolutionWidth, Settings.ResolutionHeight);
    bool bUseDesktopResolution = Mode == EWindowMode::WindowedFullscreen;
#if PLATFORM_MAC
    bUseDesktopResolution |= Settings.WindowMode != EPhase7WindowMode::Windowed;
#endif
    if (bUseDesktopResolution)
    {
        const FIntPoint DesktopResolution = UserSettings->GetDesktopResolution();
        if (DesktopResolution.X > 0 && DesktopResolution.Y > 0) Resolution = DesktopResolution;
    }
    UserSettings->SetScreenResolution(Resolution);
    UserSettings->SetFullscreenMode(Mode);
    UserSettings->ApplyResolutionSettings(bCheckForCommandLineOverrides);
    if (bSaveEngineSettings)
    {
        UserSettings->ConfirmVideoMode();
        UserSettings->SaveSettings();
    }
}

bool AAeroSimGameMode::SaveProductSettings()
{
    const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Config/AeroSim/settings-v2.json"));
    IPlatformFile& Files = FPlatformFileManager::Get().GetPlatformFile();
    Files.CreateDirectoryTree(*FPaths::GetPath(Path));
    FString Error;
    return FPhase7SaveCodec::WriteCrashSafe(Path, FPhase7SaveCodec::Encode(AppliedSettings), Error);
}

void AAeroSimGameMode::LoadProductSettings()
{
    const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Config/AeroSim/settings-v2.json"));
    FString Payload;
    FString Error;
    FPhase7Settings Loaded;
    bool bLoaded = FFileHelper::LoadFileToString(Payload, *Path) && FPhase7SaveCodec::DecodeAndMigrate(Payload, Loaded, Error);
    if (!bLoaded)
    {
        Payload.Reset();
        bLoaded = FFileHelper::LoadFileToString(Payload, *(Path + TEXT(".bak"))) && FPhase7SaveCodec::DecodeAndMigrate(Payload, Loaded, Error);
    }
    AppliedSettings = bLoaded ? Loaded : FPhase7Settings();
    ApplyDisplayConfiguration(AppliedSettings, false, true);
}

void AAeroSimGameMode::ToggleDisplaySettings()
{
    if (DisplaySettings.bOpen)
    {
        CancelDisplaySettings();
        return;
    }
    NavigateProductPage(EProductPage::Settings);
}

void AAeroSimGameMode::NavigateDisplaySettings(const int32 Delta)
{
    if (DisplaySettings.bOpen) DisplaySettings.MoveFocus(Delta);
}

void AAeroSimGameMode::ChangeDisplaySetting(const int32 Delta)
{
    if (DisplaySettings.bOpen) DisplaySettings.ChangeFocusedValue(Delta);
}

void AAeroSimGameMode::ActivateDisplaySetting()
{
    if (!DisplaySettings.bOpen) return;
    switch (DisplaySettings.FocusedRow)
    {
    case 0:
    case 1: DisplaySettings.ChangeFocusedValue(1); break;
    case 2: ExecuteSettingsAction(EProductSettingsAction::Preview); break;
    case 3: ExecuteSettingsAction(EProductSettingsAction::Apply); break;
    case 4: CancelDisplaySettings(); break;
    case 5: ExecuteSettingsAction(EProductSettingsAction::Rollback); break;
    default: break;
    }
}

void AAeroSimGameMode::CancelDisplaySettings()
{
    if (!DisplaySettings.bOpen) return;
    DisplaySettings.Cancel();
    ApplyProductConfiguration(AppliedSettings, false);
    DisplaySettings.bOpen = false;
    Shell.CloseOverlay();
    Shell.Status = Shell.Page == EProductPage::Flight ? TEXT("SAFE PAUSE - RELEASE CONTROLS, THEN ENTER / SPACE TO RESUME") : FString();
    RefreshProductWidget();
    ConfigureProductInputMode();
}

bool AAeroSimGameMode::IsMissionActive() const
{
    return Phase4Mission.Stage >= EPhase4MissionStage::Takeoff && Phase4Mission.Stage <= EPhase4MissionStage::Landing;
}

void AAeroSimGameMode::SetProductInputKind(const EProductInputKind Kind)
{
    if (Shell.InputKind != Kind)
    {
        Shell.InputKind = Kind;
        RefreshProductWidget();
    }
}

void AAeroSimGameMode::RefreshProductWidget()
{
    if (ProductWidget != nullptr) ProductWidget->Refresh();
}

void AAeroSimGameMode::ConfigureProductInputMode()
{
    APlayerController* Controller = GetWorld() != nullptr ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (Controller == nullptr) return;
    const bool bFlight = Shell.Page == EProductPage::Flight;
    Controller->bShowMouseCursor = !bFlight;
    Controller->bEnableClickEvents = !bFlight;
    if (bFlight)
    {
        if (ProductWidget != nullptr && ProductWidget->IsActivated()) ProductWidget->DeactivateWidget();
        Controller->SetInputMode(FInputModeGameOnly());
        if (FSlateApplication::IsInitialized())
            FSlateApplication::Get().SetAllUserFocusToGameViewport(EFocusCause::SetDirectly);
    }
    else
    {
        if (ProductWidget != nullptr && !ProductWidget->IsActivated()) ProductWidget->ActivateWidget();
        FInputModeGameAndUI Mode;
        Mode.SetHideCursorDuringCapture(false);
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        if (ProductWidget != nullptr) Mode.SetWidgetToFocus(ProductWidget->TakeWidget());
        Controller->SetInputMode(Mode);
    }
}

bool AAeroSimGameMode::NavigateProductPage(const EProductPage Page)
{
    if (!FProductShellState::IsValidPage(Page)) return false;
    if (Page == EProductPage::Flight || Page == EProductPage::Result || Page == EProductPage::Replay || Page == EProductPage::Preflight)
    {
        // These pages are reached only via validated mission transitions, never via navigation.
        Shell.Status = TEXT("USE THE MISSION PRIMARY ACTION");
        return false;
    }
    if (IsMissionActive() && !FProductShellState::IsOverlay(Page))
    {
        Shell.Status = TEXT("MISSION ACTIVE - RETURN SAFELY BEFORE CHANGING WORKSPACE"); return false;
    }
    if (Shell.Page == EProductPage::VisualLab && Page != EProductPage::VisualLab) CloseLabWorkspace();
    if (DisplaySettings.bOpen && Page != EProductPage::Settings) CancelDisplaySettings();
    if (!FProductShellState::IsOverlay(Page) && (Phase4Mission.Stage == EPhase4MissionStage::Result || Phase4Mission.Stage == EPhase4MissionStage::Replay))
    {
        RestartProductMission();
    }
    if (!Shell.Open(Page, IsMissionActive())) return false;
    AFlightPawn* Pawn = GetWorld() != nullptr && GetWorld()->GetFirstPlayerController() != nullptr
        ? Cast<AFlightPawn>(GetWorld()->GetFirstPlayerController()->GetPawn()) : nullptr;
    if (Page == EProductPage::Settings)
    {
        if (!DisplaySettings.bOpen) DisplaySettings.Open(AppliedSettings);
        if (Pawn != nullptr) Pawn->SetSafePaused(true);
    }
    if (Page == EProductPage::VisualLab)
    {
        LabOpeningEnvironment = EnvironmentState;
        Lab = FProductLabState();
        if (Pawn != nullptr) Pawn->SetSafePaused(true);
        if (LabCamera == nullptr)
        {
            LabCamera = GetWorld()->SpawnActor<ACameraActor>();
            if (LabCamera != nullptr)
            {
                LabCamera->SetActorLocationAndRotation(FVector(40000.0F, -42000.0F, 16000.0F), FRotator(-17.0F, 145.0F, 0.0F));
            }
        }
        if (LabCamera != nullptr) GetWorld()->GetFirstPlayerController()->SetViewTarget(LabCamera);
    }
    RefreshProductWidget();
    ConfigureProductInputMode();
    return true;
}

void AAeroSimGameMode::ActivatePrimaryAction()
{
    switch (Shell.Page)
    {
    case EProductPage::Home: NavigateProductPage(EProductPage::Hangar); break;
    case EProductPage::Hangar: NavigateProductPage(EProductPage::Mission); break;
    case EProductPage::Mission:
        if (AdvanceMission()) Shell.Page = EProductPage::Preflight;
        break;
    case EProductPage::Preflight:
        if (AdvanceMission())
        {
            Shell.Page = EProductPage::Flight;
            if (AFlightPawn* Pawn = Cast<AFlightPawn>(GetWorld()->GetFirstPlayerController()->GetPawn())) Pawn->SetSafePaused(false);
        }
        else Shell.Status = TEXT("RELEASE ALL CONTROLS TO CENTER INPUT BEFORE STARTING");
        break;
    case EProductPage::Result:
        if (AdvanceMission()) { Shell.Page = EProductPage::Replay; bReplayPlaying = false; ReplayTimeSeconds = 0.0F; }
        break;
    case EProductPage::Replay: SetReplayPlaying(!bReplayPlaying); break;
    case EProductPage::Flight:
        if (AFlightPawn* Pawn = Cast<AFlightPawn>(GetWorld()->GetFirstPlayerController()->GetPawn()))
        {
            if (Pawn->GetFlightModel().Phase == EFlightPhase::Paused) Pawn->SetSafePaused(false);
        }
        break;
    default: break;
    }
    RefreshProductWidget();
    ConfigureProductInputMode();
}

void AAeroSimGameMode::StartFlightFromControlIntent()
{
    if (Shell.Page == EProductPage::Flight) return;
    if (Shell.Page == EProductPage::Result || Shell.Page == EProductPage::Replay
        || Shell.Page == EProductPage::Settings || Shell.Page == EProductPage::VisualLab) return;

    if (Phase4Mission.Stage == EPhase4MissionStage::Hangar)
    {
        Phase4Mission.EnterPreflight();
        MissionFlow.Transition(EAeroSimMissionState::Preflight);
    }
    if (Phase4Mission.Stage == EPhase4MissionStage::Preflight)
    {
        // This path is entered by a deliberate flight-control key. Treat that key as intent rather
        // than a stuck device; subsequent runtime neutral/resume checks remain unchanged.
        Phase4Mission.SetPreflightComplete(true);
        if (!Phase4Mission.BeginMission() || !MissionFlow.Transition(EAeroSimMissionState::Flight)) return;
    }
    Shell.Page = EProductPage::Flight;
    Shell.Status = TEXT("FLIGHT CONTROLS ACTIVE");
    if (AFlightPawn* Pawn = GetWorld() != nullptr && GetWorld()->GetFirstPlayerController() != nullptr
        ? Cast<AFlightPawn>(GetWorld()->GetFirstPlayerController()->GetPawn()) : nullptr)
        Pawn->SetSafePaused(false);
    RefreshProductWidget();
    ConfigureProductInputMode();
}

void AAeroSimGameMode::HandleProductBack()
{
    if (Shell.Page == EProductPage::Settings)
    {
        CancelDisplaySettings();
        return;
    }
    if (Shell.Page == EProductPage::VisualLab)
    {
        CloseLabWorkspace();
        // Explicit resume remains required when returning to flight.
    }
    else if (Shell.Page == EProductPage::Flight) { ToggleDisplaySettings(); return; }
    else if (Shell.Page == EProductPage::Preflight)
    {
        Phase4Mission.Reset(); MissionFlow.Reset(); Shell.Page = EProductPage::Mission;
    }
    else if (Shell.Page == EProductPage::Mission) Shell.Page = EProductPage::Hangar;
    else if (Shell.Page == EProductPage::Hangar) Shell.Page = EProductPage::Home;
    else if (Shell.Page == EProductPage::Replay)
    {
        bReplayPlaying = false;
        if (MissionFlow.Transition(EAeroSimMissionState::Result))
        {
            Phase4Mission.Stage = EPhase4MissionStage::Result;
            Shell.Page = EProductPage::Result;
        }
    }
    RefreshProductWidget();
    ConfigureProductInputMode();
}

void AAeroSimGameMode::CloseLabWorkspace()
{
    if (Shell.Page != EProductPage::VisualLab) return;
    EnvironmentState = LabOpeningEnvironment;
    if (Graybox != nullptr) Graybox->ApplyEnvironmentState(EnvironmentState);
    ApplyProductConfiguration(AppliedSettings, false);
    if (APlayerController* Controller = GetWorld() != nullptr ? GetWorld()->GetFirstPlayerController() : nullptr)
    {
        Controller->SetViewTarget(Controller->GetPawn());
    }
    Lab = FProductLabState();
    Diagnostics = FProductDiagnosticSample();
    DiagnosticFrameTimes.Reset();
    Shell.CloseOverlay();
}

void AAeroSimGameMode::SetSettingsCategory(const EProductSettingsCategory Category)
{
    if (static_cast<uint8>(Category) > static_cast<uint8>(EProductSettingsCategory::Accessibility) || !DisplaySettings.bOpen) return;
    Shell.SettingsCategory = Category;
    RefreshProductWidget();
}

bool AAeroSimGameMode::StageProductSetting(const EProductSettingField Field, const int32 Delta)
{
    if (!DisplaySettings.bOpen || Delta == 0 || DisplaySettings.bPreviewOnDisplay) return false;
    FPhase7Settings Candidate = DisplaySettings.GetVisible();
    const int32 Step = Delta > 0 ? 1 : -1;
    switch (Field)
    {
    case EProductSettingField::Resolution:
    {
        const auto Values = FProductDisplaySettings::SupportedResolutions();
        const int32 Current = Values.IndexOfByKey(FIntPoint(Candidate.ResolutionWidth, Candidate.ResolutionHeight));
        const auto Next = Values[(FMath::Max(Current, 0) + Step + Values.Num()) % Values.Num()];
        Candidate.ResolutionWidth = Next.X; Candidate.ResolutionHeight = Next.Y; break;
    }
    case EProductSettingField::WindowMode:
        Candidate.WindowMode = static_cast<EPhase7WindowMode>((static_cast<int32>(Candidate.WindowMode) + Step + 3) % 3); break;
    case EProductSettingField::Quality:
        // Desktop UI cannot accidentally opt into the tablet candidate profile.
        Candidate.QualityTier = Candidate.QualityTier == EPhase7QualityTier::DesktopHigh ? EPhase7QualityTier::DesktopLow : EPhase7QualityTier::DesktopHigh; break;
    case EProductSettingField::MasterVolume: Candidate.MasterVolume = FMath::Clamp(Candidate.MasterVolume + Step * 0.1F, 0.0F, 1.0F); break;
    case EProductSettingField::VerticalFov: Candidate.VerticalFov = FMath::Clamp(Candidate.VerticalFov + Step * 5.0F, 35.0F, 100.0F); break;
    case EProductSettingField::HudScale: Candidate.HudScale = FMath::Clamp(Candidate.HudScale + Step * 0.1F, 0.8F, 1.4F); break;
    case EProductSettingField::TextScale: Candidate.TextScale = FMath::Clamp(Candidate.TextScale + Step * 0.1F, 1.0F, 2.0F); break;
    case EProductSettingField::ReducedMotion: Candidate.bReducedMotion = !Candidate.bReducedMotion; break;
    default: return false;
    }
    if (!Candidate.IsValid()) return false;
    if (DisplaySettings.Transaction.bPreviewActive) DisplaySettings.Transaction.Preview = Candidate;
    else if (!DisplaySettings.Transaction.Begin(Candidate)) return false;
    DisplaySettings.Status = TEXT("STAGED - PREVIEW OR APPLY; NOT SAVED");
    return true;
}

void AAeroSimGameMode::ApplyProductConfiguration(const FPhase7Settings& Settings, const bool bSaveEngineSettings,
    const bool bCheckForCommandLineOverrides)
{
    if (!Settings.IsValid()) return;
    ApplyDisplayConfiguration(Settings, bSaveEngineSettings, bCheckForCommandLineOverrides);
    if (Graybox != nullptr) Graybox->SetCoastalQuality(Settings.QualityTier == EPhase7QualityTier::DesktopHigh);
    if (GEngine != nullptr && GEngine->GetGameUserSettings() != nullptr)
    {
        UGameUserSettings* UserSettings = GEngine->GetGameUserSettings();
        UserSettings->SetOverallScalabilityLevel(Settings.QualityTier == EPhase7QualityTier::DesktopHigh ? 2 : 0);
        UserSettings->ApplyNonResolutionSettings();
    }
    if (AFlightPawn* Pawn = GetWorld() != nullptr && GetWorld()->GetFirstPlayerController() != nullptr
        ? Cast<AFlightPawn>(GetWorld()->GetFirstPlayerController()->GetPawn()) : nullptr)
    {
        Pawn->ApplyProductVerticalFov(Settings.VerticalFov);
        if (Pawn->GetFollowSpringArm() != nullptr) Pawn->GetFollowSpringArm()->bEnableCameraLag = !Settings.bReducedMotion;
    }
    if (GetWorld() != nullptr && GetWorld()->GetAudioDeviceRaw() != nullptr)
    {
        GetWorld()->GetAudioDeviceRaw()->SetTransientPrimaryVolume(Settings.MasterVolume);
    }
}

void AAeroSimGameMode::ExecuteSettingsAction(const EProductSettingsAction Action)
{
    if (!DisplaySettings.bOpen) return;
    if (Action == EProductSettingsAction::Cancel) { CancelDisplaySettings(); return; }
    const FPhase7Settings Before = AppliedSettings;
    const FProductDisplaySettings Opening = DisplaySettings;
    if (Action == EProductSettingsAction::Preview)
    {
        if (DisplaySettings.BeginPreview()) ApplyProductConfiguration(DisplaySettings.GetVisible(), false);
    }
    else if (Action == EProductSettingsAction::Apply || Action == EProductSettingsAction::Rollback)
    {
        const bool bChanged = Action == EProductSettingsAction::Apply ? DisplaySettings.Apply() : DisplaySettings.Rollback();
        if (!bChanged) return;
        AppliedSettings = DisplaySettings.Transaction.Applied;
        if (!SaveProductSettings())
        {
            AppliedSettings = Before;
            DisplaySettings = Opening;
            DisplaySettings.Cancel();
            ApplyProductConfiguration(Before, false);
            DisplaySettings.Status = TEXT("SAVE FAILED - PREVIOUS APPLIED CONFIGURATION RESTORED");
            return;
        }
        ApplyProductConfiguration(AppliedSettings, true);
        DisplaySettings.Status = Action == EProductSettingsAction::Apply
            ? TEXT("APPLIED + CRASH-SAFE SAVED") : TEXT("ROLLED BACK + CRASH-SAFE SAVED");
    }
}

void AAeroSimGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const double Now = FPlatformTime::Seconds();
    TickTemporalCapture(Now);
    const double WallDelta = FMath::Max(0.0, Now - LastProductTickSeconds);
    LastProductTickSeconds = Now;
    if (EvidenceCaptureDeadline > 0.0 && Now >= EvidenceCaptureDeadline)
    {
        EvidenceCaptureDeadline = 0.0;
        CaptureProductEvidence();
    }
    if (EvidenceExitDeadline > 0.0 && Now >= EvidenceExitDeadline)
    {
        EvidenceExitDeadline = 0.0;
        FPlatformMisc::RequestExit(false);
    }
    if (DisplaySettings.AdvancePreview(WallDelta)) ApplyProductConfiguration(AppliedSettings, false);
    if (WallDelta > 0.0 && FMath::IsFinite(WallDelta))
    {
        DiagnosticFrameTimes.Add(WallDelta * 1000.0);
        if (DiagnosticFrameTimes.Num() > 1800) DiagnosticFrameTimes.RemoveAt(0);
        DiagnosticElapsed += WallDelta;
        if (DiagnosticElapsed >= 0.5)
        {
            DiagnosticElapsed = 0.0;
            TArray<double> Sorted = DiagnosticFrameTimes;
            Sorted.Sort();
            Diagnostics.Samples = Sorted.Num();
            Diagnostics.FrameMs = Sorted[FMath::Clamp(FMath::CeilToInt(Sorted.Num() * 0.95) - 1, 0, Sorted.Num() - 1)];
            Diagnostics.CpuGameMs = FPlatformTime::ToMilliseconds(GGameThreadTime);
            Diagnostics.CpuRenderMs = FPlatformTime::ToMilliseconds(GRenderThreadTime);
            Diagnostics.GpuMs = GDynamicRHI != nullptr ? FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()) : 0.0;
            Diagnostics.ResidentMemoryMiB = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0);
            Diagnostics.DrawCalls = GNumDrawCallsRHI[0];
            FTextureMemoryStats TextureStats;
            if (GDynamicRHI != nullptr) RHIGetTextureMemoryStats(TextureStats);
            Diagnostics.TextureStreamingMiB = static_cast<double>(TextureStats.StreamingMemorySize) / (1024.0 * 1024.0);
            Diagnostics.TexturePoolMiB = static_cast<double>(TextureStats.TexturePoolSize) / (1024.0 * 1024.0);
            const FString Gpu = Diagnostics.GpuMs > 0.0 ? FString::Printf(TEXT("%.2f ms"), Diagnostics.GpuMs) : TEXT("NOT_MEASURED");
            Diagnostics.Summary = FString::Printf(TEXT("DIAGNOSTIC ONLY / NOT_RELEASE_EVIDENCE\nFrame P95 %.2f ms (%d samples)\nCPU Game %.2f ms / Render %.2f ms\nGPU %s\nResident %.0f MiB / Draw %d\nStreaming %.0f MiB / Pool %.0f MiB\nNo 120s warmup, 30min protocol or hardware gate"),
                Diagnostics.FrameMs, Diagnostics.Samples, Diagnostics.CpuGameMs, Diagnostics.CpuRenderMs, *Gpu,
                Diagnostics.ResidentMemoryMiB, Diagnostics.DrawCalls, Diagnostics.TextureStreamingMiB, Diagnostics.TexturePoolMiB);
        }
    }
    if (Shell.Page == EProductPage::VisualLab && LabCamera != nullptr)
    {
        int32 Width = 1, Height = 1;
        if (APlayerController* Controller = GetWorld()->GetFirstPlayerController()) Controller->GetViewportSize(Width, Height);
        LabCamera->GetCameraComponent()->SetFieldOfView(FVerticalFovTransaction::VerticalToHorizontal(
            AppliedSettings.VerticalFov, Height > 0 ? static_cast<float>(Width) / Height : 1.0F));
    }
    if (Shell.Page == EProductPage::Replay && bReplayPlaying)
    {
        if (AFlightPawn* Pawn = Cast<AFlightPawn>(GetWorld()->GetFirstPlayerController()->GetPawn()))
        {
            ReplayTimeSeconds += FMath::Min(static_cast<float>(WallDelta), 0.25F);
            const float Duration = Pawn->GetResultMetrics().DurationSeconds;
            ReplayTimeSeconds = FMath::Min(ReplayTimeSeconds, Duration);
            Pawn->ApplyReplaySample(Pawn->GetFlightTrace().Evaluate(ReplayTimeSeconds));
            if (ReplayTimeSeconds >= Duration) bReplayPlaying = false;
        }
    }
}

void AAeroSimGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
    if (ProductWidget != nullptr) { ProductWidget->RemoveFromParent(); ProductWidget = nullptr; }
    Super::EndPlay(Reason);
}

void AAeroSimGameMode::SetLabWeather(const EAeroSimWeather Weather)
{
    if (Shell.Page != EProductPage::VisualLab || static_cast<uint8>(Weather) > static_cast<uint8>(EAeroSimWeather::Rain)) return;
    SetWeather(Weather);
    DiagnosticFrameTimes.Reset();
    Diagnostics = FProductDiagnosticSample();
    Lab.bReferenceLocked = false;
    Lab.bShowingReference = false;
    Lab.Status = TEXT("DIRTY / NOT_CAPTURED - WEATHER CHANGED; LOCK REFERENCE AGAIN");
}

void AAeroSimGameMode::LockLabReference()
{
    if (Shell.Page != EProductPage::VisualLab) return;
    Lab.bReferenceLocked = true;
    Lab.bShowingReference = true;
    DiagnosticFrameTimes.Reset();
    Diagnostics = FProductDiagnosticSample();
    Lab.Seed = EnvironmentState.Seed;
    Lab.VerticalFov = AppliedSettings.VerticalFov;
    Lab.ReferenceQuality = AppliedSettings.QualityTier;
    Lab.ComparisonQuality = Lab.ReferenceQuality == EPhase7QualityTier::DesktopHigh ? EPhase7QualityTier::DesktopLow : EPhase7QualityTier::DesktopHigh;
    ApplyProductConfiguration(AppliedSettings, false);
    Lab.Status = TEXT("REFERENCE A LOCKED / NOT_CAPTURED - ONLY QUALITY MAY DIFFER");
}

void AAeroSimGameMode::ToggleLabComparison()
{
    if (Shell.Page != EProductPage::VisualLab || !Lab.bReferenceLocked) return;
    Lab.bShowingReference = !Lab.bShowingReference;
    DiagnosticFrameTimes.Reset();
    Diagnostics = FProductDiagnosticSample();
    FPhase7Settings Visible = AppliedSettings;
    Visible.QualityTier = Lab.bShowingReference ? Lab.ReferenceQuality : Lab.ComparisonQuality;
    ApplyProductConfiguration(Visible, false);
    Lab.Status = Lab.bShowingReference ? TEXT("REFERENCE A / NOT_CAPTURED") : TEXT("COMPARISON B / NOT_CAPTURED - QUALITY IS THE ONLY DIFFERENCE");
}

void AAeroSimGameMode::RestartProductMission()
{
    APlayerController* Controller=GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    AFlightPawn* Pawn=Controller ? Cast<AFlightPawn>(Controller->GetPawn()) : nullptr;
    // Explicit reset is the only recovery from a latched water incident. Ordinary
    // active missions retain their guard; do not turn a retry into an auto-resume.
    const bool bWaterRetry=Shell.Page==EProductPage::Flight && Pawn && Pawn->HasWaterIncident()
        && Pawn->GetFlightModel().Phase==EFlightPhase::Paused
        && Phase4Mission.Feedback.Incident==EPhase4Incident::UnsafeLanding;
    if (IsMissionActive() && !bWaterRetry) return;
    bReplayPlaying = false;
    Phase4Mission.Reset(); MissionFlow.Reset();
    if (Pawn) Pawn->ResetFlight();
    Shell.Page = EProductPage::Hangar;
    Shell.Status.Reset();
    RefreshProductWidget(); ConfigureProductInputMode();
}

void AAeroSimGameMode::SetReplayPlaying(const bool bPlaying)
{
    if (Shell.Page != EProductPage::Replay) return;
    if (AFlightPawn* Pawn = Cast<AFlightPawn>(GetWorld()->GetFirstPlayerController()->GetPawn()))
    {
        if (Pawn->GetFlightTrace().Samples.Num() < 2)
        {
            Shell.Status = TEXT("NO RECORDED FLIGHT - PLAYBACK UNAVAILABLE"); return;
        }
        bReplayPlaying = bPlaying;
        // Unpause the World only for UI playback; Pawn simulation stays neutral/paused.
        GetWorld()->GetFirstPlayerController()->SetPause(false);
    }
}

void AAeroSimGameMode::SeekReplay(const float NormalizedTime)
{
    if (Shell.Page != EProductPage::Replay || !FMath::IsFinite(NormalizedTime)) return;
    if (AFlightPawn* Pawn = Cast<AFlightPawn>(GetWorld()->GetFirstPlayerController()->GetPawn()))
    {
        ReplayTimeSeconds = FMath::Clamp(NormalizedTime, 0.0F, 1.0F) * Pawn->GetResultMetrics().DurationSeconds;
        Pawn->ApplyReplaySample(Pawn->GetFlightTrace().Evaluate(ReplayTimeSeconds));
    }
}

float AAeroSimGameMode::GetReplayNormalizedTime() const
{
    const AFlightPawn* Pawn = GetWorld() != nullptr && GetWorld()->GetFirstPlayerController() != nullptr
        ? Cast<AFlightPawn>(GetWorld()->GetFirstPlayerController()->GetPawn()) : nullptr;
    const float Duration = Pawn != nullptr ? Pawn->GetResultMetrics().DurationSeconds : 0.0F;
    return Duration > 0.0F ? ReplayTimeSeconds / Duration : 0.0F;
}

bool AAeroSimGameMode::ResolveReviewView(const FString& Name, const int32 Step, FVector& Location, FRotator& Rotation)
{
    if (Name == TEXT("overview")) { Location = FVector(40000.0F - Step * 300.0F, -42000.0F + Step * 600.0F, 16000.0F); Rotation = FRotator(-17, 145, 0); }
    else if (Name == TEXT("ground")) { Location = FVector(-5000.0F + Step * 200.0F, -5000.0F + Step * 90.0F, 350.0F); Rotation = FRotator(-8, 28, 0); }
    else if (Name == TEXT("south")) { Location = FVector(35000 + Step*200, -245000 + Step*150, 3000); Rotation = FRotator(-3,147,0); }
    else if (Name == TEXT("north")) { Location = FVector(35000 + Step*200, 245000 - Step*150, 3000); Rotation = FRotator(-3,-147,0); }
    else if (Name == TEXT("forest")) { Location = FVector(-16000 + Step*80, -6500 - Step*40, 420); Rotation = FRotator(4,-120,0); }
    else if (Name == TEXT("launch-low")) { Location=FVector(410+Step*4,-290+Step*3,45);Rotation=FRotator(-4,137,0); }
    else if (Name == TEXT("launch-operator")) { Location=FVector(-470+Step*4,-390+Step*3,165);Rotation=FRotator(-13,42,0); }
    else if (Name == TEXT("launch-return")) { Location=FVector(850+Step*8,650-Step*4,500);Rotation=FRotator(-25,-145,0); }
    else if (Name == TEXT("bridge")) { Location = FVector(-12300 + Step*25, 6100 + Step*20, 260); Rotation = FRotator(-3,36,0); }
    else if (Name == TEXT("mountain")) { Location = FVector(-40000 - Step*120, -35000 + Step*80, 12000); Rotation = FRotator(3,175,0); }
    else if (Name == TEXT("shore")) { const float Y=-19000+Step*35;Location=FVector(APacificCoastGraybox::GetCoastalShoreX(Y)+1900,Y,APacificCoastGraybox::SeaLevelCm+280);Rotation=FRotator(7,165,0); }
    else if (Name == TEXT("glint")) { Location = FVector(26000 + Step*60, -8000 - Step*40, APacificCoastGraybox::SeaLevelCm + 600); Rotation = FRotator(-4,-35,0); }
    else if (Name == TEXT("corridor")) { Location = FVector(-15609 - Step*18, -6895 - Step*36, 250); Rotation = FRotator(-3,-117.3F,0); }
    else if (Name == TEXT("cliff")) { Location = FVector(33000 + Step*120, -25000 + Step*120, -1500); Rotation = FRotator(-3,160,0); }
    else if (Name == TEXT("wildlife-whale")) { Location = FVector(52000 + Step*10, -65000 + Step*15, APacificCoastGraybox::SeaLevelCm + 520); Rotation = FRotator(2, 141, 0); }
    else if (Name == TEXT("wildlife-predators")) { Location = FVector(-34400 + Step*8, 37100 + Step*7, APacificCoastGraybox::GetCoastalLandHeight(-34400,37100) + 210); Rotation = FRotator(-5, 132, 0); }
    else if (Name == TEXT("wildlife-herd")) { Location = FVector(-20500 + Step*8, 30200 + Step*8, APacificCoastGraybox::GetCoastalLandHeight(-20500,30200) + 220); Rotation = FRotator(-6, 135, 0); }
    else return false;
    return true;
}

void AAeroSimGameMode::TickTemporalCapture(const double Now)
{
    if (!bTemporalCapture || Now < TemporalCaptureNextTime || FScreenshotRequest::IsScreenshotRequested()) return;
    if (TemporalCaptureIndex >= 24)
    {
        const bool bComplete = FPlatformFileManager::Get().GetPlatformFile().FileExists(*(TemporalCaptureOutput + TEXT(".json")));
        UE_LOG(LogTemp, Display, TEXT("TEMPORAL_CAPTURE_END frames=%d complete=%d"), TemporalCaptureIndex, bComplete ? 1 : 0);
        TemporalCaptureNextTime = TNumericLimits<double>::Max();
        EvidenceExitDeadline = Now + 2.0;
        return;
    }
    APlayerController* Controller = GetWorld() != nullptr ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (TemporalCaptureCamera == nullptr || Controller == nullptr || Controller->PlayerCameraManager == nullptr) return;
    // Eight stationary frames, eight moving samples, then eight stationary frames.
    const int32 Step = FMath::Clamp(TemporalCaptureIndex - 7, 0, 8);
    FVector Location;
    FRotator Rotation;
    ResolveReviewView(bCoastalOverviewCapture ? TEXT("overview") : TEXT("ground"), Step, Location, Rotation);
    FString CoastalView;
    if (bCoastalOverviewCapture && FParse::Value(FCommandLine::Get(), TEXT("AeroSimCoastalView="), CoastalView) && CoastalView != TEXT("ground"))
    {
        ResolveReviewView(CoastalView, Step, Location, Rotation);
    }
    TemporalCaptureCamera->SetActorLocationAndRotation(Location, Rotation);
    int32 Width = 0, Height = 0;
    Controller->GetViewportSize(Width, Height);
    TemporalCaptureCamera->GetCameraComponent()->SetFieldOfView(FVerticalFovTransaction::VerticalToHorizontal(
        AppliedSettings.VerticalFov, Height > 0 ? static_cast<float>(Width) / Height : 1.0F));
    Controller->PlayerCameraManager->UpdateCamera(0.0F);
    TemporalCaptureOutput = FPaths::Combine(TemporalCaptureRoot, FString::Printf(TEXT("frame-%03d.png"), TemporalCaptureIndex));
    CaptureProductEvidence();
    ++TemporalCaptureIndex;
    TemporalCaptureNextTime = Now + 0.3;
}

void AAeroSimGameMode::CaptureProductEvidence()
{
    if (FScreenshotRequest::IsScreenshotRequested()) { Shell.Status = TEXT("CAPTURE BUSY"); return; }
    FString Output;
    FParse::Value(FCommandLine::Get(), TEXT("AeroSimScreenshot="), Output);
    if (bTemporalCapture) Output = TemporalCaptureOutput;
    const FString EvidenceRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots/Product")));
    if (Output.IsEmpty()) Output = FPaths::Combine(EvidenceRoot, FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S")) + TEXT("-") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8) + TEXT(".png"));
    Output = FPaths::ConvertRelativePathToFull(Output);
    // The diagnostic CLI can select only a file within the dedicated capture root, not arbitrary disk writes.
    if (!FPaths::IsUnderDirectory(Output, EvidenceRoot) || !FPaths::GetExtension(Output).Equals(TEXT("png"), ESearchCase::IgnoreCase))
    {
        Shell.Status = TEXT("CAPTURE FAILED / PATH_NOT_ALLOWED"); return;
    }
    LastProductEvidenceOutput = Output;
    FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(Output));
    TSharedRef<FJsonObject> Manifest = MakeShared<FJsonObject>();
    Manifest->SetStringField(TEXT("schema"), TEXT("aerosim.product-evidence/v1"));
    Manifest->SetStringField(TEXT("build_id"), FAeroSimRuntimeContract::BuildId);
    Manifest->SetStringField(TEXT("source_revision"), TEXT(AEROSIM_SOURCE_REVISION));
    Manifest->SetStringField(TEXT("source_fingerprint"), TEXT(AEROSIM_SOURCE_FINGERPRINT));
    Manifest->SetBoolField(TEXT("source_dirty"), AEROSIM_SOURCE_DIRTY != 0);
    Manifest->SetStringField(TEXT("runtime_contract"), FAeroSimRuntimeContract::Contract);
    Manifest->SetStringField(TEXT("ue_version"), FEngineVersion::Current().ToString());
    Manifest->SetStringField(TEXT("platform"), FPlatformProperties::PlatformName());
    Manifest->SetStringField(TEXT("hardware"), FPlatformMisc::GetCPUBrand());
    Manifest->SetStringField(TEXT("gpu"), FPlatformMisc::GetPrimaryGPUBrand());
    Manifest->SetStringField(TEXT("rhi"), GDynamicRHI != nullptr ? GDynamicRHI->GetName() : TEXT("UNKNOWN"));
    Manifest->SetStringField(TEXT("scene_id"), TEXT("pacific-coast-inspection"));
#if UE_BUILD_SHIPPING
    Manifest->SetStringField(TEXT("configuration"), TEXT("Shipping diagnostic / NOT_RELEASE_EVIDENCE"));
#elif UE_BUILD_DEVELOPMENT
    Manifest->SetStringField(TEXT("configuration"), TEXT("Development diagnostic / NOT_RELEASE_EVIDENCE"));
#else
    Manifest->SetStringField(TEXT("configuration"), TEXT("UNKNOWN / NOT_RELEASE_EVIDENCE"));
#endif
    Manifest->SetNumberField(TEXT("product_page"), static_cast<uint8>(Shell.Page));
    Manifest->SetNumberField(TEXT("weather"), static_cast<uint8>(EnvironmentState.Weather));
    Manifest->SetNumberField(TEXT("time_minutes"), EnvironmentState.TimeMinutes);
    Manifest->SetNumberField(TEXT("seed"), EnvironmentState.Seed);
    const FPhase7Settings Visible = DisplaySettings.bPreviewOnDisplay ? DisplaySettings.GetVisible() : AppliedSettings;
    Manifest->SetNumberField(TEXT("quality_tier"), static_cast<uint8>(Shell.Page == EProductPage::VisualLab && Lab.bReferenceLocked
        ? (Lab.bShowingReference ? Lab.ReferenceQuality : Lab.ComparisonQuality) : Visible.QualityTier));
    Manifest->SetNumberField(TEXT("vertical_fov"), Visible.VerticalFov);
    Manifest->SetStringField(TEXT("camera_contract"), TEXT("Context+Family+Preset / vertical FOV truth"));
    if (APlayerController* Controller = GetWorld() != nullptr ? GetWorld()->GetFirstPlayerController() : nullptr)
    {
        int32 Width = 0, Height = 0;
        Controller->GetViewportSize(Width, Height);
        Manifest->SetNumberField(TEXT("viewport_width"), Width);
        Manifest->SetNumberField(TEXT("viewport_height"), Height);
        Manifest->SetNumberField(TEXT("horizontal_fov"), FVerticalFovTransaction::VerticalToHorizontal(Visible.VerticalFov,
            Height > 0 ? static_cast<float>(Width) / Height : 1.0F));
        if (AFlightPawn* Pawn = Cast<AFlightPawn>(Controller->GetPawn()))
        {
            Manifest->SetNumberField(TEXT("camera_mode"), static_cast<uint8>(Pawn->GetCameraMode()));
            Manifest->SetStringField(TEXT("aircraft_runtime_id"), TEXT("aerosim-generic-golden"));
        }
        FVector ViewLocation; FRotator ViewRotation;
        Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
        Manifest->SetStringField(TEXT("camera_position_cm"), ViewLocation.ToString());
        Manifest->SetStringField(TEXT("camera_rotation_degrees"), ViewRotation.ToString());
        if (Controller->PlayerCameraManager != nullptr)
            AddFrustumGeometryBudget(GetWorld(), Controller->PlayerCameraManager->GetCameraCacheView(), Width, Height, Manifest);
    }
    if (!EvidenceReviewView.IsEmpty())
    {
        Manifest->SetStringField(TEXT("capture_protocol"), TEXT("bridge-review-view/v1"));
        Manifest->SetStringField(TEXT("review_view"), EvidenceReviewView);
        Manifest->SetStringField(TEXT("view_mode"), EvidenceViewMode);
        Manifest->SetNumberField(TEXT("review_sequence_index"), EvidenceSequenceIndex);
        Manifest->SetNumberField(TEXT("review_sequence_frames"), EvidenceSequenceFrames);
        Manifest->SetNumberField(TEXT("review_sequence_elapsed_seconds"), FPlatformTime::Seconds() - EvidenceSequenceStart);
        Manifest->SetBoolField(TEXT("world_paused"), GetWorld() != nullptr && GetWorld()->IsPaused());
    }
    Manifest->SetStringField(TEXT("visual_verdict"), TEXT("NOT_RUN - HUMAN G0-G4 REQUIRED"));
    Manifest->SetStringField(TEXT("asset_status"), TEXT("GENERIC PROXY / BLOCKED_ASSET"));
    Manifest->SetStringField(TEXT("user_confirmation"), TEXT("BLOCKED_USER_CONFIRMATION"));
    Manifest->SetStringField(TEXT("performance_claim"), TEXT("NOT_MEASURED"));
    Manifest->SetNumberField(TEXT("diagnostic_frame_p95_ms"), Diagnostics.FrameMs);
    Manifest->SetNumberField(TEXT("diagnostic_samples"), Diagnostics.Samples);
    Manifest->SetNumberField(TEXT("diagnostic_cpu_game_ms"), Diagnostics.CpuGameMs);
    Manifest->SetNumberField(TEXT("diagnostic_cpu_render_ms"), Diagnostics.CpuRenderMs);
    Manifest->SetNumberField(TEXT("diagnostic_gpu_ms"), Diagnostics.GpuMs);
    Manifest->SetNumberField(TEXT("diagnostic_resident_mib"), Diagnostics.ResidentMemoryMiB);
    Manifest->SetNumberField(TEXT("diagnostic_draw_calls"), Diagnostics.DrawCalls);
    // UE 5.8.3 MetalRHI never calls FlushProfilerStats(), so GNumDrawCallsRHI stays zero on Metal.
    const bool bMetalRhi = GDynamicRHI != nullptr && FString(GDynamicRHI->GetName()).Contains(TEXT("Metal"));
    Manifest->SetStringField(TEXT("diagnostic_draw_calls_source"), Diagnostics.DrawCalls > 0 ? TEXT("GNumDrawCallsRHI")
        : bMetalRhi ? TEXT("UNAVAILABLE: UE 5.8.3 MetalRHI does not report draw stats") : TEXT("ZERO_OR_NOT_YET_SAMPLED"));
    Manifest->SetNumberField(TEXT("diagnostic_streaming_mib"), Diagnostics.TextureStreamingMiB);
    Manifest->SetStringField(TEXT("diagnostic_scope"), TEXT("rolling window + latest engine counters / not qualification"));
    Manifest->SetStringField(TEXT("image_file"), FPaths::GetCleanFilename(Output));
    Manifest->SetStringField(TEXT("map_package"), GetWorld()->GetPackage()->GetName());
    if (bTemporalCapture)
    {
        Manifest->SetStringField(TEXT("capture_protocol"), bCoastalOverviewCapture ? TEXT("coastal-cliff-overview/v1") : TEXT("ground-hold-move-hold/v1"));
        Manifest->SetNumberField(TEXT("sequence_index"), TemporalCaptureIndex);
        Manifest->SetNumberField(TEXT("capture_elapsed_seconds"), FPlatformTime::Seconds() - TemporalCaptureStartTime);
    }
    const auto Handle = MakeShared<FDelegateHandle>();
    const auto PixelHandle = MakeShared<FDelegateHandle>();
    const auto PixelSaved = MakeShared<bool>(false);
    *PixelHandle = UGameViewportClient::OnScreenshotCaptured().AddWeakLambda(this,
        [Output, Manifest, PixelSaved](const int32 Width, const int32 Height, const TArray<FColor>& Pixels)
        {
            UE_LOG(LogTemp, Display, TEXT("PRODUCT_EVIDENCE_PIXELS width=%d height=%d count=%d"), Width, Height, Pixels.Num());
            if (Width <= 0 || Height <= 0 || Width > 16384 || Height > 16384 || Pixels.Num() < Width * Height) return;
            // An occluded or off-screen fullscreen window on macOS reads back pure black; never commit that as evidence.
            bool bBlank = true;
            for (int32 Index = 0; Index < Width * Height && bBlank; Index += 997)
            {
                const FColor& Pixel = Pixels[Index];
                bBlank = Pixel.R < 3 && Pixel.G < 3 && Pixel.B < 3;
            }
            if (bBlank) { UE_LOG(LogTemp, Warning, TEXT("PRODUCT_EVIDENCE_BLANK width=%d height=%d"), Width, Height); return; }
            TArray64<uint8> Png;
            FImageUtils::PNGCompressImageArray(Width, Height, TArrayView64<const FColor>(Pixels.GetData(), Width * Height), Png);
            if (Png.IsEmpty() || Png.Num() > 64 * 1024 * 1024) return;
            IPlatformFile& Files = FPlatformFileManager::Get().GetPlatformFile();
            TUniquePtr<IFileHandle> ImageFile(Files.OpenWrite(*Output));
            *PixelSaved = ImageFile && ImageFile->Write(Png.GetData(), Png.Num()) && ImageFile->Flush(true);
            UE_LOG(LogTemp, Display, TEXT("PRODUCT_EVIDENCE_WRITE saved=%d bytes=%lld"), *PixelSaved ? 1 : 0, Png.Num());
            Manifest->SetNumberField(TEXT("viewport_width"), Width);
            Manifest->SetNumberField(TEXT("viewport_height"), Height);
        });
    *Handle = FScreenshotRequest::OnScreenshotRequestProcessed().AddWeakLambda(this, [this, Output, Manifest, Handle, PixelHandle, PixelSaved]()
    {
        // Removing a delegate can destroy its captured values immediately. Keep local owners first.
        const FString CaptureOutput = Output;
        const TSharedRef<FJsonObject> CaptureManifest = Manifest;
        AAeroSimGameMode* const Owner = this;
        const FDelegateHandle CompletedHandle = *Handle;
        const FDelegateHandle CompletedPixelHandle = *PixelHandle;
        const bool bPixelCaptureSaved = *PixelSaved;
        UGameViewportClient::OnScreenshotCaptured().Remove(CompletedPixelHandle);
        FScreenshotRequest::OnScreenshotRequestProcessed().Remove(CompletedHandle);
        TArray<uint8> ImageBytes;
        FString Payload;
        if (!bPixelCaptureSaved || !FFileHelper::LoadFileToArray(ImageBytes, *CaptureOutput))
        {
            UE_LOG(LogTemp, Warning, TEXT("PRODUCT_EVIDENCE_FAIL pixels_saved=%d"), bPixelCaptureSaved ? 1 : 0);
            Owner->Shell.Status = TEXT("CAPTURE FAILED / NOT_CAPTURED"); return;
        }
        CaptureManifest->SetStringField(TEXT("image_sha256"), ProductEvidenceSha256(ImageBytes));
        FJsonSerializer::Serialize(CaptureManifest, TJsonWriterFactory<>::Create(&Payload));
        TSharedRef<FJsonObject> Envelope = MakeShared<FJsonObject>();
        Envelope->SetStringField(TEXT("manifest_sha256"), ProductEvidenceSha256(Payload));
        Envelope->SetStringField(TEXT("canonical_manifest"), Payload);
        FString Saved;
        FJsonSerializer::Serialize(Envelope, TJsonWriterFactory<>::Create(&Saved));
        FString WriteError;
        if (FPhase7SaveCodec::WriteCrashSafe(CaptureOutput + TEXT(".json"), Saved, WriteError))
        {
            Owner->Shell.Status = TEXT("CAPTURED + SHA256 / HUMAN VISUAL REVIEW NOT_RUN");
            if (Owner->Shell.Page == EProductPage::VisualLab) Owner->Lab.Status = Owner->Shell.Status;
        }
        else Owner->Shell.Status = TEXT("CAPTURE IMAGE ONLY / INCOMPLETE EVIDENCE");
    });
    // Temporal and review-view captures read the viewport directly, without asking Slate to capture a native window.
    FScreenshotRequest::RequestScreenshot(Output, !bTemporalCapture && EvidenceReviewView.IsEmpty(), false);
}
