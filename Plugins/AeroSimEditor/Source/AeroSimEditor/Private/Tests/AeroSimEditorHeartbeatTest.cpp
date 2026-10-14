#if WITH_DEV_AUTOMATION_TESTS

#include "AeroSimEditorInspection.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAeroSimEditorHeartbeatTest,
    "AeroSim.Phase0.Editor.Heartbeat",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAeroSimEditorHeartbeatTest::RunTest(const FString& Parameters)
{
    const FAeroSimHeartbeat Heartbeat = UAeroSimEditorInspection::Heartbeat();
    TestTrue(TEXT("Loaded module is observed"), Heartbeat.bModuleLoaded);
    TestTrue(TEXT("Project name is readable"), Heartbeat.bProjectNameReadable);
    TestTrue(TEXT("Engine version is readable"), Heartbeat.bEngineVersionReadable);
    TestTrue(TEXT("Aggregate heartbeat is healthy"), Heartbeat.bHealthy);
    TestEqual(TEXT("Healthy status is explicit"), Heartbeat.Status, FString(TEXT("healthy")));
    TestEqual(TEXT("Heartbeat service is fixed"), Heartbeat.Service, FString(TEXT("AeroSimEditor")));
    TestEqual(TEXT("Heartbeat protocol is fixed"), Heartbeat.ProtocolVersion, FString(TEXT("1")));

    const FAeroSimProjectInspection Inspection = UAeroSimEditorInspection::InspectProject();
    TestTrue(TEXT("Inspection runs in editor"), Inspection.bIsEditor);
    TestTrue(TEXT("Inspection has a project name"), !Inspection.ProjectName.IsEmpty());
    TestTrue(TEXT("Inspection has an engine version"), !Inspection.EngineVersion.IsEmpty());
    TestEqual(TEXT("Only two read-only operations are advertised"), Inspection.AllowedOperations.Num(), 2);
    return true;
}

#endif
