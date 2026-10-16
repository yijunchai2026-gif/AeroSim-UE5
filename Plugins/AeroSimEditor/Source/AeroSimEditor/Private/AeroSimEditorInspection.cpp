#include "AeroSimEditorInspection.h"

#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Modules/ModuleManager.h"

FAeroSimHeartbeat UAeroSimEditorInspection::Heartbeat()
{
    const FAeroSimProjectInspection Inspection = InspectProject();

    FAeroSimHeartbeat Result;
    Result.bModuleLoaded = FModuleManager::Get().IsModuleLoaded(TEXT("AeroSimEditor"));
    Result.bProjectNameReadable = !Inspection.ProjectName.IsEmpty();
    Result.bEngineVersionReadable = !Inspection.EngineVersion.IsEmpty();
    Result.bHealthy = Result.bModuleLoaded
        && Result.bProjectNameReadable
        && Result.bEngineVersionReadable
        && Inspection.bIsEditor;
    Result.Status = Result.bHealthy
        ? TEXT("healthy")
        : FString::Printf(
            TEXT("unhealthy: module=%s project=%s engine=%s editor=%s"),
            Result.bModuleLoaded ? TEXT("ok") : TEXT("missing"),
            Result.bProjectNameReadable ? TEXT("ok") : TEXT("missing"),
            Result.bEngineVersionReadable ? TEXT("ok") : TEXT("missing"),
            Inspection.bIsEditor ? TEXT("ok") : TEXT("false"));
    return Result;
}

FAeroSimProjectInspection UAeroSimEditorInspection::InspectProject()
{
    FAeroSimProjectInspection Result;
    Result.ProjectName = FApp::GetProjectName();
    Result.EngineVersion = FEngineVersion::Current().ToString();
    Result.bIsEditor = GIsEditor;
    Result.AllowedOperations = {
        TEXT("heartbeat"),
        TEXT("capabilities"),
        TEXT("inspect_project"),
        TEXT("inspect_world"),
        TEXT("list_actors"),
        TEXT("inspect_actor"),
        TEXT("inspect_component"),
        TEXT("inspect_asset_metadata"),
        TEXT("inspect_data_layers"),
        TEXT("inspect_world_partition"),
        TEXT("validate_scene"),
        TEXT("capture_request"),
        TEXT("inspect_audit")
    };
    return Result;
}
