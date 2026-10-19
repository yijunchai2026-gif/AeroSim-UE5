#include "AeroSimControlBridge.h"
#include "AeroSimGameMode.h"
#include "AeroSimProductWidget.h"
#include "Widgets/SWindow.h"
#include "AeroSimPlayerController.h"
#include "AeroSimRuntimeContract.h"
#include "FlightPawn.h"
#include "ProductEvidence.h"
#include "Dom/JsonObject.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"
#include "Widgets/SWidget.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "ContentStreaming.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/HUD.h"
#include "HAL/IConsoleManager.h"
#include "Phase2Systems.h"

namespace
{
IPlatformFile& BridgeFiles(){return FPlatformFileManager::Get().GetPlatformFile();}
bool WriteBridgeUtf8(const FString& Path,const FString& Text,bool Append=false)
{
    FTCHARToUTF8 Bytes(*Text);TUniquePtr<IFileHandle> File(BridgeFiles().OpenWrite(*Path,Append));
    return File&&File->Write(reinterpret_cast<const uint8*>(Bytes.Get()),Bytes.Length())&&File->Flush(true);
}
void ReplaceBridgeFile(const FString& Destination,const FString& Source)
{
    // Fixed mailbox names only; the reader retries a missing or partial response.
    BridgeFiles().DeleteFile(*Destination);BridgeFiles().MoveFile(*Destination,*Source);
}
FString PageLabel(EProductPage Page)
{
    const TCHAR* Names[] = {TEXT("Home"),TEXT("Hangar"),TEXT("Mission"),TEXT("Preflight"),TEXT("Flight"),TEXT("Settings"),TEXT("VisualLab"),TEXT("Result"),TEXT("Replay")};
    const int32 Index=static_cast<int32>(Page); return Index>=0 && Index<UE_ARRAY_COUNT(Names)?Names[Index]:TEXT("Unknown");
}
FKey AllowedKey(const FString& Name)
{
    const FKey Keys[] = {EKeys::W,EKeys::S,EKeys::A,EKeys::D,EKeys::Q,EKeys::E,EKeys::LeftShift,EKeys::LeftControl,
        EKeys::C,EKeys::Escape,EKeys::Enter,EKeys::SpaceBar,EKeys::F10,EKeys::H,EKeys::R};
    for(const FKey& Key:Keys) if(Key.GetFName().ToString()==Name) return Key;
    return FKey();
}
bool ParseViewMode(const FString& Name, EViewModeIndex& Mode)
{
    struct FEntry { const TCHAR* Name; EViewModeIndex Mode; };
    static const FEntry Entries[] = {
        {TEXT("lit"),VMI_Lit},{TEXT("lighting_only"),VMI_LightingOnly},{TEXT("detail_lighting"),VMI_Lit_DetailLighting},
        {TEXT("unlit"),VMI_Unlit}};
    for(const FEntry& Entry:Entries) if(Name.Equals(Entry.Name,ESearchCase::CaseSensitive)){Mode=Entry.Mode;return true;}
    return false;
}
bool AllowedReviewView(const FString& Name)
{
    if(!Name.Equals(Name.ToLower(),ESearchCase::CaseSensitive)) return false;
    FVector Location;FRotator Rotation;
    return Name==TEXT("player") || AAeroSimGameMode::ResolveReviewView(Name,8,Location,Rotation);
}
void VectorFields(const TSharedRef<FJsonObject>& Object, const TCHAR* Prefix, const FVector& Value)
{
    Object->SetNumberField(FString(Prefix)+TEXT("_x"),Value.X);
    Object->SetNumberField(FString(Prefix)+TEXT("_y"),Value.Y);
    Object->SetNumberField(FString(Prefix)+TEXT("_z"),Value.Z);
}
}
UAeroSimControlBridge::UAeroSimControlBridge()
{
    PrimaryComponentTick.bCanEverTick=true;
    PrimaryComponentTick.bTickEvenWhenPaused=true;
}
void UAeroSimControlBridge::WriteJson(const FString& Name,const TSharedRef<FJsonObject>& Object) const
{
    FString Text; FJsonSerializer::Serialize(Object,TJsonWriterFactory<>::Create(&Text));
    const FString Path=Directory/Name, Temp=Path+TEXT(".tmp");
    if(WriteBridgeUtf8(Temp,Text))ReplaceBridgeFile(Path,Temp);
}
void UAeroSimControlBridge::BeginPlay()
{
    Super::BeginPlay();
#if !UE_BUILD_SHIPPING
    Enabled=FParse::Param(FCommandLine::Get(),TEXT("AeroSimControlBridge"));
#endif
    if(!Enabled){SetComponentTickEnabled(false);return;}
    Directory=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("ControlBridge"));
    BridgeFiles().CreateDirectoryTree(*Directory);
    FString Existing;
    TSharedPtr<FJsonObject> Previous;
    if(FFileHelper::LoadFileToString(Existing,*(Directory/TEXT("session.json")))
        &&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Existing),Previous)&&Previous.IsValid())
    {
        double Pid=0;
        if(Previous->TryGetNumberField(TEXT("pid"),Pid)&&Pid>0&&Pid<4294967295.0
            &&FPlatformProcess::IsApplicationRunning(static_cast<uint32>(Pid)))
        {
            Enabled=false;SetComponentTickEnabled(false);
            UE_LOG(LogTemp,Warning,TEXT("AEROSIM_CONTROL_BRIDGE_ALREADY_OWNED"));return;
        }
    }
    Session=FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UE_LOG(LogTemp,Display,TEXT("AEROSIM_CONTROL_BRIDGE_READY session=%s directory=%s"),*Session,*Directory);
}
void UAeroSimControlBridge::EndPlay(const EEndPlayReason::Type Reason)
{
    if(Enabled){ReleaseKeys();if(Pending.IsValid()) Complete(false,TEXT("SESSION_ENDED"));RestoreReviewView();}
    Super::EndPlay(Reason);
}
bool UAeroSimControlBridge::BeginReviewView(AAeroSimGameMode* Game,APlayerController* Controller,const FString& View,const FString& ViewMode,FString& Error)
{
    EViewModeIndex Mode=VMI_Lit;
    UGameViewportClient* Client=GetWorld()->GetGameViewport();
    if(!Client||!Controller->PlayerCameraManager||!ParseViewMode(ViewMode,Mode)){Error=TEXT("VIEWPORT_UNAVAILABLE");return false;}
    SavedViewTarget=Controller->GetViewTarget();SavedViewMode=Client->ViewModeIndex;
    AHUD* Hud=Controller->GetHUD();bSavedShowHUD=Hud!=nullptr&&Hud->bShowHUD;
    bReviewActive=true;bReviewCameraTarget=View!=TEXT("player");
    if(bReviewCameraTarget)
    {
        FVector Location;FRotator Rotation;
        if(!AAeroSimGameMode::ResolveReviewView(View,8,Location,Rotation)){Error=TEXT("VIEW_NOT_ALLOWED");return false;}
        if(!ReviewCamera)
        {
            FActorSpawnParameters Parameters;Parameters.ObjectFlags|=RF_Transient;
            ReviewCamera=GetWorld()->SpawnActor<ACameraActor>(Location,Rotation,Parameters);
        }
        if(!ReviewCamera){Error=TEXT("REVIEW_CAMERA_UNAVAILABLE");return false;}
        ReviewCamera->SetActorLocationAndRotation(Location,Rotation);
        int32 Width=0,Height=0;Controller->GetViewportSize(Width,Height);
        ReviewCamera->GetCameraComponent()->SetFieldOfView(FVerticalFovTransaction::VerticalToHorizontal(
            Game->AppliedSettings.VerticalFov,Height>0?static_cast<float>(Width)/Height:1.0F));
        Controller->SetViewTarget(ReviewCamera);
        Controller->PlayerCameraManager->UpdateCamera(0.0F);
    }
    if(Hud) Hud->bShowHUD=false;
    // Cooked builds refuse non-Lit modes unless r.ForceDebugViewModes=1, but leaving it on crashes every cooked
    // screenshot (ConfigureBufferVisualizationSettings reads an editor-only cvar). Enable only around SetViewMode.
    IConsoleVariable* ForceDebugViewModes=IConsoleManager::Get().FindConsoleVariable(TEXT("r.ForceDebugViewModes"));
    const int32 PreviousForce=ForceDebugViewModes?ForceDebugViewModes->GetInt():0;
    if(ForceDebugViewModes&&Mode!=VMI_Lit) ForceDebugViewModes->Set(1,ECVF_SetByCode);
    Client->SetViewMode(Mode);
    if(ForceDebugViewModes&&Mode!=VMI_Lit) ForceDebugViewModes->Set(PreviousForce,ECVF_SetByCode);
    if(Client->ViewModeIndex!=Mode){Error=TEXT("VIEW_MODE_UNAVAILABLE");return false;}
    Game->EvidenceReviewView=View;Game->EvidenceViewMode=ViewMode;
    return true;
}
bool UAeroSimControlBridge::StartReviewFrame(AAeroSimGameMode* Game,const double Now)
{
    auto* PC=GetWorld()->GetFirstPlayerController();
    if(bReviewCameraTarget&&(!PC||PC->GetViewTarget()!=ReviewCamera)){Complete(false,TEXT("VIEW_TARGET_OVERRIDDEN"));return false;}
    if(FScreenshotRequest::IsScreenshotRequested()){Complete(false,TEXT("CAPTURE_BUSY"));return false;}
    Game->EvidenceSequenceIndex=SequenceOutputs.Num();Game->EvidenceSequenceFrames=SequenceFrames;
    Game->CaptureProductEvidence();
    if(!FScreenshotRequest::IsScreenshotRequested()){Complete(false,TEXT("CAPTURE_NOT_STARTED"));return false;}
    PendingCaptureOutput=Game->LastProductEvidenceOutput;SequenceOutputs.Add(PendingCaptureOutput);
    NextFrameTime=Now+SequenceInterval;
    return true;
}
void UAeroSimControlBridge::RestoreReviewView()
{
    if(!bReviewActive) return;
    bReviewActive=false;
    if(auto* Game=Cast<AAeroSimGameMode>(GetOwner())){Game->EvidenceReviewView.Reset();Game->EvidenceViewMode.Reset();}
    UWorld* World=GetWorld();if(!World) return;
    if(UGameViewportClient* Client=World->GetGameViewport()) Client->SetViewMode(static_cast<EViewModeIndex>(SavedViewMode));
    if(APlayerController* Controller=World->GetFirstPlayerController())
    {
        if(AHUD* Hud=Controller->GetHUD()) Hud->bShowHUD=bSavedShowHUD;
        if(bReviewCameraTarget&&Controller->GetViewTarget()==ReviewCamera&&SavedViewTarget.IsValid()) Controller->SetViewTarget(SavedViewTarget.Get());
    }
    bReviewCameraTarget=false;SavedViewTarget.Reset();
}
TSharedRef<FJsonObject> UAeroSimControlBridge::Inspect() const
{
    auto State=MakeShared<FJsonObject>();
    State->SetStringField(TEXT("runtime_id"),TEXT("player:0"));
    State->SetStringField(TEXT("session"),Session);
    State->SetStringField(TEXT("source_fingerprint"),TEXT(AEROSIM_SOURCE_FINGERPRINT));
    State->SetStringField(TEXT("build_id"),FAeroSimRuntimeContract::BuildId);
    State->SetNumberField(TEXT("pid"),FPlatformProcess::GetCurrentProcessId());
    const auto* Mode=Cast<AAeroSimGameMode>(GetOwner());
    auto* PC=GetWorld()?GetWorld()->GetFirstPlayerController():nullptr;
    const auto* Pawn=PC?Cast<AFlightPawn>(PC->GetPawn()):nullptr;
    State->SetBoolField(TEXT("possessed"),Pawn!=nullptr);
    State->SetBoolField(TEXT("world_paused"),GetWorld() && GetWorld()->IsPaused());
    State->SetStringField(TEXT("page"),Mode?PageLabel(Mode->GetProductPage()):TEXT("Unknown"));
    if(Mode)
    {
        // DEV-FLIGHT: read-only navigation state for real-input smoke evidence; never a write surface.
        const auto& Mission=Mode->GetPhase4Mission();const auto& Guide=Mission.GetCurrentGuidanceTarget();
        State->SetNumberField(TEXT("mission_stage"),static_cast<int32>(Mission.Stage));
        State->SetNumberField(TEXT("next_checkpoint"),Mission.NextCheckpointIndex);
        State->SetNumberField(TEXT("mission_incident"),static_cast<int32>(Mission.Feedback.Incident));
        State->SetNumberField(TEXT("guide_leg_index"),Guide.LegIndex);
        State->SetNumberField(TEXT("guide_leg_count"),Guide.LegCount);
        State->SetStringField(TEXT("guide_leg_id"),Guide.LegId.ToString());
        State->SetStringField(TEXT("guide_id"),Guide.GuideId.ToString());
        State->SetBoolField(TEXT("guide_recovery"),Guide.bRecovery);
        State->SetNumberField(TEXT("guide_progress_cm"),Mission.GetGuidanceLegProgressCm());
        VectorFields(State,TEXT("guide_target_cm"),Guide.Position);
        VectorFields(State,TEXT("guide_point_cm"),Guide.GuidePoint);
    }
    if(FSlateApplication::IsInitialized())
    {
        const auto Focus=FSlateApplication::Get().GetKeyboardFocusedWidget();
        State->SetStringField(TEXT("slate_focus"),Focus.IsValid()?Focus->GetTypeAsString():TEXT("None"));
        State->SetBoolField(TEXT("application_active"),FSlateApplication::Get().IsActive());
    }
    if(PC)
    {
        State->SetBoolField(TEXT("cursor_visible"),PC->bShowMouseCursor);
        State->SetBoolField(TEXT("ignore_move"),PC->IsMoveInputIgnored());
        State->SetBoolField(TEXT("ignore_look"),PC->IsLookInputIgnored());
        if(auto* LP=PC->GetLocalPlayer()) if(auto* Sub=ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
            State->SetBoolField(TEXT("mapping_active"),Pawn && Sub->HasMappingContext(Pawn->GetInputSuite().MappingContext));
        if(auto* View=GetWorld()->GetGameViewport()) if(View->Viewport)
        {
            State->SetBoolField(TEXT("viewport_focus"),View->Viewport->HasFocus());
            State->SetBoolField(TEXT("mouse_captured"),View->Viewport->HasMouseCapture());
        }
    }
    if(Pawn)
    {
        const auto& Input=Pawn->GetControlInput();
        State->SetNumberField(TEXT("phase"),static_cast<int32>(Pawn->GetFlightModel().Phase));
        State->SetNumberField(TEXT("resume_seconds"),Pawn->GetResumeCountdownSeconds());
        State->SetBoolField(TEXT("resume_pending"),Pawn->IsResumeCountdownActive());
        State->SetBoolField(TEXT("physical_neutral"),Pawn->IsPhysicalInputNeutral());
        State->SetNumberField(TEXT("pitch"),Input.Pitch);State->SetNumberField(TEXT("roll"),Input.Roll);
        State->SetNumberField(TEXT("yaw"),Input.Yaw);State->SetNumberField(TEXT("throttle"),Input.Throttle);
        VectorFields(State,TEXT("position_cm"),Pawn->GetFlightModel().Position);
        VectorFields(State,TEXT("actor_cm"),Pawn->GetActorLocation());
        State->SetNumberField(TEXT("yaw_degrees"),Pawn->GetFlightModel().Rotation.Yaw);
        State->SetNumberField(TEXT("pitch_degrees"),Pawn->GetFlightModel().Rotation.Pitch);
        State->SetNumberField(TEXT("camera_mode"),static_cast<int32>(Pawn->GetCameraMode()));
        auto* Enhanced=PC?Cast<UEnhancedPlayerInput>(PC->PlayerInput):nullptr;
        State->SetBoolField(TEXT("enhanced_player_input"),Enhanced!=nullptr);
        if(Enhanced)
        {
            const auto& Suite=Pawn->GetInputSuite();
            State->SetNumberField(TEXT("mapped_throttle"),Enhanced->GetActionValue(Suite.ThrottleAction).Get<float>());
            State->SetNumberField(TEXT("mapped_pitch"),Enhanced->GetActionValue(Suite.PitchAction).Get<float>());
            State->SetNumberField(TEXT("mapped_roll"),Enhanced->GetActionValue(Suite.RollAction).Get<float>());
            State->SetNumberField(TEXT("mapped_yaw"),Enhanced->GetActionValue(Suite.YawAction).Get<float>());
        }
    }
    return State;
}
void UAeroSimControlBridge::SendKey(const FKey& Key,bool bDown)
{
    if(!FSlateApplication::IsInitialized()) return;
    auto* PC=GetWorld()->GetFirstPlayerController();
    const uint32 User=PC && PC->GetLocalPlayer()?static_cast<uint32>(PC->GetLocalPlayer()->GetControllerId()):0;
    const FKeyEvent Event(Key,FModifierKeysState(),User,false,0,0);
    if(bDown) FSlateApplication::Get().ProcessKeyDownEvent(Event);
    else FSlateApplication::Get().ProcessKeyUpEvent(Event);
}
void UAeroSimControlBridge::SendPrimaryMouse(bool bDown)
{
    auto* Game=Cast<AAeroSimGameMode>(GetOwner());
    if(!Game||!Game->ProductWidget||!FSlateApplication::IsInitialized()) return;
    auto& Slate=FSlateApplication::Get();
    if(bDown)
    {
        auto Target=Game->ProductWidget->PrimaryFocusTarget.Pin();
        if(!Target.IsValid()||!Target->IsEnabled()) return;
        const auto& Geometry=Target->GetCachedGeometry();
        PrimaryMousePosition=Geometry.LocalToAbsolute(Geometry.GetLocalSize()*.5F);
    }
    TSet<FKey> Buttons; if(bDown) Buttons.Add(EKeys::LeftMouseButton);
    const FPointerEvent Event(0u,0u,PrimaryMousePosition,PrimaryMousePosition,Buttons,EKeys::LeftMouseButton,0,FModifierKeysState());
    Slate.ProcessMouseMoveEvent(Event,false);
    if(bDown)
    {
        auto Target=Game->ProductWidget->PrimaryFocusTarget.Pin();
        auto Window=Target.IsValid()?Slate.FindWidgetWindow(Target.ToSharedRef()):nullptr;
        if(Window.IsValid()) {PrimaryMouseHeld=true;Slate.ProcessMouseButtonDownEvent(Window->GetNativeWindow(),Event);}
    }
    else {PrimaryMouseHeld=false;Slate.ProcessMouseButtonUpEvent(Event);}
}
void UAeroSimControlBridge::ReleaseKeys()
{
    for(const FKey& Key:HeldKeys) SendKey(Key,false);
    HeldKeys.Reset();
    if(PrimaryMouseHeld) SendPrimaryMouse(false);
}
void UAeroSimControlBridge::Complete(bool bValid,const FString& Error)
{
    if(!Pending.IsValid()) return;
    ReleaseKeys();
    const FString Operation=Pending->GetStringField(TEXT("operation"));
    const double SettledSeconds=FPlatformTime::Seconds()-SettleStart;
    RestoreReviewView();
    if(!bValid && Restore) Restore();
    auto Reply=MakeShared<FJsonObject>();
    Reply->SetStringField(TEXT("request_id"),Pending->GetStringField(TEXT("request_id")));
    Reply->SetStringField(TEXT("session"),Session);
    Reply->SetStringField(TEXT("status"),bValid?TEXT("COMMITTED"):TEXT("ROLLED_BACK"));
    Reply->SetStringField(TEXT("error"),Error);
    Reply->SetObjectField(TEXT("request"),Pending.ToSharedRef());
    Reply->SetStringField(TEXT("timestamp_utc"),FDateTime::UtcNow().ToIso8601());
    Reply->SetStringField(TEXT("transaction_id"),Pending->GetStringField(TEXT("request_id")));
    Reply->SetStringField(TEXT("behavior_verdict"),TEXT("REQUIRES_ASSERTION_ON_ACTUAL_STATE"));
    Reply->SetObjectField(TEXT("before"),Before.ToSharedRef());Reply->SetObjectField(TEXT("after"),Inspect());
    if(Pending->HasField(TEXT("mapped_peak"))) Reply->SetNumberField(TEXT("mapped_peak"),Pending->GetNumberField(TEXT("mapped_peak")));
    if(Pending->HasField(TEXT("semantic_peak"))) Reply->SetNumberField(TEXT("semantic_peak"),Pending->GetNumberField(TEXT("semantic_peak")));
    if(!PendingCaptureOutput.IsEmpty()) Reply->SetStringField(TEXT("evidence_png"),PendingCaptureOutput);
    if(Operation==TEXT("capture_view"))
    {
        Reply->SetStringField(TEXT("review_view"),Pending->GetStringField(TEXT("view")));
        Reply->SetStringField(TEXT("view_mode"),Pending->GetStringField(TEXT("view_mode")));
        Reply->SetNumberField(TEXT("settle_frames"),SettleFrames);
        Reply->SetNumberField(TEXT("settle_seconds"),SettledSeconds);
        Reply->SetNumberField(TEXT("streaming_wanting_at_capture"),WantingAtCapture);
        TArray<TSharedPtr<FJsonValue>> Frames;
        for(const FString& Output:SequenceOutputs) Frames.Add(MakeShared<FJsonValueString>(Output));
        Reply->SetArrayField(TEXT("evidence_pngs"),Frames);
        SequenceOutputs.Reset();
    }
    Respond(Reply);
    Pending.Reset();Before.Reset();Restore=nullptr;Deadline=0;PendingCaptureOutput.Reset();
}
void UAeroSimControlBridge::Respond(const TSharedRef<FJsonObject>& Reply,bool bCache)
{
    if(!Reply->HasField(TEXT("timestamp_utc")))Reply->SetStringField(TEXT("timestamp_utc"),FDateTime::UtcNow().ToIso8601());
    WriteJson(TEXT("response.json"),Reply);
    FString Audit;FJsonSerializer::Serialize(Reply,TJsonWriterFactory<>::Create(&Audit));Audit+=TEXT("\n");
    const FString AuditPath=Directory/TEXT("audit.jsonl");
    if(BridgeFiles().FileSize(*AuditPath)>2*1024*1024)ReplaceBridgeFile(AuditPath+TEXT(".previous"),AuditPath);
    WriteBridgeUtf8(AuditPath,Audit,true);
    if(bCache)
    {
        const FString Id=Reply->GetStringField(TEXT("request_id"));Replies.Add(Id,Reply);ReplyOrder.Add(Id);
        if(ReplyOrder.Num()>64){Replies.Remove(ReplyOrder[0]);ReplyOrder.RemoveAt(0);}
    }
}
void UAeroSimControlBridge::Dispatch(const TSharedRef<FJsonObject>& Request)
{
    FString Id,RequestSession,Operation,Mode,Target;
    if(!Request->TryGetStringField(TEXT("request_id"),Id) || Id.Len()<16 || Id.Len()>64) return;
    FString Canonical;FJsonSerializer::Serialize(Request,TJsonWriterFactory<>::Create(&Canonical));
    const FString Hash=ProductEvidenceSha256(Canonical);
    auto Error=[&](const TCHAR* Message)
    {
        auto Reply=MakeShared<FJsonObject>();Reply->SetStringField(TEXT("request_id"),Id);
        Reply->SetStringField(TEXT("status"),TEXT("REJECTED"));Reply->SetStringField(TEXT("error"),Message);Reply->SetObjectField(TEXT("request"),Request);Respond(Reply,false);
    };
    if(const FString* ExistingHash=RequestHashes.Find(Id))
    {
        if(*ExistingHash!=Hash){Error(TEXT("REQUEST_ID_REUSE"));return;}
        if(Id!=LastRequest) if(auto* Cached=Replies.Find(Id))WriteJson(TEXT("response.json"),Cached->ToSharedRef());
        LastRequest=Id;return;
    }
    LastRequest=Id;RequestHashes.Add(Id,Hash);
    if(RequestHashes.Num()>4096){Error(TEXT("SESSION_REQUEST_QUOTA"));return;}
    double Expires=0;
    const double Now=FDateTime::UtcNow().ToUnixTimestamp();
    if(!Request->TryGetStringField(TEXT("session"),RequestSession)||RequestSession!=Session){Error(TEXT("SESSION_MISMATCH"));return;}
    if(!Request->TryGetNumberField(TEXT("expires_unix"),Expires)||!FMath::IsFinite(Expires)||Expires<Now||Expires>Now+15){Error(TEXT("REQUEST_EXPIRED"));return;}
    if(!Request->TryGetStringField(TEXT("target"),Target)||Target!=TEXT("player:0")){Error(TEXT("TARGET_NOT_ALLOWED"));return;}
    if(!Request->TryGetStringField(TEXT("operation"),Operation)){Error(TEXT("OPERATION_REQUIRED"));return;}
    if(Operation==TEXT("inspect"))
    {
        auto Reply=MakeShared<FJsonObject>();Reply->SetStringField(TEXT("request_id"),Id);Reply->SetStringField(TEXT("status"),TEXT("OK"));
        Reply->SetObjectField(TEXT("state"),Inspect());Respond(Reply);return;
    }
    if(Operation!=TEXT("primary")&&Operation!=TEXT("back")&&Operation!=TEXT("key")&&Operation!=TEXT("capture")&&Operation!=TEXT("release")&&Operation!=TEXT("click_primary")&&Operation!=TEXT("capture_view"))
    {Error(TEXT("OPERATION_NOT_ALLOWED"));return;}
    FString View,ViewMode;
    if(Operation==TEXT("capture_view"))
    {
        EViewModeIndex ParsedMode;
        if(!Request->TryGetStringField(TEXT("view"),View)||!AllowedReviewView(View)){Error(TEXT("VIEW_NOT_ALLOWED"));return;}
        if(!Request->TryGetStringField(TEXT("view_mode"),ViewMode)||!ParseViewMode(ViewMode,ParsedMode)){Error(TEXT("VIEW_MODE_NOT_ALLOWED"));return;}
        double Frames=1,IntervalMs=250;
        Request->TryGetNumberField(TEXT("frames"),Frames);Request->TryGetNumberField(TEXT("interval_ms"),IntervalMs);
        if(!FMath::IsFinite(Frames)||Frames!=FMath::FloorToDouble(Frames)||Frames<1||Frames>8
            ||!FMath::IsFinite(IntervalMs)||IntervalMs<100||IntervalMs>2000){Error(TEXT("SEQUENCE_NOT_ALLOWED"));return;}
        SequenceFrames=static_cast<int32>(Frames);SequenceInterval=IntervalMs/1000.0;
    }
    Request->TryGetStringField(TEXT("mode"),Mode);
    if(Mode!=TEXT("preview")&&Mode!=TEXT("apply")){Error(TEXT("MODE_NOT_ALLOWED"));return;}
    auto* Game=Cast<AAeroSimGameMode>(GetOwner());
    auto* PC=GetWorld()->GetFirstPlayerController();auto* Pawn=PC?Cast<AFlightPawn>(PC->GetPawn()):nullptr;
    if(!Game||!Pawn){Error(TEXT("PLAYER_NOT_READY"));return;}
    FKey Key;double Duration=0;
    if(Operation==TEXT("key"))
    {
        FString KeyName;Request->TryGetStringField(TEXT("key"),KeyName);Key=AllowedKey(KeyName);
        if(!Key.IsValid()||!Request->TryGetNumberField(TEXT("duration_ms"),Duration)||!FMath::IsFinite(Duration)||Duration<50||Duration>2000)
        {Error(TEXT("KEY_OR_LEASE_NOT_ALLOWED"));return;}
    }
    if(Mode==TEXT("preview"))
    {
        auto Reply=MakeShared<FJsonObject>();Reply->SetStringField(TEXT("request_id"),Id);Reply->SetStringField(TEXT("status"),TEXT("PREVIEW"));
        Reply->SetObjectField(TEXT("state"),Inspect());Reply->SetStringField(TEXT("expected_page"),PageLabel(Game->GetProductPage()));
        Respond(Reply);return;
    }
    FString Expected;Request->TryGetStringField(TEXT("expected_page"),Expected);
    if(Expected!=PageLabel(Game->GetProductPage())){Error(TEXT("PAGE_CHANGED_REPREVIEW"));return;}
    const auto Model=Pawn->FlightModel;const auto Input=Pawn->ControlInput;const auto Transform=Pawn->GetActorTransform();
    const auto Mission=Game->Phase4Mission;const auto Flow=Game->MissionFlow;const auto Shell=Game->Shell;
    const auto Trace=Pawn->FlightTrace;const auto Elapsed=Pawn->FlightElapsedSeconds;const auto Accumulator=Pawn->TraceAccumulatorSeconds;
    const auto Camera=Pawn->CameraMode;const bool Paused=GetWorld()->IsPaused();const bool Resume=Pawn->bResumeRequested;const float Countdown=Pawn->ResumeCountdownSeconds;
    const bool WaterIncident=Pawn->bWaterIncidentLatched,CollisionHold=Pawn->bCollisionHold;
    Restore=[this,Pawn,Game,PC,Model,Input,Transform,Mission,Flow,Shell,Trace,Elapsed,Accumulator,Camera,Paused,Resume,Countdown,WaterIncident,CollisionHold]()
    {
        Pawn->FlightModel=Model;Pawn->ControlInput=Input;Pawn->SetActorTransform(Transform);
        Pawn->FlightTrace=Trace;Pawn->FlightElapsedSeconds=Elapsed;Pawn->TraceAccumulatorSeconds=Accumulator;
        Pawn->bWaterIncidentLatched=WaterIncident;Pawn->bCollisionHold=CollisionHold;
        Pawn->CameraMode=Camera;Pawn->ApplyCameraMode();Pawn->bResumeRequested=Resume;Pawn->ResumeCountdownSeconds=Countdown;
        Game->Phase4Mission=Mission;Game->MissionFlow=Flow;Game->Shell=Shell;
        PC->SetPause(Paused);Game->RefreshProductWidget();Game->ConfigureProductInputMode();
    };
    Pending=Request;Before=Inspect();Deadline=FPlatformTime::Seconds()+.15;
    if(Operation==TEXT("primary")) Game->ActivatePrimaryAction();
    else if(Operation==TEXT("back")) Game->HandleProductBack();
    else if(Operation==TEXT("release")) ReleaseKeys();
    else if(Operation==TEXT("click_primary"))
    {
        SendPrimaryMouse(true);
        if(!PrimaryMouseHeld){Complete(false,TEXT("PRIMARY_CONTROL_UNAVAILABLE"));return;}
    }
    else if(Operation==TEXT("capture"))
    {
        if(FScreenshotRequest::IsScreenshotRequested()){Complete(false,TEXT("CAPTURE_BUSY"));return;}
        Game->CaptureProductEvidence();
        if(!FScreenshotRequest::IsScreenshotRequested()){Complete(false,TEXT("CAPTURE_NOT_STARTED"));return;}
        PendingCaptureOutput=Game->LastProductEvidenceOutput;Deadline=FPlatformTime::Seconds()+5;
    }
    else if(Operation==TEXT("capture_view"))
    {
        if(FScreenshotRequest::IsScreenshotRequested()||Game->bTemporalCapture){Complete(false,TEXT("CAPTURE_BUSY"));return;}
        FString Failure;
        if(!BeginReviewView(Game,PC,View,ViewMode,Failure)){Complete(false,Failure);return;}
        SettleFrames=0;WantingAtCapture=0;SequenceOutputs.Reset();SettleStart=FPlatformTime::Seconds();
        Deadline=SettleStart+15+SequenceFrames*(SequenceInterval+1.0);
    }
    else if(Operation==TEXT("key"))
    {
        HeldKeys.Add(Key);SendKey(Key,true);Deadline=FPlatformTime::Seconds()+Duration/1000.0;
        Pending->SetNumberField(TEXT("mapped_peak"),0);Pending->SetNumberField(TEXT("semantic_peak"),0);
    }
}
void UAeroSimControlBridge::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta,Type,Function);
    if(!Enabled) return;
    const double Now=FPlatformTime::Seconds();
    if(Now>=Heartbeat)
    {
        auto Info=Inspect();Info->SetNumberField(TEXT("heartbeat_unix"),FDateTime::UtcNow().ToUnixTimestamp());
        WriteJson(TEXT("session.json"),Info);Heartbeat=Now+1;
    }
    if(Pending.IsValid())
    {
        auto State=Inspect();
        if(Pending->HasField(TEXT("mapped_peak")))
        {
            double Mapped=0,Semantic=0;
            const TCHAR* Axes[]={TEXT("pitch"),TEXT("roll"),TEXT("yaw"),TEXT("throttle")};
            for(const TCHAR* Axis:Axes)
            {
                double V=0;State->TryGetNumberField(FString(TEXT("mapped_"))+Axis,V);Mapped=FMath::Max(Mapped,FMath::Abs(V));
                State->TryGetNumberField(Axis,V);Semantic=FMath::Max(Semantic,FMath::Abs(V));
            }
            Pending->SetNumberField(TEXT("mapped_peak"),FMath::Max(Mapped,Pending->GetNumberField(TEXT("mapped_peak"))));
            Pending->SetNumberField(TEXT("semantic_peak"),FMath::Max(Semantic,Pending->GetNumberField(TEXT("semantic_peak"))));
        }
        const FString Operation=Pending->GetStringField(TEXT("operation"));
        const bool Capture=Operation==TEXT("capture")||Operation==TEXT("capture_view");
        auto* Game=Cast<AAeroSimGameMode>(GetOwner());
        if(Operation==TEXT("capture_view")&&Game)
        {
            if(SequenceOutputs.IsEmpty())
            {
                // Let TAA, Lumen and texture streaming settle after the camera cut; stop waiting on streaming after 6 s.
                ++SettleFrames;
                const double Settled=Now-SettleStart;
                const int32 Wanting=IStreamingManager::Get().GetNumWantingResources();
                if(SettleFrames>=30&&Settled>=1.0&&(Wanting==0||Settled>=6.0))
                {
                    WantingAtCapture=Wanting;Game->EvidenceSequenceStart=Now;
                    if(!StartReviewFrame(Game,Now)) return;
                }
            }
            else if(BridgeFiles().FileExists(*(SequenceOutputs.Last()+TEXT(".json"))))
            {
                if(SequenceOutputs.Num()>=SequenceFrames){Complete(true);return;}
                if(Now>=NextFrameTime&&!StartReviewFrame(Game,Now)) return;
            }
        }
        else if(Capture&&!PendingCaptureOutput.IsEmpty()&&BridgeFiles().FileExists(*(PendingCaptureOutput+TEXT(".json")))) {Complete(true);return;}
        if(Now>=Deadline)
        {
            const bool Finite=FMath::IsFinite(State->GetNumberField(TEXT("position_cm_x")))&&FMath::IsFinite(State->GetNumberField(TEXT("position_cm_z")));
            Complete(!Capture&&Finite,Capture?TEXT("CAPTURE_TIMEOUT"):(!Finite?TEXT("NONFINITE_STATE"):TEXT("")));
        }
        return;
    }
    const FString Path=Directory/TEXT("request.json");
    const int64 Size=BridgeFiles().FileSize(*Path);
    if(Size<=0||Size>8192) return;
    FString Text;if(!FFileHelper::LoadFileToString(Text,*Path))return;
    TSharedPtr<FJsonObject> Request;
    if(FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Request)&&Request.IsValid())Dispatch(Request.ToSharedRef());
}
