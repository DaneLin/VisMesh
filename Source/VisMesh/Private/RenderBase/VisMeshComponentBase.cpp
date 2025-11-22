#include "RenderBase/VisMeshComponentBase.h"

#include "RenderBase/VisMeshSubsystem.h"

UVisMeshComponentBase::UVisMeshComponentBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UVisMeshComponentBase::OnRegister()
{
	Super::OnRegister();

	// 1. 过滤掉模板对象(CDO)，只注册实例
	if (IsTemplate())
	{
		return;
	}

	// 2. 获取 World 并注册
	if (UWorld* World = GetWorld())
	{
		if (UVisMeshSubsystem* Subsystem = World->GetSubsystem<UVisMeshSubsystem>())
		{
			// 此时 Subsystem 接收的是 UVisMeshComponentBase* 指针
			Subsystem->RegisterComponent(this);
		}
	}
}

void UVisMeshComponentBase::OnUnregister()
{
	// 1. 获取 World 并注销
	if (UWorld* World = GetWorld())
	{
		// 检查 Subsystem 是否还存在（防止 World 析构时崩溃）
		if (UVisMeshSubsystem* Subsystem = World->GetSubsystem<UVisMeshSubsystem>())
		{
			Subsystem->UnregisterComponent(this);
		}
	}

	Super::OnUnregister(); // 必须调用
}