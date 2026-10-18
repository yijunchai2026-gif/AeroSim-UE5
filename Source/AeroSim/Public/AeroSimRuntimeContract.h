#pragma once

#include "CoreMinimal.h"

/** Immutable Phase 7 identity plus compatible product UX schema exposed to runtime UI, logs and automation. */
struct AEROSIM_API FAeroSimRuntimeContract final
{
    static constexpr int32 ContractVersion = 1;
    static constexpr const TCHAR* Phase = TEXT("Phase7");
    static constexpr const TCHAR* BuildId = TEXT("AeroSim-Phase7-ProductShellA-20261016-v1");
    static constexpr const TCHAR* Contract = TEXT("aerosim.runtime.phase7/v1");
    static constexpr const TCHAR* ProductUxSchema = TEXT("aerosim.product-ux/v1");
    static constexpr const TCHAR* PreviousContract = TEXT("aerosim.runtime.phase6/v1");
    static constexpr const TCHAR* LegacyContractPhase5 = TEXT("aerosim.runtime.phase5/v1");
    static constexpr const TCHAR* LegacyContractPhase4 = TEXT("aerosim.runtime.phase4/v1");
    static constexpr const TCHAR* LegacyContractPhase3 = TEXT("aerosim.runtime.phase3/v1");

    static bool IsValid();
};
