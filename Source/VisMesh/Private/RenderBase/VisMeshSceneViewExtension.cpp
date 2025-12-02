#include "RenderBase/VisMeshSceneViewExtension.h"

#include "RenderBase/VisMeshComponentBase.h"
#include "RenderBase/VisMeshSceneProxyBase.h"
#include "RenderBase/VisMeshSubsystem.h"


FVisMeshSceneViewExtension::FVisMeshSceneViewExtension(const FAutoRegister& AutoReg, UWorld* InWorld,UVisMeshSubsystem* System)
	: FWorldSceneViewExtension(AutoReg, InWorld)
	  , OwnerSystem(System)
{
}

void FVisMeshSceneViewExtension::Invalidate()
{
	OwnerSystem = nullptr;
}

void FVisMeshSceneViewExtension::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	if (!IsValid(OwnerSystem)) return;

	FScopeLock Lock(&OwnerSystem->ComponentsLock);
	const auto& Components = OwnerSystem->GetRegisteredComponents();

	for (const TWeakObjectPtr<UVisMeshComponentBase>& WeakComp : Components)
	{
		UVisMeshComponentBase* Comp = WeakComp.Get();
		
		if (Comp && Comp->SceneProxy)
		{
			// 这里我们假设：注册到 OwnerSystem 的组件，其 Proxy 一定是 FVisMeshSceneProxyBase 的子类
			FVisMeshSceneProxyBase* ComputeProxy = static_cast<FVisMeshSceneProxyBase*>(Comp->SceneProxy);
            
			if (ComputeProxy)
			{
				ComputeProxy->DispatchComputePass_RenderThread(GraphBuilder, InViewFamily);
			}
		}
	}
}
