#pragma once

#include "VisMeshSubsystem.generated.h"

class UVisMeshComponentBase;
class FVisMeshSceneViewExtension;

UCLASS()
class UVisMeshSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	TSharedPtr<FVisMeshSceneViewExtension, ESPMode::ThreadSafe> SceneViewExtension;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void RegisterComponent(UVisMeshComponentBase* Component);
	void UnregisterComponent(UVisMeshComponentBase* Component);

	// 提供给 SVE 访问的列表 (注意：SVE需要处理线程安全)
	const TArray<TWeakObjectPtr<UVisMeshComponentBase>>& GetRegisteredComponents() const { return RegisteredComponents; }
	
	// 锁：用于保护 RegisteredComponents，因为 GT 写，RT 读
	mutable FCriticalSection ComponentsLock;

private:
	// 使用 WeakPtr 防止悬垂指针，万一 Component 被 GC 了
	TArray<TWeakObjectPtr<UVisMeshComponentBase>> RegisteredComponents;
};