#if WITH_DEV_AUTOMATION_TESTS

#include "AeroSimEditorMcp.h"
#include "AeroSimPhase7Optimization.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhase7OptimizationPolicyTest, "AeroSim.Phase7.Editor.OptimizationPolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPhase7OptimizationPolicyTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    FPhase7OptimizationRequest Request;
    Request.RequestId = TEXT("phase7-test");
    Request.MapId = TEXT("/Game/AeroSim/Sandbox/Maps/AI_Review_Sandbox");
    Request.DataLayerId = TEXT("AeroSim_AI_Draft");
    Request.QualityProfileId = TEXT("mac-development-high-v1");
    Request.Kind = EPhase7OptimizationKind::Layout;
    FString Error;
    const TArray<FPhase7OptimizationSuggestion> Suggestions = FPhase7OptimizationAdvisor::Suggest(Request, Error);
    TestTrue(TEXT("Sandbox request is accepted"), Error.IsEmpty() && !Suggestions.IsEmpty());
    for (const FPhase7OptimizationSuggestion& Suggestion : Suggestions)
    {
        TestEqual(TEXT("Every suggestion is unconfirmed draft"), Suggestion.Status, FString(TEXT("DRAFT_NOT_USER_CONFIRMED")));
        TestFalse(TEXT("Suggestion never self-applies"), Suggestion.bApplied);
    }

    Request.MapId = TEXT("/Game/AeroSim/Maps/PacificCoast");
    TestTrue(TEXT("Production map fails closed"), FPhase7OptimizationAdvisor::Suggest(Request, Error).IsEmpty() && Error == TEXT("SANDBOX_ONLY"));

    Request.MapId = TEXT("/Game/AeroSim/Sandbox/Maps/AI_Review_Sandbox");
    Request.Kind = EPhase7OptimizationKind::Performance;
    const TArray<FPhase7OptimizationSuggestion> Performance = FPhase7OptimizationAdvisor::Suggest(Request, Error);
    TestTrue(TEXT("Performance recommendation exists"), Performance.Num() == 1);
    TestFalse(TEXT("Performance recommendation cannot mutate ScenePatch"), Performance[0].bCanConvertToScenePatch);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhase7OptimizationMcpTest, "AeroSim.Phase7.Editor.McpAdvisoryOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPhase7OptimizationMcpTest::RunTest(const FString& Parameters)
{
    static_cast<void>(Parameters);
    const FString Response = UAeroSimEditorMcp::ExecuteRequest(TEXT("{\"schema\":\"aerosim.editor.mcp.request/v1\",\"version\":1,\"request_id\":\"phase7-mcp-test\",\"operation\":\"suggest_optimization\",\"params\":{\"kind\":\"lod\",\"map_id\":\"/Game/AeroSim/Sandbox/Maps/AI_Review_Sandbox\",\"data_layer_id\":\"AeroSim_AI_Draft\",\"quality_profile_id\":\"mac-development-high-v1\",\"maximum_suggestions\":2}}"));
    TSharedPtr<FJsonObject> Root;
    const bool bParsed = FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Response), Root) && Root.IsValid();
    TestTrue(TEXT("MCP response parses"), bParsed);
    if (!bParsed) { return false; }
    TestTrue(TEXT("MCP returns success"), Root->GetBoolField(TEXT("ok")));
    const TSharedPtr<FJsonObject> Data = Root->GetObjectField(TEXT("data"));
    TestTrue(TEXT("Response requires human review"), Data->GetBoolField(TEXT("requires_human_review")));
    TestEqual(TEXT("Response remains draft"), Data->GetStringField(TEXT("status")), FString(TEXT("DRAFT_NOT_USER_CONFIRMED")));
    const TArray<TSharedPtr<FJsonValue>>& Items = Data->GetArrayField(TEXT("items"));
    TestTrue(TEXT("Response contains suggestion"), !Items.IsEmpty());
    TestFalse(TEXT("Response never reports applied"), Items.IsEmpty() ? true : Items[0]->AsObject()->GetBoolField(TEXT("applied")));
    return true;
}

#endif
