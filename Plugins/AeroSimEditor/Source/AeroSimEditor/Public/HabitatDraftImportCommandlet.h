#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "HabitatDraftImportCommandlet.generated.h"

/** Fixed CC0 grass/herb intake only; isolated unconfirmed development packages. */
UCLASS()
class AEROSIMEDITOR_API UHabitatDraftImportCommandlet final : public UCommandlet
{
    GENERATED_BODY()
public:
    UHabitatDraftImportCommandlet();
    virtual int32 Main(const FString& Params) override;
};
