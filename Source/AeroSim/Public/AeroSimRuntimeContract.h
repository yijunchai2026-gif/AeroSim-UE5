#pragma once

#include "CoreMinimal.h"

/** Immutable Phase 6 identity exposed to runtime UI, logs and automation. */
struct AEROSIM_API FAeroSimRuntimeContract final
{
    static constexpr int32 ContractVersion = 1;
    static constexpr const TCHAR* Phase = TEXT("Phase6");
    static constexpr const TCHAR* BuildId = TEXT("AeroSim-Phase6-20261016");
    static constexpr const TCHAR* Contract = TEXT("aerosim.runtime.phase6/v1");
    static constexpr const TCHAR* PreviousContract = TEXT("aerosim.runtime.phase5/v1");
    static constexpr const TCHAR* LegacyContractPhase4 = TEXT("aerosim.runtime.phase4/v1");
    static constexpr const TCHAR* LegacyContractPhase3 = TEXT("aerosim.runtime.phase3/v1");

    static bool IsValid();
};
