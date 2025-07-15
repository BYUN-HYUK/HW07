#include "MyPawn.h"
#include "MyPlayerController.h" // 플레이어 컨트롤러 접근을 위해 필요
#include "Components/CapsuleComponent.h" // 폰의 충돌 및 루트 컴포넌트
#include "Components/SkeletalMeshComponent.h" // 폰의 시각적 메시
#include "GameFramework/SpringArmComponent.h" // 카메라를 폰 뒤에 유지
#include "Camera/CameraComponent.h" // 폰의 시점 카메라
#include "GameFramework/FloatingPawnMovement.h" // 폰의 이동 로직
#include "EnhancedInputComponent.h" // Enhanced Input 시스템의 바인딩
#include "EnhancedInputSubsystems.h" // Enhanced Input 서브시스템 접근
#include "GameFramework/Controller.h" // 폰의 컨트롤러 접근
#include "InputMappingContext.h" // 입력 매핑 컨텍스트

// 이 값을 변경하여 카메라의 최대/최소 상하 각도를 조절
namespace PawnCameraConstants
{
	const float MaxCameraPitchUp = 80.0f;   // 카메라가 위로 볼 수 있는 최대 각도
	const float MaxCameraPitchDown = -80.0f; // 카메라가 아래로 볼 수 있는 최대 각도
}

// Sets default values
AMyPawn::AMyPawn()
{
	PrimaryActorTick.bCanEverTick = false;

	// 충돌 및 루트 컴포넌트 설정
	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("PawnCapsule"));
	RootComponent = Capsule;

	// 메시 설정
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PawnMesh"));
	Mesh->SetupAttachment(RootComponent);

	// 카메라를 폰 뒤에 유지하고 부드러운 움직임을 제공하는 스프링암 설정
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraSpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 300.0f; // 카메라와 폰 사이의 거리

	// 폰의 Yaw 회전과 스프링암의 Pitch 회전을 제어
	SpringArm->bUsePawnControlRotation = false;

	// 폰의 시점 카메라 설정
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));

	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	// 카메라의 Pitch는 스프링암을 통해, Yaw는 폰의 회전을 통해 제어됩니다.
	Camera->bUsePawnControlRotation = false;

	// 이는 마우스 입력으로 폰의 회전을 직접 계산하고 적용하기 위함입니다.
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	// 폰의 기본적인 이동 로직을 제공하는 컴포넌트
	FloatingPawnMovement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("PawnMovementComponent"));
}

// Called when the game starts or when spawned
void AMyPawn::BeginPlay()
{
	Super::BeginPlay();
}

// Called to bind functionality to input
void AMyPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Enhanced Input 시스템을 사용하기 위해 PlayerInputComponent를 EnhancedInputComponent로 캐스팅
	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// 플레이어 컨트롤러에 정의된 Input Action 에셋에 접근할 수 있음
		if (AMyPlayerController* MyController = Cast<AMyPlayerController>(GetController()))
		{
			// 이동 Input Action이 유효한 경우, Move 함수에 바인딩합니다.
			if (MyController->MoveAction)
			{
				EnhancedInput->BindAction(MyController->MoveAction, ETriggerEvent::Triggered, this, &AMyPawn::Move);
			}

			// 시점 회전 Input Action이 유효한 경우, Look 함수에 바인딩합니다.
			if (MyController->LookAction)
			{
				EnhancedInput->BindAction(MyController->LookAction, ETriggerEvent::Triggered, this, &AMyPawn::Look);
			}
		}
	}
}

void AMyPawn::Move(const FInputActionValue& Value)
{
	// Look 함수가 폰의 회전을 직접 제어하므로, 이동 방향은 폰이 현재 바라보는 방향을 기준으로 합니다.
	const FVector2D MovementVector = Value.Get<FVector2D>();

	// 전/후진 이동: 폰의 정면 방향으로 입력 값(Y축)만큼 이동합니다.
	AddMovementInput(GetActorForwardVector(), MovementVector.Y);
	// 좌/우 이동: 폰의 오른쪽 방향으로 입력 값(X축)만큼 이동합니다.
	AddMovementInput(GetActorRightVector(), MovementVector.X);
}

void AMyPawn::Look(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();

	// --- Yaw (좌우) 회전: 폰 전체를 회전시킵니다. ---
	// 마우스 X축 입력만큼 폰의 Yaw(좌우) 회전을 추가합니다.
	AddActorLocalRotation(FRotator(0.0f, LookAxisVector.X, 0.0f));

	// --- Pitch (상하) 회전: 스프링암만 회전시킵니다. ---
	// 스프링암이 유효한 경우에만 처리합니다.
	if (SpringArm)
	{
		// 현재 스프링암의 상대 회전 값을 가져옵니다.
		FRotator NewRotation = SpringArm->GetRelativeRotation();
		// 마우스 Y축 입력만큼 Pitch 값을 변경합니다.
		NewRotation.Pitch += LookAxisVector.Y;

		// 카메라가 땅을 뚫고 보거나 하늘로 뒤집히는 현상을 방지하기 위해 Pitch 각도를 제한합니다.
		NewRotation.Pitch = FMath::Clamp(NewRotation.Pitch, PawnCameraConstants::MaxCameraPitchDown, PawnCameraConstants::MaxCameraPitchUp);
		
		// 새로 계산된 회전 값을 스프링암에 적용합니다.
		SpringArm->SetRelativeRotation(NewRotation);
	}
}