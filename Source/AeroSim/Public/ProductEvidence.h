#pragma once
#include "CoreMinimal.h"

/** Shared portable SHA-256 for bounded evidence bytes; no platform crypto or shell calls. */
AEROSIM_API FString ProductEvidenceSha256(const TArray<uint8>& Bytes);
AEROSIM_API FString ProductEvidenceSha256(const FString& Text);
