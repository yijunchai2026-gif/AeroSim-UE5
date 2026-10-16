#include "AeroSimPhase7Optimization.h"

namespace
{
const FString OptimizationSandboxMap = TEXT("/Game/AeroSim/Sandbox/Maps/AI_Review_Sandbox");
const FString OptimizationSandboxLayer = TEXT("AeroSim_AI_Draft");

FPhase7OptimizationSuggestion MakeSuggestion(EPhase7OptimizationKind Kind, const TCHAR* Id, const TCHAR* Summary, const TCHAR* Rationale, const TCHAR* Gate, bool bPatch)
{
    FPhase7OptimizationSuggestion Result;
    Result.Kind = Kind;
    Result.SuggestionId = Id;
    Result.Summary = Summary;
    Result.Rationale = Rationale;
    Result.RequiredGate = Gate;
    Result.bCanConvertToScenePatch = bPatch;
    return Result;
}
}

bool FPhase7OptimizationAdvisor::ValidateRequest(const FPhase7OptimizationRequest& Request, FString& OutError)
{
    if (Request.RequestId.IsEmpty() || Request.RequestId.Len() > 64) { OutError = TEXT("INVALID_REQUEST_ID"); return false; }
    if (Request.MapId != OptimizationSandboxMap || Request.DataLayerId != OptimizationSandboxLayer) { OutError = TEXT("SANDBOX_ONLY"); return false; }
    if (Request.QualityProfileId != TEXT("mac-development-high-v1")
        && Request.QualityProfileId != TEXT("windows-1440p-high-candidate-v1")
        && Request.QualityProfileId != TEXT("ipad-device-candidate-v1"))
    {
        OutError = TEXT("QUALITY_PROFILE_NOT_ALLOWED"); return false;
    }
    if (Request.MaximumSuggestions < 1 || Request.MaximumSuggestions > 8) { OutError = TEXT("SUGGESTION_QUOTA"); return false; }
    OutError.Reset();
    return true;
}

TArray<FPhase7OptimizationSuggestion> FPhase7OptimizationAdvisor::Suggest(const FPhase7OptimizationRequest& Request, FString& OutError)
{
    TArray<FPhase7OptimizationSuggestion> Result;
    if (!ValidateRequest(Request, OutError)) { return Result; }
    switch (Request.Kind)
    {
    case EPhase7OptimizationKind::Layout:
        Result.Add(MakeSuggestion(Request.Kind, TEXT("layout.route-clearance.v1"), TEXT("Preview route-gate spacing in Sandbox"), TEXT("Preserve the fixed flight corridor and support G0-G2 review."), TEXT("G0"), true));
        Result.Add(MakeSuggestion(Request.Kind, TEXT("layout.support-contact.v1"), TEXT("Review draft support contact"), TEXT("Flag candidate draft transforms for human ground-contact inspection."), TEXT("G1"), true));
        break;
    case EPhase7OptimizationKind::Lighting:
        Result.Add(MakeSuggestion(Request.Kind, TEXT("lighting.exposure-review.v1"), TEXT("Compare fixed Clear 16:30 exposure"), TEXT("Capture with the fixed cameras; do not auto-write lighting."), TEXT("G3"), false));
        break;
    case EPhase7OptimizationKind::Lod:
        Result.Add(MakeSuggestion(Request.Kind, TEXT("lod.low-off-review.v1"), TEXT("Author explicit Low/Off fallback before enabling high-cost LOD path"), TEXT("Nanite/HLOD decisions require platform measurements and pop review."), TEXT("G4"), false));
        break;
    case EPhase7OptimizationKind::Performance:
        Result.Add(MakeSuggestion(Request.Kind, TEXT("performance.capture-required.v1"), TEXT("Collect target-build CPU/GPU/memory/draw/streaming evidence"), TEXT("Budgets are not measurements; no optimization is applied without a complete capture."), TEXT("G4"), false));
        break;
    default:
        OutError = TEXT("KIND_NOT_ALLOWED");
        return {};
    }
    Result.SetNum(FMath::Min(Result.Num(), Request.MaximumSuggestions));
    return Result;
}
