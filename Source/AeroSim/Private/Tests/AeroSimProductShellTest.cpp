#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "ProductShell.h"
#include "ProductEvidence.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProductNavigationSafetyTest, "AeroSim.ProductShell.Navigation.FailClosed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProductNavigationSafetyTest::RunTest(const FString& Parameters)
{
    FProductShellState Shell;
    TestEqual(TEXT("First run has visible Home"), Shell.Page, EProductPage::Home);
    TestTrue(TEXT("Home to Hangar"), Shell.Open(EProductPage::Hangar, false));
    TestTrue(TEXT("Hangar to mission brief"), Shell.Open(EProductPage::Mission, false));
    TestTrue(TEXT("Settings opens as overlay"), Shell.Open(EProductPage::Settings, false));
    Shell.CloseOverlay();
    TestEqual(TEXT("Cancel preserves prior page"), Shell.Page, EProductPage::Mission);
    Shell.Page = EProductPage::Flight;
    TestFalse(TEXT("Active flight cannot open Home"), Shell.Open(EProductPage::Home, true));
    TestFalse(TEXT("Active flight cannot change aircraft"), Shell.Open(EProductPage::Hangar, true));
    TestFalse(TEXT("Unknown page denied"), Shell.Open(static_cast<EProductPage>(255), false));
    TestTrue(TEXT("Active flight can open safe settings"), Shell.Open(EProductPage::Settings, true));
    Shell.CloseOverlay();
    TestEqual(TEXT("Settings returns to flight"), Shell.Page, EProductPage::Flight);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProductEvidenceHashTest, "AeroSim.ProductShell.Evidence.Sha256KnownVectors", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProductEvidenceHashTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Empty SHA256"), ProductEvidenceSha256(FString()), FString(TEXT("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")));
    TestEqual(TEXT("abc SHA256"), ProductEvidenceSha256(FString(TEXT("abc"))), FString(TEXT("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")));
    TestEqual(TEXT("Multiblock SHA256"), ProductEvidenceSha256(FString(TEXT("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"))), FString(TEXT("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1")));
    return true;
}
#endif
