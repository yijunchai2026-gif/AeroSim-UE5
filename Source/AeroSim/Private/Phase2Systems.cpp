#include "Phase2Systems.h"

bool FEnvironmentState::IsValid() const
{
    return Version == CurrentVersion && TimeMinutes == 16 * 60 + 30 && Seed == 21630;
}

void FEnvironmentState::SetWeather(const EAeroSimWeather NewWeather)
{
    Weather = NewWeather;
}

float FEnvironmentState::GetWetness() const
{
    return Weather == EAeroSimWeather::Rain ? 1.0F : 0.0F;
}

float FEnvironmentState::GetCloudCover() const
{
    switch (Weather)
    {
    case EAeroSimWeather::Clear: return 0.08F;
    case EAeroSimWeather::Overcast: return 0.82F;
    case EAeroSimWeather::Rain: return 1.0F;
    default: return 0.08F;
    }
}

float FEnvironmentState::GetLightIntensity() const
{
    switch (Weather)
    {
    case EAeroSimWeather::Clear: return 7.0F;
    case EAeroSimWeather::Overcast: return 2.6F;
    case EAeroSimWeather::Rain: return 1.1F;
    default: return 7.0F;
    }
}

float FEnvironmentState::GetFogDensity() const
{
    switch (Weather)
    {
    case EAeroSimWeather::Clear: return 0.0015F;
    case EAeroSimWeather::Overcast: return 0.012F;
    case EAeroSimWeather::Rain: return 0.028F;
    default: return 0.0015F;
    }
}

float FEnvironmentState::GetExposureBias() const
{
    switch (Weather)
    {
    case EAeroSimWeather::Clear: return 0.15F;
    case EAeroSimWeather::Overcast: return -0.55F;
    case EAeroSimWeather::Rain: return -1.05F;
    default: return 0.15F;
    }
}

float FEnvironmentState::GetSceneSaturation() const
{
    switch (Weather)
    {
    case EAeroSimWeather::Clear: return 1.0F;
    case EAeroSimWeather::Overcast: return 0.72F;
    case EAeroSimWeather::Rain: return 0.46F;
    default: return 1.0F;
    }
}

bool FMissionFlow::CanTransition(const EAeroSimMissionState Next) const
{
    switch (State)
    {
    case EAeroSimMissionState::Hangar: return Next == EAeroSimMissionState::Preflight;
    case EAeroSimMissionState::Preflight: return Next == EAeroSimMissionState::Flight || Next == EAeroSimMissionState::Hangar;
    case EAeroSimMissionState::Flight: return Next == EAeroSimMissionState::Result;
    case EAeroSimMissionState::Result: return Next == EAeroSimMissionState::Replay || Next == EAeroSimMissionState::Hangar;
    case EAeroSimMissionState::Replay: return Next == EAeroSimMissionState::Result || Next == EAeroSimMissionState::Hangar;
    default: return false;
    }
}

bool FMissionFlow::Transition(const EAeroSimMissionState Next)
{
    if (!CanTransition(Next))
    {
        return false;
    }
    State = Next;
    return true;
}

bool FMissionFlow::Advance()
{
    switch (State)
    {
    case EAeroSimMissionState::Hangar: return Transition(EAeroSimMissionState::Preflight);
    case EAeroSimMissionState::Preflight: return Transition(EAeroSimMissionState::Flight);
    case EAeroSimMissionState::Flight: return Transition(EAeroSimMissionState::Result);
    case EAeroSimMissionState::Result: return Transition(EAeroSimMissionState::Replay);
    case EAeroSimMissionState::Replay: return Transition(EAeroSimMissionState::Hangar);
    default: return false;
    }
}

void FMissionFlow::Reset()
{
    State = EAeroSimMissionState::Hangar;
}

bool FVerticalFovTransaction::Preview(const float VerticalDegrees)
{
    if (!FMath::IsFinite(VerticalDegrees)
        || VerticalDegrees < MinimumVerticalFov
        || VerticalDegrees > MaximumVerticalFov)
    {
        return false;
    }
    PreviewVerticalFov = VerticalDegrees;
    bPreviewActive = true;
    return true;
}

bool FVerticalFovTransaction::Apply()
{
    if (!bPreviewActive)
    {
        return false;
    }
    UndoVerticalFov = AppliedVerticalFov;
    AppliedVerticalFov = PreviewVerticalFov;
    bPreviewActive = false;
    return true;
}

void FVerticalFovTransaction::Cancel()
{
    PreviewVerticalFov = AppliedVerticalFov;
    bPreviewActive = false;
}

bool FVerticalFovTransaction::Undo()
{
    if (bPreviewActive || FMath::IsNearlyEqual(AppliedVerticalFov, UndoVerticalFov))
    {
        return false;
    }
    Swap(AppliedVerticalFov, UndoVerticalFov);
    PreviewVerticalFov = AppliedVerticalFov;
    return true;
}

float FVerticalFovTransaction::GetEffectiveVerticalFov() const
{
    return bPreviewActive ? PreviewVerticalFov : AppliedVerticalFov;
}

float FVerticalFovTransaction::VerticalToHorizontal(const float VerticalDegrees, const float AspectRatio)
{
    const float SafeAspect = FMath::Max(AspectRatio, KINDA_SMALL_NUMBER);
    const float VerticalRadians = FMath::DegreesToRadians(FMath::Clamp(VerticalDegrees, 1.0F, 179.0F));
    return FMath::RadiansToDegrees(2.0F * FMath::Atan(FMath::Tan(VerticalRadians * 0.5F) * SafeAspect));
}

float FVerticalFovTransaction::HorizontalToVertical(const float HorizontalDegrees, const float AspectRatio)
{
    const float SafeAspect = FMath::Max(AspectRatio, KINDA_SMALL_NUMBER);
    const float HorizontalRadians = FMath::DegreesToRadians(FMath::Clamp(HorizontalDegrees, 1.0F, 179.0F));
    return FMath::RadiansToDegrees(2.0F * FMath::Atan(FMath::Tan(HorizontalRadians * 0.5F) / SafeAspect));
}

bool FFlightTraceSample::Equals(const FFlightTraceSample& Other, const float Tolerance) const
{
    return FMath::IsNearlyEqual(TimeSeconds, Other.TimeSeconds, Tolerance)
        && Position.Equals(Other.Position, Tolerance)
        && Rotation.Equals(Other.Rotation, Tolerance);
}

void FDeterministicFlightTrace::Reset()
{
    Samples.Reset();
}

void FDeterministicFlightTrace::Record(const float TimeSeconds, const FVector& Position, const FRotator& Rotation)
{
    const float QuantizedTime = FMath::RoundToFloat(FMath::Max(0.0F, TimeSeconds) / SampleIntervalSeconds) * SampleIntervalSeconds;
    if (!Samples.IsEmpty() && QuantizedTime <= Samples.Last().TimeSeconds + KINDA_SMALL_NUMBER)
    {
        return;
    }
    FFlightTraceSample& Sample = Samples.AddDefaulted_GetRef();
    Sample.TimeSeconds = QuantizedTime;
    Sample.Position = Position.GridSnap(0.1);
    Sample.Rotation = FRotator(
        FMath::GridSnap(Rotation.Pitch, 0.01),
        FMath::GridSnap(Rotation.Yaw, 0.01),
        FMath::GridSnap(Rotation.Roll, 0.01));
}

FFlightTraceSample FDeterministicFlightTrace::Evaluate(const float TimeSeconds) const
{
    if (Samples.IsEmpty())
    {
        return FFlightTraceSample();
    }
    if (TimeSeconds <= Samples[0].TimeSeconds)
    {
        return Samples[0];
    }
    for (int32 Index = 1; Index < Samples.Num(); ++Index)
    {
        if (TimeSeconds <= Samples[Index].TimeSeconds)
        {
            const FFlightTraceSample& A = Samples[Index - 1];
            const FFlightTraceSample& B = Samples[Index];
            const float Alpha = FMath::GetRangePct(A.TimeSeconds, B.TimeSeconds, TimeSeconds);
            FFlightTraceSample Result;
            Result.TimeSeconds = TimeSeconds;
            Result.Position = FMath::Lerp(A.Position, B.Position, Alpha);
            Result.Rotation = FMath::Lerp(A.Rotation, B.Rotation, Alpha);
            return Result;
        }
    }
    return Samples.Last();
}

FFlightResultMetrics FDeterministicFlightTrace::BuildResult() const
{
    FFlightResultMetrics Result;
    Result.Samples = Samples.Num();
    if (Samples.IsEmpty())
    {
        return Result;
    }
    Result.DurationSeconds = Samples.Last().TimeSeconds;
    Result.MaximumAltitudeMeters = Samples[0].Position.Z / 100.0F;
    for (int32 Index = 1; Index < Samples.Num(); ++Index)
    {
        Result.DistanceMeters += FVector::Distance(Samples[Index - 1].Position, Samples[Index].Position) / 100.0F;
        Result.MaximumAltitudeMeters = FMath::Max(Result.MaximumAltitudeMeters, Samples[Index].Position.Z / 100.0F);
    }
    return Result;
}
