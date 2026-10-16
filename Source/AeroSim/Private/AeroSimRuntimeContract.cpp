#include "AeroSimRuntimeContract.h"

bool FAeroSimRuntimeContract::IsValid()
{
    return ContractVersion == 1
        && FCString::Strlen(Phase) > 0
        && FCString::Strlen(BuildId) > 0
        && FCString::Strcmp(Contract, TEXT("aerosim.runtime.phase6/v1")) == 0;
}
