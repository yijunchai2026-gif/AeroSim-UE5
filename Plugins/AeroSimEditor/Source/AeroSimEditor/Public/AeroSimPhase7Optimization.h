#pragma once

#include "CoreMinimal.h"

enum class EPhase7OptimizationKind : uint8
{
    Layout,
    Lighting,
    Lod,
    Performance
};

struct AEROSIMEDITOR_API FPhase7OptimizationRequest final
{
    FString RequestId;
    EPhase7OptimizationKind Kind = EPhase7OptimizationKind::Layout;
    FString MapId;
    FString DataLayerId;
    FString QualityProfileId;
    int32 MaximumSuggestions = 8;
};

struct AEROSIMEDITOR_API FPhase7OptimizationSuggestion final
{
    FString SuggestionId;
    EPhase7OptimizationKind Kind = EPhase7OptimizationKind::Layout;
    FString Summary;
    FString Rationale;
    FString Status = TEXT("DRAFT_NOT_USER_CONFIRMED");
    FString RequiredGate = TEXT("G0");
    bool bCanConvertToScenePatch = false;
    bool bApplied = false;
};

/**
 * Finite, deterministic advisory surface. It never mutates the world. Layout suggestions may be
 * translated by a human into the existing Phase 6 ScenePatch transaction; lighting/LOD/performance
 * remain recommendations until a separately allowlisted action and Low/Off rollback exist.
 */
class AEROSIMEDITOR_API FPhase7OptimizationAdvisor final
{
public:
    static bool ValidateRequest(const FPhase7OptimizationRequest& Request, FString& OutError);
    static TArray<FPhase7OptimizationSuggestion> Suggest(const FPhase7OptimizationRequest& Request, FString& OutError);
};
