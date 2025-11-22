#include "VisMeshSceneViewExtension.h"

#include "VisMeshComponentBase.h"
#include "VisMeshSceneProxy.h"
#include "VisMeshSubsystem.h"


FIndirectPopulateSceneViewExtension::FIndirectPopulateSceneViewExtension(const FAutoRegister& AutoReg, UWorld* InWorld,UVisMeshSubsystem* System)
	: FWorldSceneViewExtension(AutoReg, InWorld)
	  , OwnerSystem(System)
{
}

void FIndirectPopulateSceneViewExtension::Invalidate()
{
	OwnerSystem = nullptr;
}

void FIndirectPopulateSceneViewExtension::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	if (!IsValid(OwnerSystem)) return;

	FScopeLock Lock(&OwnerSystem->ComponentsLock);
	const auto& Components = OwnerSystem->GetRegisteredComponents();

	for (const TWeakObjectPtr<UVisMeshComponentBase>& WeakComp : Components)
	{
		UVisMeshComponentBase* Comp = WeakComp.Get();
		
		if (Comp && Comp->SceneProxy)
		{
			// 这里我们假设：注册到 OwnerSystem 的组件，其 Proxy 一定是 FVisMeshBaseSceneProxy 的子类
			FVisMeshBaseSceneProxy* ComputeProxy = static_cast<FVisMeshBaseSceneProxy*>(Comp->SceneProxy);
            
			if (ComputeProxy)
			{
				ComputeProxy->DispatchComputePass_RenderThread(GraphBuilder, InViewFamily);
			}
		}
	}
}
