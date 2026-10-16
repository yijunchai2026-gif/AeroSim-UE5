#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AeroSimEditorMcp.generated.h"

/** Fixed limits for the in-process, read-only Editor MCP protocol. */
USTRUCT(BlueprintType)
struct AEROSIMEDITOR_API FAeroSimEditorMcpLimits
{
    GENERATED_BODY()

    static constexpr int32 DefaultPageSize = 50;
    static constexpr int32 MaximumPageSize = 100;
    static constexpr int32 DefaultTimeoutMs = 250;
    static constexpr int32 MaximumTimeoutMs = 2000;
    static constexpr int32 MaximumRequestBytes = 16384;
    static constexpr int32 MaximumResponseBytes = 65536;
};

/** Sanitized append-only audit entry. Raw request bodies and secrets are never retained. */
USTRUCT(BlueprintType)
struct AEROSIMEDITOR_API FAeroSimEditorMcpAuditEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "AeroSim|Editor MCP")
    FString TimestampUtc;

    UPROPERTY(BlueprintReadOnly, Category = "AeroSim|Editor MCP")
    FString RequestId;

    UPROPERTY(BlueprintReadOnly, Category = "AeroSim|Editor MCP")
    FString Operation;

    UPROPERTY(BlueprintReadOnly, Category = "AeroSim|Editor MCP")
    FString ResultCode;

    UPROPERTY(BlueprintReadOnly, Category = "AeroSim|Editor MCP")
    FString RequestDigest;

    UPROPERTY(BlueprintReadOnly, Category = "AeroSim|Editor MCP")
    int32 DurationMs = 0;
};

/**
 * In-process Phase 5 Editor MCP dispatcher.
 *
 * There is deliberately no socket/server, filesystem, shell, Python, console command,
 * process launch, arbitrary reflection, arbitrary UObject invocation, or mutation API.
 * The caller supplies JSON and receives JSON on the Editor game thread.
 */
UCLASS()
class AEROSIMEDITOR_API UAeroSimEditorMcp final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    static constexpr int32 ProtocolVersion = 1;

    UFUNCTION(BlueprintCallable, Category = "AeroSim|Editor MCP")
    static FString ExecuteRequest(const FString& RequestJson);

    UFUNCTION(BlueprintPure, Category = "AeroSim|Editor MCP")
    static TArray<FAeroSimEditorMcpAuditEntry> InspectAudit(int32 Offset = 0, int32 Limit = 50);

    static FString ProtocolSchema();
    static FString RuntimeContract();
};
