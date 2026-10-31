#include "Player/DTHero.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTPaletteGen.h"
#include "Dusktracer.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Materials/MaterialInterface.h"
#include "Player/DTHeroSubsystem.h"
#include "Settings/DTMenuSubsystem.h"
#include "Settings/DTUserSettings.h"

namespace DTHeroLocal
{
	// Asset paths (not tunables); same instance-colour material as the enemies (custom data 0-2 RGB, 3 emissive).
	static const TCHAR* MeshPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	static const TCHAR* MaterialPath = TEXT("/Game/DT/Materials/M_InstanceColor.M_InstanceColor");
	constexpr int32 NumCustomFloats = 4;
	// Grey-box proportions (presentation only, no gameplay): the facing block is a small puck at the body's rim.
	constexpr float FacingScale = 0.4f;
	// The armour is lit by the scene, not self-lit.
	constexpr float BodyEmissive = 0.0f;
	constexpr int32 BodyInstance = 0;
	constexpr int32 FacingInstance = 1;

	static float BodyDiameter() { return 2.0f * static_cast<float>(DTConst::HERO_RADIUS_UU); }
	// The body stands on the ground and is centred on the sim plane, so its height is twice the plane height.
	static float BodyHeight() { return 2.0f * static_cast<float>(DTConst::SIM_PLANE_Z_UU); }
}

ADTHero::ADTHero()
{
	PrimaryActorTick.bCanEverTick = true;
	SetCanBeDamaged(false);
	bReplicates = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	AutoPossessAI = EAutoPossessAI::Disabled;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Body = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetMobility(EComponentMobility::Movable);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetGenerateOverlapEvents(false);
	Body->SetCanEverAffectNavigation(false);
	Body->NumCustomDataFloats = DTHeroLocal::NumCustomFloats;
}

UDTHeroSubsystem* ADTHero::GetHeroSubsystem() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UDTHeroSubsystem>() : nullptr;
}

FVector2f ADTHero::ScreenToPlane(const FVector2D& Screen)
{
	return FVector2f(static_cast<float>(Screen.Y), static_cast<float>(Screen.X));
}

void ADTHero::BeginPlay()
{
	Super::BeginPlay();

	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, DTHeroLocal::MeshPath);
	if (Mesh)
	{
		Body->SetStaticMesh(Mesh);
		// M_InstanceColor ships with the arena (S0); without it the mesh keeps its own material and the hero draws untinted.
		if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, DTHeroLocal::MaterialPath))
		{
			Body->SetMaterial(0, Material);
		}
		const FVector MeshSize = Mesh->GetBounds().BoxExtent * 2.0;
		const float Diameter = DTHeroLocal::BodyDiameter();
		const float Height = DTHeroLocal::BodyHeight();
		const FVector BodyScale(Diameter / MeshSize.X, Diameter / MeshSize.Y, Height / MeshSize.Z);
		const FVector Centre(0.0, 0.0, static_cast<double>(DTConst::SIM_PLANE_Z_UU));
		Body->ClearInstances();
		Body->AddInstance(FTransform(FRotator::ZeroRotator, Centre, BodyScale), false);
		Body->AddInstance(FTransform(FRotator::ZeroRotator, Centre + FVector(Diameter * 0.5, 0.0, 0.0), BodyScale * DTHeroLocal::FacingScale), false);
		const FLinearColor Armour = DTPal::Linear(DTPal::Hero_Armour);
		const FLinearColor Facing = DTPal::Linear(DTPal::Player_Core);
		Body->SetCustomData(DTHeroLocal::BodyInstance, { Armour.R, Armour.G, Armour.B, DTHeroLocal::BodyEmissive }, false);
		Body->SetCustomData(DTHeroLocal::FacingInstance, { Facing.R, Facing.G, Facing.B, DTHeroLocal::BodyEmissive }, true);
		bBodyReady = true;
	}
	else
	{
		UE_LOG(LogDT, Warning, TEXT("Hero mesh %s missing; the hero will not render"), DTHeroLocal::MeshPath);
	}

	if (UDTHeroSubsystem* HeroSystem = GetHeroSubsystem())
	{
		HeroSystem->RegisterPawn(this);
		if (const FDTHeroState* State = HeroSystem->GetState(PlayerIndex))
		{
			DrawFromSim(*State, 0.0f);
		}
	}
}

void ADTHero::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UDTHeroSubsystem* HeroSystem = GetHeroSubsystem())
	{
		HeroSystem->UnregisterPawn(this);
	}
	Super::EndPlay(EndPlayReason);
}

void ADTHero::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	const APlayerController* PC = Cast<APlayerController>(NewController);
	const UGameInstance* GameInstance = GetGameInstance();
	if (PC && PC->GetLocalPlayer() && GameInstance)
	{
		const int32 Index = GameInstance->GetLocalPlayers().IndexOfByKey(PC->GetLocalPlayer());
		if (Index != INDEX_NONE)
		{
			PlayerIndex = Index;
		}
	}
}

void ADTHero::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		UE_LOG(LogDT, Error, TEXT("ADTHero: the input component is not an UEnhancedInputComponent (Config/DefaultInput.ini); no hero input"));
		return;
	}
	if (!InputSet)
	{
		InputSet = UDTInputSet::Create(this);
	}
	Input->BindActionValue(InputSet->Move);
	Input->BindActionValue(InputSet->Aim);
	Input->BindAction(InputSet->Dash, ETriggerEvent::Started, this, &ADTHero::OnDashStarted);
	Input->BindAction(InputSet->Intercession, ETriggerEvent::Started, this, &ADTHero::OnIntercessionStarted);
	Input->BindAction(InputSet->Pause, ETriggerEvent::Started, this, &ADTHero::OnPauseStarted);
}

void ADTHero::PawnClientRestart()
{
	Super::PawnClientRestart();
	AddMappingContexts();
}

void ADTHero::UnPossessed()
{
	RemoveMappingContexts();
	Super::UnPossessed();
}

void ADTHero::AddMappingContexts()
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	UEnhancedInputLocalPlayerSubsystem* Subsystem = PC ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
	if (!Subsystem || !InputSet)
	{
		return;
	}
	// The local player outlives a map reload and would keep the last hero's contexts (settings.md R-SET-24).
	Subsystem->ClearAllMappings();
	// Both devices at the same priority: either works at any time (R-CMB-02).
	for (const UInputMappingContext* Context : InputSet->GetContexts())
	{
		Subsystem->AddMappingContext(Context, 0);
	}
	UE_LOG(LogDT, Log, TEXT("Hero input: %s + %s active for player %d"), *InputSet->Kbm->GetName(), *InputSet->Pad->GetName(), PlayerIndex);
}

void ADTHero::RemoveMappingContexts()
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	UEnhancedInputLocalPlayerSubsystem* Subsystem = PC ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
	if (!Subsystem || !InputSet)
	{
		return;
	}
	for (const UInputMappingContext* Context : InputSet->GetContexts())
	{
		Subsystem->RemoveMappingContext(Context);
	}
}

void ADTHero::OnDashStarted(const FInputActionValue& Value)
{
	Frame.bDashPress = TakesPress(GetWorld()->IsPaused()) || Frame.bDashPress;
}

void ADTHero::OnIntercessionStarted(const FInputActionValue& Value)
{
	Frame.bIntercessionPress = TakesPress(GetWorld()->IsPaused()) || Frame.bIntercessionPress;
}

void ADTHero::OnPauseStarted(const FInputActionValue& Value)
{
	// The screen opens over anything, the run-start hand included (settings.md R-SET-16); it closes itself.
	if (UDTMenuSubsystem* Menu = GetWorld() ? GetWorld()->GetSubsystem<UDTMenuSubsystem>() : nullptr)
	{
		Menu->RequestPause();
	}
}

void ADTHero::RefreshBindings()
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	UEnhancedInputLocalPlayerSubsystem* Subsystem = PC ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
	if (Subsystem && InputSet)
	{
		InputSet->ApplyBindings(DTSettings::Live().Bindings);
		Subsystem->RequestRebuildControlMappings();
	}
}

void ADTHero::SampleFrame()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	const UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	if (!PC || !Input || !InputSet)
	{
		return;
	}
	const FVector2D MoveScreen = Input->GetBoundActionValue(InputSet->Move).Get<FVector2D>();
	const FVector2D AimScreen = Input->GetBoundActionValue(InputSet->Aim).Get<FVector2D>();
	Frame.Move = ScreenToPlane(MoveScreen);

	// IA_Aim carries mouse deltas on KBM and the right stick on pad; the raw pad stick tells them apart.
	const FVector2D PadRight(PC->GetInputAnalogKeyState(EKeys::Gamepad_RightX), PC->GetInputAnalogKeyState(EKeys::Gamepad_RightY));
	const FVector2D PadLeft(PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftX), PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftY));
	FVector2D MousePos = LastMousePos;
	const bool bHasMouse = PC->GetMousePosition(MousePos.X, MousePos.Y);
	const bool bMouseMoved = bHasMouse && !MousePos.Equals(LastMousePos);
	LastMousePos = MousePos;

	const bool bPadStick = !PadRight.IsNearlyZero() || !PadLeft.IsNearlyZero();
	if (!PadRight.IsNearlyZero())
	{
		bMouseAim = false;
		Frame.AimStick = ScreenToPlane(AimScreen);
	}
	else
	{
		Frame.AimStick = FVector2f::ZeroVector;
		if (bMouseMoved || !AimScreen.IsNearlyZero())
		{
			bMouseAim = true;
		}
	}
	if (bPadStick)
	{
		Frame.Device = EDTInputDevice::Pad;
	}
	else if (bMouseMoved || !MoveScreen.IsNearlyZero())
	{
		Frame.Device = EDTInputDevice::KBM;
	}

	// Cursor projected onto the sim plane (R-CMB-04).
	Frame.bAimCursorValid = false;
	FVector RayOrigin;
	FVector RayDir;
	if (bMouseAim && bHasMouse && PC->DeprojectMousePositionToWorld(RayOrigin, RayDir) && RayDir.Z < -UE_KINDA_SMALL_NUMBER)
	{
		const double T = (static_cast<double>(DTConst::SIM_PLANE_Z_UU) - RayOrigin.Z) / RayDir.Z;
		if (T > 0.0)
		{
			const FVector Hit = RayOrigin + RayDir * T;
			Frame.AimCursorWorld = FVector2f(static_cast<float>(Hit.X), static_cast<float>(Hit.Y));
			Frame.bAimCursorValid = true;
		}
	}

	if (const UDTHeroSubsystem* HeroSystem = GetHeroSubsystem())
	{
		PC->SetShowMouseCursor(HeroSystem->GetAimMode(PlayerIndex) == EDTAimMode::Manual && bMouseAim);
	}
}

void ADTHero::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsLocallyControlled() || !Cast<APlayerController>(GetController()))
	{
		return;
	}
	SampleFrame();
	if (UDTHeroSubsystem* HeroSystem = GetHeroSubsystem())
	{
		HeroSystem->LatchInput(PlayerIndex, Frame);
	}
	Frame.bDashPress = false;
	Frame.bIntercessionPress = false;
}

void ADTHero::DrawFromSim(const FDTHeroState& State, float Alpha)
{
	const FVector2f Pos = FMath::Lerp(State.PrevPos, State.Pos, Alpha);
	SetActorLocation(FVector(Pos.X, Pos.Y, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
	if (!bBodyReady)
	{
		return;
	}
	const FVector Aim(State.AimDir.X, State.AimDir.Y, 0.0f);
	const FVector Centre(0.0, 0.0, static_cast<double>(DTConst::SIM_PLANE_Z_UU));
	FTransform Facing;
	Body->GetInstanceTransform(DTHeroLocal::FacingInstance, Facing, false);
	Facing.SetLocation(Centre + Aim * (DTHeroLocal::BodyDiameter() * 0.5));
	Body->UpdateInstanceTransform(DTHeroLocal::FacingInstance, Facing, false, true, true);
}
