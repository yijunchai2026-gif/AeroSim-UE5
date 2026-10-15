#pragma once

#include "CoreMinimal.h"

/** Immutable Phase 1 identity exposed to runtime UI, logs and automation. */
struct AEROSIM_API FAeroSimRuntimeContract final
{
    static constexpr int32 ContractVersion = 1;
    static constexpr const TCHAR* Phase = TEXT("Phase1");
    static constexpr const TCHAR* BuildId = TEXT("AeroSim-Phase1-20261016");
    static constexpr const TCHAR* Contract = TEXT("aerosim.runtime.phase1/v1");

    static bool IsValid();
};
