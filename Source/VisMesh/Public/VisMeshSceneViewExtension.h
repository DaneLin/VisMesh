#pragma once
#include "SceneViewExtension.h"

class UVisMeshSubsystem;

class FIndirectPopulateSceneViewExtension : public FWorldSceneViewExtension
{
public:
	FIndirectPopulateSceneViewExtension(const FAutoRegister& AutoReg, UWorld* InWorld, UVisMeshSubsystem* System);
	void Invalidate();

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}

	//~ Begin ISceneViewExtension interface
	virtual void  PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;
	//~ End ISceneViewExtension interface

public:
	UVisMeshSubsystem* OwnerSystem;
};
