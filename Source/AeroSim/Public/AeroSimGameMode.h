#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AeroSimGameMode.generated.h"

class APacificCoastGraybox;

UCLASS()
class AEROSIM_API AAeroSimGameMode final : public AGameModeBase
{
    GENERATED_BODY()

public:
    AAeroSimGameMode();
    virtual void StartPlay() override;

private:
    UPROPERTY(Transient)
    TObjectPtr<APacificCoastGraybox> Graybox;
};
