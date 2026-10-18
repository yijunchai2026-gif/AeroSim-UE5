#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "CoastDraftImportCommandlet.generated.h"

class UMaterialFunctionInterface;
class UTexture2D;

/** Editor-only, fixed-whitelist importer for unconfirmed Phase 7 environment draft content. */
UCLASS()
class AEROSIMEDITOR_API UCoastDraftImportCommandlet final : public UCommandlet
{
    GENERATED_BODY()

public:
    UCoastDraftImportCommandlet();
    virtual int32 Main(const FString& Params) override;

private:
    UPROPERTY()
    TObjectPtr<UMaterialFunctionInterface> WorldAlignedTexture;
    UPROPERTY()
    TObjectPtr<UMaterialFunctionInterface> WorldAlignedNormal;
    UPROPERTY()
    TObjectPtr<UTexture2D> WaterNormal;
};
