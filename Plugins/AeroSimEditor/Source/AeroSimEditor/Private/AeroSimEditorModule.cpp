#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogAeroSimEditor, Log, All);

class FAeroSimEditorModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        UE_LOG(LogAeroSimEditor, Log, TEXT("AeroSimEditor module started; heartbeat becomes healthy after module load completes."));
    }

    virtual void ShutdownModule() override
    {
    }
};

IMPLEMENT_MODULE(FAeroSimEditorModule, AeroSimEditor)
