#pragma once
#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Components/ActorComponent.h"
#include "AeroSimControlBridge.generated.h"
class FJsonObject;
class ACameraActor;
class AAeroSimGameMode;
class APlayerController;

/** Opt-in local Development control adapter; fixed mailbox, game-thread dispatch. */
UCLASS()
class AEROSIM_API UAeroSimControlBridge final : public UActorComponent
{
    GENERATED_BODY()
public:
    UAeroSimControlBridge();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Function) override;
private:
    friend class FAeroSimControlFlightRollbackTest;
    TSharedRef<FJsonObject> Inspect() const;
    void WriteJson(const FString& Name, const TSharedRef<FJsonObject>& Object) const;
    void Respond(const TSharedRef<FJsonObject>& Object, bool bCache = true);
    TMap<FString,FString> RequestHashes;
    TMap<FString,TSharedPtr<FJsonObject>> Replies;
    TArray<FString> ReplyOrder;
    void Dispatch(const TSharedRef<FJsonObject>& Request);
    void Complete(bool bValid, const FString& Error = TEXT(""));
    void ReleaseKeys();
    void SendPrimaryMouse(bool bDown);
    bool PrimaryMouseHeld = false;
    FVector2D PrimaryMousePosition = FVector2D::ZeroVector;
    void SendKey(const FKey& Key, bool bDown);
    bool Enabled = false;
    FString Directory, Session, LastRequest;
    TSharedPtr<FJsonObject> Pending, Before;
    TArray<FKey> HeldKeys;
    TFunction<void()> Restore;
    double Deadline = 0, Heartbeat = 0;
    int32 CaptureSerial = 0;

    bool BeginReviewView(AAeroSimGameMode* Game, APlayerController* Controller, const FString& View, const FString& ViewMode, FString& Error);
    void RestoreReviewView();
    UPROPERTY(Transient)
    TObjectPtr<ACameraActor> ReviewCamera;
    TWeakObjectPtr<AActor> SavedViewTarget;
    int32 SavedViewMode = 0;
    bool bSavedShowHUD = true;
    bool bReviewActive = false;
    bool bReviewCameraTarget = false;
    int32 SettleFrames = 0;
    double SettleStart = 0;
    int32 WantingAtCapture = 0;
    FString PendingCaptureOutput;
    bool StartReviewFrame(AAeroSimGameMode* Game, double Now);
    int32 SequenceFrames = 1;
    double SequenceInterval = 0.25, NextFrameTime = 0;
    TArray<FString> SequenceOutputs;
};
