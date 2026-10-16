#include "AeroSimRuntimeContract.h"

bool FAeroSimRuntimeContract::IsValid()
{
    return ContractVersion == 1
        && FCString::Strlen(Phase) > 0
        && FCString::Strlen(BuildId) > 0
        && FCString::Strcmp(Contract, TEXT("aerosim.runtime.phase7/v1")) == 0
        && FCString::Strcmp(ProductUxSchema, TEXT("aerosim.product-ux/v1")) == 0;
}
