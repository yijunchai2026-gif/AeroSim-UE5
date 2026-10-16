#if WITH_DEV_AUTOMATION_TESTS

#include "AeroSimEditorMcp.h"
#include "Misc/AutomationTest.h"

namespace
{
FString Request(const TCHAR* Id, const TCHAR* Operation, const TCHAR* Params = TEXT("{}"))
{
    return FString::Printf(TEXT("{\"schema\":\"aerosim.editor.mcp.request/v1\",\"version\":1,\"request_id\":\"%s\",\"operation\":\"%s\",\"params\":%s}"), Id, Operation, Params);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhase5McpProtocolTest, "AeroSim.Phase5.EditorMCP.ProtocolCapabilities", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPhase5McpProtocolTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    const FString Response = UAeroSimEditorMcp::ExecuteRequest(Request(TEXT("cap-1"), TEXT("capabilities")));
    TestTrue(TEXT("Response schema is versioned"), Response.Contains(TEXT("aerosim.editor.mcp.response/v1")));
    TestTrue(TEXT("Request id is correlated"), Response.Contains(TEXT("request_id")) && Response.Contains(TEXT("cap-1")));
    TestTrue(TEXT("Read-only capability is explicit"), Response.Contains(TEXT("read_only")) && Response.Contains(TEXT("true")));
    TestTrue(TEXT("All required read tools are discoverable"), Response.Contains(TEXT("inspect_world")) && Response.Contains(TEXT("inspect_asset_metadata")) && Response.Contains(TEXT("capture_request")));
    TestTrue(TEXT("Dangerous surfaces are explicitly denied"), Response.Contains(TEXT("shell")) && Response.Contains(TEXT("python")) && Response.Contains(TEXT("arbitrary_uobject")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhase5McpReadOnlyTest, "AeroSim.Phase5.EditorMCP.ReadOnlyFailClosed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPhase5McpReadOnlyTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    for (const TCHAR* Mutation : { TEXT("apply_transform"), TEXT("spawn_actor"), TEXT("delete_asset"), TEXT("execute_python"), TEXT("console_command") })
    {
        const FString Response = UAeroSimEditorMcp::ExecuteRequest(Request(TEXT("deny-1"), Mutation));
        TestTrue(*FString::Printf(TEXT("%s is denied"), Mutation), Response.Contains(TEXT("READ_ONLY_DENIED")));
    }
    const FString Unknown = UAeroSimEditorMcp::ExecuteRequest(Request(TEXT("unknown-1"), TEXT("dance")));
    TestTrue(TEXT("Unknown read operation is not treated as mutation"), Unknown.Contains(TEXT("OPERATION_NOT_SUPPORTED")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhase5McpValidationTest, "AeroSim.Phase5.EditorMCP.SchemaLimitsAndAudit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPhase5McpValidationTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    TestTrue(TEXT("Bad schema fails closed"), UAeroSimEditorMcp::ExecuteRequest(TEXT("{\"schema\":\"bad\",\"version\":1,\"request_id\":\"x\",\"operation\":\"heartbeat\",\"params\":{}}" )).Contains(TEXT("SCHEMA_UNSUPPORTED")));
    TestTrue(TEXT("Bad request id fails closed"), UAeroSimEditorMcp::ExecuteRequest(Request(TEXT("bad id"), TEXT("heartbeat"))).Contains(TEXT("INVALID_REQUEST_ID")));
    TestTrue(TEXT("Traversal is denied"), UAeroSimEditorMcp::ExecuteRequest(Request(TEXT("path-1"), TEXT("inspect_asset_metadata"), TEXT("{\"asset_path\":\"/Game/../Secret\"}"))).Contains(TEXT("PATH_NOT_ALLOWED")));
    TestTrue(TEXT("External path is denied"), UAeroSimEditorMcp::ExecuteRequest(Request(TEXT("path-2"), TEXT("inspect_asset_metadata"), TEXT("{\"asset_path\":\"/Users/secret\"}"))).Contains(TEXT("PATH_NOT_ALLOWED")));
    TestTrue(TEXT("Audit is append-only visible"), UAeroSimEditorMcp::InspectAudit(0, 100).Num() >= 4);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhase5McpEvidenceTest, "AeroSim.Phase5.EditorMCP.EvidenceAndGates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPhase5McpEvidenceTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    const FString Capture = UAeroSimEditorMcp::ExecuteRequest(Request(TEXT("capture-1"), TEXT("capture_request"), TEXT("{\"camera_id\":\"near-follow\",\"width\":1920,\"height\":1080}")));
    TestTrue(TEXT("Capture is a request, not a fabricated screenshot"), Capture.Contains(TEXT("EDITOR_CAPTURE_REQUIRED")));
    TestTrue(TEXT("Manifest binds fixed camera"), Capture.Contains(TEXT("near-follow")) && Capture.Contains(TEXT("artifact_sha256")));
    const FString InvalidCamera = UAeroSimEditorMcp::ExecuteRequest(Request(TEXT("capture-2"), TEXT("capture_request"), TEXT("{\"camera_id\":\"free-camera\"}")));
    TestTrue(TEXT("Arbitrary cameras are denied"), InvalidCamera.Contains(TEXT("CAMERA_NOT_ALLOWED")));
    const FString Gates = UAeroSimEditorMcp::ExecuteRequest(Request(TEXT("gates-1"), TEXT("validate_scene")));
    TestTrue(TEXT("Gates remain ordered"), Gates.Contains(TEXT("G0")) && Gates.Contains(TEXT("G4")));
    TestTrue(TEXT("Visual gates require human evidence"), Gates.Contains(TEXT("PENDING_MANUAL_REVIEW")));
    return true;
}

#endif
