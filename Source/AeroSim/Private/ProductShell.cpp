#include "ProductShell.h"

bool FProductShellState::IsValidPage(const EProductPage Value)
{
    return static_cast<uint8>(Value) <= static_cast<uint8>(EProductPage::Replay);
}

bool FProductShellState::IsOverlay(const EProductPage Value)
{
    return Value == EProductPage::Settings || Value == EProductPage::VisualLab;
}

bool FProductShellState::Open(const EProductPage Next, const bool bMissionActive)
{
    if (!IsValidPage(Next)) return false;
    if (bMissionActive && Next != EProductPage::Flight && !IsOverlay(Next))
    {
        Status = TEXT("MISSION ACTIVE - RETURN SAFELY BEFORE CHANGING WORKSPACE");
        return false;
    }
    if (IsOverlay(Next) && !IsOverlay(Page)) ReturnPage = Page;
    Page = Next;
    return true;
}

void FProductShellState::CloseOverlay()
{
    if (IsOverlay(Page)) Page = ReturnPage;
}
