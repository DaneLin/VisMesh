#include "VisMeshSubsystem.h"

#include "VisMeshSceneViewExtension.h"

void UVisMeshSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	SceneViewExtension = FSceneViewExtensions::NewExtension<FIndirectPopulateSceneViewExtension>(GetWorld(), this);
	Super::Initialize(Collection);
}

void UVisMeshSubsystem::Deinitialize()
{
	if (SceneViewExtension)
	{
		// Prevent this SVE from being gathered, in case it is kept alive by a strong reference somewhere else.
		{
			SceneViewExtension->IsActiveThisFrameFunctions.Empty();

			FSceneViewExtensionIsActiveFunctor IsActiveFunctor;

			IsActiveFunctor.IsActiveFunction = [](const ISceneViewExtension* SceneViewExtension, const FSceneViewExtensionContext& Context)
			{
				return TOptional<bool>(false);
			};

			SceneViewExtension->IsActiveThisFrameFunctions.Add(IsActiveFunctor);
		}

		ENQUEUE_RENDER_COMMAND(ReleaseSVE)([this](FRHICommandListImmediate& RHICmdList)
			{
				// Prevent this SVE from being gathered, in case it is kept alive by a strong reference somewhere else.
				{
					SceneViewExtension->IsActiveThisFrameFunctions.Empty();

					FSceneViewExtensionIsActiveFunctor IsActiveFunctor;

					IsActiveFunctor.IsActiveFunction = [](const ISceneViewExtension* SceneViewExtension, const FSceneViewExtensionContext& Context)
						{
							return TOptional<bool>(false);
						};

					SceneViewExtension->IsActiveThisFrameFunctions.Add(IsActiveFunctor);
				}

				SceneViewExtension->Invalidate();
				SceneViewExtension.Reset();
				SceneViewExtension = nullptr;
			});
	}

	// Finish all rendering commands first
	FlushRenderingCommands();

	FScopeLock Lock(&ComponentsLock);
	RegisteredComponents.Empty();
    
	Super::Deinitialize();
}

void UVisMeshSubsystem::RegisterComponent(UVisMeshComponentBase* Component)
{
	if (Component)
	{
		FScopeLock Lock(&ComponentsLock);
		RegisteredComponents.AddUnique(Component);
	}
}

void UVisMeshSubsystem::UnregisterComponent(UVisMeshComponentBase* Component)
{
	if (Component)
	{
		FScopeLock Lock(&ComponentsLock);
		RegisteredComponents.Remove(Component);
	}
}
