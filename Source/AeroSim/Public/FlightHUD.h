#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "FlightHUD.generated.h"

/** Canvas HUD kept in the upper-left safe region so the forward view remains clear. */
UCLASS()
class AEROSIM_API AFlightHUD final : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;
};
