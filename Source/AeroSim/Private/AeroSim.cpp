#include "Modules/ModuleManager.h"

#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Phase7Product.h"
#include "Containers/Ticker.h"

DEFINE_LOG_CATEGORY_STATIC(LogAeroSimPhase7, Log, All);

class FAeroSimModule final : public FDefaultGameModuleImpl
{
public:
    virtual void StartupModule() override
    {
        int32 RequestedSeconds = 0;
        if (!FParse::Value(FCommandLine::Get(), TEXT("Phase7SoakSeconds="), RequestedSeconds))
        {
            return;
        }
        SoakSeconds = FMath::Clamp(RequestedSeconds, 1, 1800);
        int32 StepsA = 0;
        int32 StepsB = 0;
        Digest = FPhase7DeterministicSoak::Run(SoakSeconds, 21630, StepsA);
        const uint64 ReplayDigest = FPhase7DeterministicSoak::Run(SoakSeconds, 21630, StepsB);
        if (StepsA != StepsB || Digest != ReplayDigest)
        {
            UE_LOG(LogAeroSimPhase7, Error, TEXT("PHASE7_SOAK_FAIL reason=DETERMINISM_MISMATCH"));
            FPlatformMisc::RequestExitWithStatus(true, 2);
            return;
        }
        StartedSeconds = FPlatformTime::Seconds();
        UE_LOG(LogAeroSimPhase7, Display, TEXT("PHASE7_SOAK_START duration_s=%d steps=%d seed=21630 runtime=aerosim.runtime.phase7/v1"), SoakSeconds, StepsA);
        Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FAeroSimModule::TickSoak), 1.0F);
    }

    virtual void ShutdownModule() override
    {
        if (Ticker.IsValid())
        {
            FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
            Ticker.Reset();
        }
    }

private:
    bool TickSoak(float)
    {
        const double Elapsed = FPlatformTime::Seconds() - StartedSeconds;
        if (Elapsed < static_cast<double>(SoakSeconds))
        {
            return true;
        }
        UE_LOG(LogAeroSimPhase7, Display, TEXT("PHASE7_SOAK_PASS duration_s=%d digest=%llu runtime=aerosim.runtime.phase7/v1"), SoakSeconds, static_cast<unsigned long long>(Digest));
        Ticker.Reset();
        FPlatformMisc::RequestExitWithStatus(false, 0);
        return false;
    }

    FTSTicker::FDelegateHandle Ticker;
    double StartedSeconds = 0.0;
    int32 SoakSeconds = 0;
    uint64 Digest = 0;
};

IMPLEMENT_PRIMARY_GAME_MODULE(FAeroSimModule, AeroSim, "AeroSim");
