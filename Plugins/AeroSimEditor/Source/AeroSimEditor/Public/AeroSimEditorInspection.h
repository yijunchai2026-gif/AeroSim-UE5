#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AeroSimEditorInspection.generated.h"

USTRUCT(BlueprintType)
struct AEROSIMEDITOR_API FAeroSimHeartbeat
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "AeroSim|Editor")
    bool bHealthy = false;

    UPROPERTY(BlueprintReadOnly, Category = "AeroSim|Editor")
    bool bModuleLoaded = false;

    UPROPERTY(BlueprintReadOnly, Category = "AeroSim|Editor")
    bool bProjectNameReadable = false;

    UPROPERTY(BlueprintReadOnly, Category = "AeroSim|Editor")
    bool bEngineVersionReadable = false;

    UPROPERTY(BlueprintReadOnly, Category = "AeroSim|Editor")
    FString Service = TEXT("AeroSimEditor");

    UPROPERTY(BlueprintReadOnly, Category = "AeroSim|Editor")
    FString ProtocolVersion = TEXT("1");

    UPROPERTY(BlueprintReadOnly, Category = "AeroSim|Editor")
    FString Status;
};

USTRUCT(BlueprintType)
struct AEROSIMEDITOR_API FAeroSimProjectInspection
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "AeroSim|Editor")
    FString ProjectName;

    UPROPERTY(BlueprintReadOnly, Category = "AeroSim|Editor")
    FString EngineVersion;

    UPROPERTY(BlueprintReadOnly, Category = "AeroSim|Editor")
    bool bIsEditor = false;

    UPROPERTY(BlueprintReadOnly, Category = "AeroSim|Editor")
    TArray<FString> AllowedOperations;
};

/**
 * Fixed, read-only editor health and metadata surface.
 * Intentionally exposes no Python, shell, console-command, file-write, or process APIs.
 */
UCLASS()
class AEROSIMEDITOR_API UAeroSimEditorInspection final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "AeroSim|Editor")
    static FAeroSimHeartbeat Heartbeat();

    UFUNCTION(BlueprintPure, Category = "AeroSim|Editor")
    static FAeroSimProjectInspection InspectProject();
};
