#pragma once

#include "CoreMinimal.h"

/** Immutable Phase 2 identity exposed to runtime UI, logs and automation. */
struct AEROSIM_API FAeroSimRuntimeContract final
{
    static constexpr int32 ContractVersion = 1;
    static constexpr const TCHAR* Phase = TEXT("Phase2");
    static constexpr const TCHAR* BuildId = TEXT("AeroSim-Phase2-20261016");
    static constexpr const TCHAR* Contract = TEXT("aerosim.runtime.phase2/v1");

    static bool IsValid();
};
