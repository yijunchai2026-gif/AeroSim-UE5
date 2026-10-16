#include "Phase3Aircraft.h"

namespace
{
void SetReason(FString* OutReason, const TCHAR* Reason)
{
    if (OutReason != nullptr)
    {
        *OutReason = Reason;
    }
}
}

bool FAircraftDefinition::IsStructurallyValid(FString* OutReason) const
{
    if (RuntimeId.IsNone() || CatalogId.IsNone() || DisplayName.IsEmpty() || AssetVersion.IsEmpty())
    {
        SetReason(OutReason, TEXT("Identity fields are required."));
        return false;
    }
    if (SourceUri.IsEmpty() || Authorization.IsEmpty() || Disclosure.IsEmpty())
    {
        SetReason(OutReason, TEXT("Source, authorization and disclosure are required."));
        return false;
    }
    if (DimensionsCm.X <= 0.0F || DimensionsCm.Y <= 0.0F || DimensionsCm.Z <= 0.0F
        || GroundSupportZCm >= CenterOfMassCm.Z)
    {
        SetReason(OutReason, TEXT("Dimensions, center of mass or support plane are invalid."));
        return false;
    }
    if (Variants.IsEmpty() || Lods.Num() < 2 || MaterialSlots.Num() < 4)
    {
        SetReason(OutReason, TEXT("Variant, LOD and material-slot contracts are incomplete."));
        return false;
    }
    for (const FAircraftLodContract& Lod : Lods)
    {
        if (Lod.Name.IsNone() || Lod.ScreenSize <= 0.0F || Lod.TriangleBudget <= 0)
        {
            SetReason(OutReason, TEXT("LOD entry is invalid."));
            return false;
        }
    }
    return true;
}

bool FAircraftDefinition::CanEnterFormalTask(FString* OutReason) const
{
    if (!IsStructurallyValid(OutReason))
    {
        return false;
    }
    if (AssetStatus != EAircraftAssetStatus::UserConfirmed)
    {
        SetReason(OutReason, TEXT("Formal tasks require USER_CONFIRMED status."));
        return false;
    }
    if (VisualSource != EAircraftVisualSource::ReplaceableStaticMeshPackage
        || ReplacementAsset.IsNull() || ContentSha256.Len() != 64)
    {
        SetReason(OutReason, TEXT("Formal tasks require a hashed replacement asset package."));
        return false;
    }
    if (!BlockingQualityGates.IsEmpty())
    {
        SetReason(OutReason, TEXT("Blocking quality gates remain."));
        return false;
    }
    return true;
}

FAircraftDefinition FAircraftDefinition::MakeGenericGolden()
{
    FAircraftDefinition Definition;
    Definition.RuntimeId = TEXT("generic-golden-camera-quad");
    Definition.CatalogId = TEXT("aerosim.generic.consumer-quad");
    Definition.DisplayName = TEXT("AeroSim Generic Golden Aircraft");
    Definition.AssetVersion = TEXT("programmatic-1.0.0");
    Definition.SourceUri = TEXT("engine://BasicShapes + AeroSim/Phase3Aircraft.cpp");
    Definition.Authorization = TEXT("AeroSim original programmatic proxy; no third-party model or trademark texture.");
    Definition.Disclosure = TEXT("GENERIC PROXY / BLOCKED_ASSET: not a formal DJI or licensed production aircraft.");
    Definition.AssetStatus = EAircraftAssetStatus::BlockedAsset;
    Definition.VisualSource = EAircraftVisualSource::ProgrammaticEngineShapes;
    Definition.DimensionsCm = FVector(38.0F, 32.0F, 12.0F);
    Definition.CenterOfMassCm = FVector(-1.0F, 0.0F, 1.5F);
    Definition.GroundSupportZCm = -6.0F;

    FAircraftVariant Variant;
    Variant.VariantId = TEXT("graphite-silver");
    Variant.DisplayName = TEXT("Graphite Silver / Generic");
    Variant.ShellColor = FLinearColor(0.34F, 0.39F, 0.43F);
    Definition.Variants.Add(Variant);

    FAircraftLodContract Lod0;
    Lod0.Name = TEXT("LOD0_PROGRAMMATIC_DETAIL");
    Lod0.ScreenSize = 0.35F;
    Lod0.TriangleBudget = 24000;
    Definition.Lods.Add(Lod0);
    FAircraftLodContract Lod1;
    Lod1.Name = TEXT("LOD1_PROXY");
    Lod1.ScreenSize = 0.08F;
    Lod1.TriangleBudget = 6000;
    Lod1.bProxy = true;
    Definition.Lods.Add(Lod1);

    const TPair<FName, const TCHAR*> Slots[] = {
        { TEXT("Shell"), TEXT("matte structural polymer") },
        { TEXT("Metal"), TEXT("motor and gimbal metal") },
        { TEXT("Lens"), TEXT("camera optical glass") },
        { TEXT("Propeller"), TEXT("rotor blades") },
        { TEXT("NavigationEmission"), TEXT("front/rear navigation lights") }
    };
    for (const TPair<FName, const TCHAR*>& Slot : Slots)
    {
        FAircraftMaterialSlot Entry;
        Entry.SlotId = Slot.Key;
        Entry.Purpose = Slot.Value;
        Definition.MaterialSlots.Add(Entry);
    }
    Definition.PassedQualityGates = { TEXT("GENERIC_IDENTITY"), TEXT("SCALE"), TEXT("SUPPORT"), TEXT("MATERIAL_SLOTS"), TEXT("LOD_PROXY") };
    Definition.BlockingQualityGates = { TEXT("FORMAL_SOURCE_MODEL"), TEXT("PRODUCTION_GEOMETRY"), TEXT("USER_CONFIRMATION") };
    return Definition;
}

void FGimbalController::SetTarget(const float Pitch, const float Yaw)
{
    TargetPitchDegrees = FMath::Clamp(Pitch, MinimumPitchDegrees, MaximumPitchDegrees);
    TargetYawDegrees = FMath::Clamp(Yaw, -MaximumYawDegrees, MaximumYawDegrees);
}

void FGimbalController::AddTargetInput(const float PitchDelta, const float YawDelta)
{
    SetTarget(TargetPitchDegrees + PitchDelta, TargetYawDegrees + YawDelta);
}

void FGimbalController::Advance(const float DeltaSeconds)
{
    if (DeltaSeconds <= 0.0F)
    {
        return;
    }
    PitchDegrees = FMath::FInterpTo(PitchDegrees, TargetPitchDegrees, DeltaSeconds, SmoothSpeed);
    YawDegrees = FMath::FInterpTo(YawDegrees, TargetYawDegrees, DeltaSeconds, SmoothSpeed);
    PitchDegrees = FMath::Clamp(PitchDegrees, MinimumPitchDegrees, MaximumPitchDegrees);
    YawDegrees = FMath::Clamp(YawDegrees, -MaximumYawDegrees, MaximumYawDegrees);
}

FRotator FGimbalController::GetStabilizedRelativeRotation() const
{
    return FRotator(PitchDegrees, YawDegrees, 0.0F);
}

bool FPhase3EvidenceCameraContract::IsValid() const
{
    const bool bKnownPreset = CameraPreset == TEXT("FPV") || CameraPreset == TEXT("GIMBAL")
        || CameraPreset == TEXT("NEAR_FOLLOW") || CameraPreset == TEXT("FAR_FOLLOW")
        || CameraPreset == TEXT("SCALE_SUPPORT");
    return !Id.IsNone() && bKnownPreset && VerticalFov >= 35.0F && VerticalFov <= 95.0F
        && GimbalPitch >= FGimbalController::MinimumPitchDegrees
        && GimbalPitch <= FGimbalController::MaximumPitchDegrees;
}

TArray<FPhase3EvidenceCameraContract> FPhase3EvidenceCameraContract::FixedSet()
{
    TArray<FPhase3EvidenceCameraContract> Set;
    auto Add = [&Set](const TCHAR* Id, const FVector& Location, const FRotator& Rotation, const TCHAR* Preset, const float VFoV, const float GimbalPitch)
    {
        FPhase3EvidenceCameraContract Contract;
        Contract.Id = Id;
        Contract.PawnTransform = FTransform(Rotation, Location);
        Contract.CameraPreset = Preset;
        Contract.VerticalFov = VFoV;
        Contract.GimbalPitch = GimbalPitch;
        Set.Add(Contract);
    };
    Add(TEXT("aircraft-three-quarter"), FVector(-320.0F, -180.0F, 115.0F), FRotator(0.0F, 28.0F, 0.0F), TEXT("NEAR_FOLLOW"), 50.0F, -12.0F);
    Add(TEXT("fpv"), FVector::ZeroVector, FRotator::ZeroRotator, TEXT("FPV"), 68.0F, -8.0F);
    Add(TEXT("gimbal-down"), FVector::ZeroVector, FRotator::ZeroRotator, TEXT("GIMBAL"), 58.0F, -78.0F);
    Add(TEXT("near-follow"), FVector(-800.0F, 0.0F, 260.0F), FRotator(-10.0F, 0.0F, 0.0F), TEXT("NEAR_FOLLOW"), 60.0F, -12.0F);
    Add(TEXT("far-follow"), FVector(-1700.0F, 0.0F, 520.0F), FRotator(-16.0F, 0.0F, 0.0F), TEXT("FAR_FOLLOW"), 64.0F, -12.0F);
    Add(TEXT("scale-support"), FVector(-420.0F, -240.0F, 90.0F), FRotator(-2.0F, 30.0F, 0.0F), TEXT("SCALE_SUPPORT"), 48.0F, -12.0F);
    return Set;
}
