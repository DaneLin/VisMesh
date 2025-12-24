// Fill out your copyright notice in the Description page of Project Settings.


#include "VisCharts/VisBarChart.h"

#include "Components/VisMeshProceduralComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Utils/KismetVisMeshLibrary.h"

// Sets default values
AVisBarChart::AVisBarChart()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// 1. 主大网格
	MainMeshComponent = CreateDefaultSubobject<UVisMeshProceduralComponent>(TEXT("MainMesh"));
	RootComponent = MainMeshComponent;
	// [关键优化] 关闭主网格碰撞！10万个多边形的复杂碰撞是帧率杀手
	MainMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 2. 高亮游标网格
	HighlightMeshComponent = CreateDefaultSubobject<UVisMeshProceduralComponent>(TEXT("HighlightMesh"));
	HighlightMeshComponent->SetupAttachment(RootComponent);
	// 高亮组件也不需要碰撞
	HighlightMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HighlightMeshComponent->SetVisibility(false); // 初始隐藏

	SelectionMeshComponent = CreateDefaultSubobject<UVisMeshProceduralComponent>(TEXT("SelectionMesh"));
	HighlightMeshComponent->SetupAttachment(RootComponent);
}

// Called when the game starts or when spawned
void AVisBarChart::BeginPlay()
{
	Super::BeginPlay();

	// 测试数据
	// --- 1. 生成 100,000 个随机测试数据 ---
	const int32 NumData = NumInstances;
	TArray<float> TestData;
	TestData.SetNumUninitialized(NumData);

	// 并行填充随机数据
	ParallelFor(NumData, [&](int32 i) {
		TestData[i] = FMath::RandRange(10.f, 100.f);
	});

	// 自动设置列数，使其排列成近似正方形 (sqrt(100000) ≈ 316)
	GridColumnCount = FMath::CeilToInt(FMath::Sqrt((float)NumData));

	// 2. 预先为高亮组件生成一个“标准柱子”的 Mesh (Index 0)
	// 这样 Tick 里只需要改 Transform，不需要改 MeshData
	{
		TArray<FVector> BoxVerts;
		TArray<int32> BoxTris; 
		TArray<FVector> BoxNormals;
		TArray<FVector2D> BoxUVs;
		TArray<FVisMeshTangent> BoxTangents;
		UKismetVisMeshLibrary::GenerateBoxMesh(FVector(0.5f), BoxVerts, BoxTris, BoxNormals, BoxUVs, BoxTangents);
        
		// 给它一个显眼的高亮颜色
		TArray<FColor> HighColors;
		HighColors.Init(FColor::Yellow, BoxVerts.Num());

		HighlightMeshComponent->CreateMeshSection(0, BoxVerts, BoxTris, BoxNormals, BoxUVs, BoxUVs, BoxUVs, BoxUVs, HighColors, BoxTangents, false);

		if (HighlightMeshMaterial)
		{
			HighlightMeshComponent->SetMaterial(0, HighlightMeshMaterial);
		}
	}


	GenerateBarChart(TestData);

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->bShowMouseCursor = true;
		PC->bEnableClickEvents = true;
		PC->bEnableMouseOverEvents = true;

		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);
	}
	
	if (MainMeshMaterial)
	{
		MainMeshComponent->SetMaterial(0, MainMeshMaterial);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("请在蓝图中设置 MainMeshMaterial！"));
	}

	// 2. 初始化 MPC 参数 (防止参数残留)
	if (ChartMPC)
	{
		// 确保一开始遮挡效果是关闭的
		UKismetMaterialLibrary::SetScalarParameterValue(this, ChartMPC, FName("IsSelectionActive"), 0.0f);
		// 重置目标位置到极远处
		UKismetMaterialLibrary::SetVectorParameterValue(this, ChartMPC, FName("TargetPosition"), FLinearColor(0, 0, -10000));
		// 重置半径 (可选)
		UKismetMaterialLibrary::SetScalarParameterValue(this, ChartMPC, FName("CutoutRadius"), 2000);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ChartMPC 未设置！无法使用遮挡交互。"));
	}

	{
		TArray<FVector> BoxVerts;
		TArray<int32> BoxTris;
		TArray<FVector> BoxNormals;
		TArray<FVector2D> BoxUVs;
		TArray<FVisMeshTangent> BoxTangents;
		UKismetVisMeshLibrary::GenerateBoxMesh(FVector(0.5f), BoxVerts, BoxTris, BoxNormals, BoxUVs, BoxTangents);
        
		SelectionMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SelectionMeshComponent->SetVisibility(false);

		if (SelectionMeshMaterial)
		{
			SelectionMeshComponent->SetMaterial(0, SelectionMeshMaterial);
		}
	}
}

// Called every frame
void AVisBarChart::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;

	float MouseX, MouseY;
	if (PC->GetMousePosition(MouseX, MouseY))
	{
		FVector WorldLoc, WorldDir;
		if (PC->DeprojectScreenPositionToWorld(MouseX, MouseY, WorldLoc, WorldDir))
		{
			FTransform ActorTrans = GetTransform();
			FVector LocalStart = ActorTrans.InverseTransformPosition(WorldLoc);
			FVector LocalDir = ActorTrans.InverseTransformVectorNoScale(WorldDir);
			
			// 归一化方向（InverseTransformVectorNoScale 不保证归一化）
			LocalDir.Normalize();

			// --- 执行 Raycast ---
			int32 HoverIndex = RaycastOnBarChart(LocalStart, LocalDir);

			if (HoverIndex != -1)
			{
				// 状态更新逻辑 (保持不变)
				if (HoverIndex != LastHoverIndex)
				{
					int32 Row = HoverIndex / GridColumnCount;
					int32 Col = HoverIndex % GridColumnCount;
					
					float XPos = Col * (BarWidth + BarGap);
					float YPos = Row * (BarWidth + BarGap);
					
					float RawHeight = CachedDataValues[HoverIndex] * HeightMultiplier;
					float FinalHeight = RawHeight * HoverScale;

					HighlightMeshComponent->SetRelativeLocation(FVector(XPos, YPos, FinalHeight * 0.5f));
					float BiasScale = 1.01f; // 放大 1%
					HighlightMeshComponent->SetRelativeScale3D(FVector(BarWidth * BiasScale, BarWidth * BiasScale, FinalHeight));
					if (!HighlightMeshComponent->IsVisible())
					{
						HighlightMeshComponent->SetVisibility(true);
					}

					LastHoverIndex = HoverIndex;
				}
			}
			else
			{
				HighlightMeshComponent->SetVisibility(false);
				LastHoverIndex = -1;
			}
		}
	}

	HandleClick();
	
}

void AVisBarChart::GenerateBarChart(const TArray<float>& DataValues)
{
	if (DataValues.Num() == 0) return;

	// 1. 缓存数据
	CachedDataValues = DataValues;
	if (GridColumnCount <= 0) GridColumnCount = 1;

	// 2. 准备模板 (1x1x1 盒子)
	TArray<int32> TemplateTris;
	TArray<FVector> TemplateNormals;
	TArray<FVector2D> TemplateUVs;
	TArray<FVisMeshTangent> TemplateTangents;
	UKismetVisMeshLibrary::GenerateBoxMesh(FVector(0.5f), TemplateVerts, TemplateTris, TemplateNormals, TemplateUVs, TemplateTangents);

	// 3. 预分配内存 (10万数据 * 24顶点 = 240万顶点)
	int32 NumBars = DataValues.Num();
	int32 VertsPerBar = TemplateVerts.Num();
	int32 TotalVerts = NumBars * VertsPerBar;
	int32 TotalTris = NumBars * TemplateTris.Num();

	FVisMeshData MeshData;
	MeshData.Positions.SetNumUninitialized(TotalVerts);
	MeshData.Normals.SetNumUninitialized(TotalVerts);
	MeshData.Colors.SetNumUninitialized(TotalVerts);
	MeshData.UV0.SetNumUninitialized(TotalVerts);
	MeshData.Triangles.Reserve(TotalTris);

	// 4. [关键优化] 并行计算几何体
	ParallelFor(NumBars, [&](int32 i)
	{
		// 计算网格行列
		int32 Row = i / GridColumnCount;
		int32 Col = i % GridColumnCount;

		// 计算变换
		float Height = DataValues[i] * HeightMultiplier;
		float XPos = Col * (BarWidth + BarGap);
		float YPos = Row * (BarWidth + BarGap); // Y轴向下延伸
		
		FVector Location(XPos, YPos, Height * 0.5f);
		FVector Scale(BarWidth, BarWidth, Height);

		int32 BaseVertIdx = i * VertsPerBar;

		// 填充该柱子的所有顶点
		for (int32 v = 0; v < VertsPerBar; v++)
		{
			// 变换顶点位置
			MeshData.Positions[BaseVertIdx + v] = Location + (TemplateVerts[v] * Scale);
			
			// 复制属性
			MeshData.Normals[BaseVertIdx + v] = TemplateNormals[v];
			MeshData.UV0[BaseVertIdx + v] = TemplateUVs[v];

			// 计算颜色 (根据 Z 轴判断是顶面还是侧面)
			FColor C = (TemplateVerts[v].Z > 0.0f) ? TopColor.ToFColor(true) : BaseColor.ToFColor(true);
			MeshData.Colors[BaseVertIdx + v] = C;
		}
	});

	// 5. 填充索引 (TArray Add 非线程安全，需串行，但这是纯拷贝，非常快)
	for (int32 i = 0; i < NumBars; i++)
	{
		int32 BaseVertIdx = i * VertsPerBar;
		for (int32 Idx : TemplateTris)
		{
			MeshData.Triangles.Add(BaseVertIdx + Idx);
		}
	}


	// 6. 提交到 GPU (注意：bCreateCollision = false)
	MainMeshComponent->CreateMeshSection(0, MoveTemp(MeshData), false);
}

void AVisBarChart::HandleClick()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC) return;

    // 检查左键是否刚刚按下
    if (PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
    {
        // 复用之前的 Raycast 逻辑获取当前鼠标下的索引
        // 注意：这里需要你把 Tick 里的 Raycast 逻辑稍微封装一下，或者直接复制过来
        // 假设我们有一个成员变量 CurrentHoverIndex 记录了 Tick 中计算出的结果
        int32 TargetIndex = LastHoverIndex; // LastHoverIndex 是我们在 Tick 里算的
    	float MouseX, MouseY;
    	if (!PC->GetMousePosition(MouseX, MouseY)) return;
    	FVector WorldLoc, WorldDir;
    	if (PC->DeprojectScreenPositionToWorld(MouseX, MouseY, WorldLoc, WorldDir))
    	{
    		FTransform ActorTrans = GetTransform();
    		FVector LocalStart = ActorTrans.InverseTransformPosition(WorldLoc);
    		FVector LocalDir = ActorTrans.InverseTransformVectorNoScale(WorldDir);
			
    		// 归一化方向（InverseTransformVectorNoScale 不保证归一化）
    		LocalDir.Normalize();

    		// --- 执行 Raycast ---
    		TargetIndex = RaycastOnBarChart(LocalStart, LocalDir);
    	}

        if (TargetIndex != -1)
        {
            // --- 选中了新的柱子 ---
            SelectedIndex = TargetIndex;

            // 1. 更新选中网格位置 (和 Highlight 逻辑一样)
            int32 Row = SelectedIndex / GridColumnCount;
            int32 Col = SelectedIndex % GridColumnCount;
            float XPos = Col * (BarWidth + BarGap);
            float YPos = Row * (BarWidth + BarGap);
            float RawHeight = CachedDataValues[SelectedIndex] * HeightMultiplier;
            float FinalHeight = RawHeight; // 选中通常保持原高，或者也稍微放大

        	SelectionMeshComponent->SetWorldLocation(GetActorLocation() + FVector(XPos, YPos, 0.0f));
            
        	// 重置缩放为 1 (所有的尺寸在网格生成时计算)
        	SelectionMeshComponent->SetRelativeScale3D(FVector(1.0f));
        	float FrameGap = 2.0f; 
        	float LineThickness = 5.0f; // 线条粗细
        	GenerateSelectionFrame(BarWidth + FrameGap, FinalHeight + FrameGap, LineThickness);
            
        	SelectionMeshComponent->SetVisibility(true);

            // 2. [核心] 更新材质参数以产生遮挡剔除效果
        	FVector LocalPos = FVector(XPos, YPos, 0);
        	FVector WorldPos = MainMeshComponent->GetComponentTransform().TransformPosition(LocalPos);

        	// 使用 KismetMaterialLibrary 更新全局参数集合
        	UKismetMaterialLibrary::SetVectorParameterValue(this, ChartMPC, FName("TargetPosition"), FLinearColor(WorldPos)); // 注意 FVector 转 FLinearColor
        	UKismetMaterialLibrary::SetScalarParameterValue(this, ChartMPC, FName("CutoutRadius"), 2000);
        	UKismetMaterialLibrary::SetScalarParameterValue(this, ChartMPC, FName("IsSelectionActive"), 1.0f);
        }
        else
        {
            // --- 点击了空白处：取消选中 ---
            SelectedIndex = -1;
            SelectionMeshComponent->SetVisibility(false);

        	UKismetMaterialLibrary::SetScalarParameterValue(this, ChartMPC, FName("IsSelectionActive"), 0.0f);
        }
    }
}

int32 AVisBarChart::RaycastOnBarChart(FVector LocalStart, FVector LocalDir) const
{
	// 1. 基础参数
	float CellSize = BarWidth + BarGap;
	float HalfBarWidth = BarWidth * 0.5f;
	
	// 步长：为了性能和精度的平衡，步长设为柱子宽度的一半是比较安全的
	// 太大会穿模，太小会浪费性能
	float StepSize = BarWidth * 0.5f; 
	FVector StepVec = LocalDir * StepSize;

	// 2. 确定搜索范围
	// 如果相机在上方，我们向下搜索直到 Z=0
	// 也就是光线行进的总距离预估
	float MaxDist = 200000.0f; // 设置一个最大距离防止死循环
	
	// 优化：如果射线是朝上的（LocalDir.Z > 0），且起点已经在最高柱子之上，那就不可能命中
	// 这里简单处理，假设主要从上往下看

	FVector CurrentPos = LocalStart;

	// 3. 步进循环 (Ray Marching)
	// 限制最大步数，例如 500 步 (足够覆盖很长的视距)
	for (int32 Step = 0; Step < 1000; ++Step)
	{
		// 如果 Z 小于 0，说明穿过了地面，肯定没戏了 (假设柱子底面在 Z=0)
		if (CurrentPos.Z < 0.0f) 
		{
			return -1;
		}

		// --- 空间哈希 (Spatial Hashing) ---
		// 计算当前点所在的格子行列
		// 注意：GenerateBarChart 中 Location = Col * CellSize
		// 这意味着柱子中心在整数格点上
		int32 Col = FMath::RoundToInt(CurrentPos.X / CellSize);
		int32 Row = FMath::RoundToInt(CurrentPos.Y / CellSize);

		// 检查索引是否越界
		if (Col >= 0 && Col < GridColumnCount)
		{
			int32 Index = Row * GridColumnCount + Col;
			
			// 确保 Index 在有效数据范围内
			if (CachedDataValues.IsValidIndex(Index))
			{
				// --- 命中测试 (Intersection Test) ---
				
				// 1. 检查高度：当前点必须在柱子高度范围内
				float BarHeight = CachedDataValues[Index] * HeightMultiplier;
				if (CurrentPos.Z <= BarHeight)
				{
					// 2. 检查水平范围 (XZ/YZ)：
					// 虽然我们算出了 Row/Col，但我们可能只是路过这个格子的空隙(Gap)
					// 需要精确判断是否在 BarWidth 的范围内
					float BarCenterX = Col * CellSize;
					float BarCenterY = Row * CellSize;

					if (FMath::Abs(CurrentPos.X - BarCenterX) <= HalfBarWidth &&
						FMath::Abs(CurrentPos.Y - BarCenterY) <= HalfBarWidth)
					{
						// **命中！**
						// 光线确实进入了这个柱子的实体内部
						return Index;
					}
				}
			}
		}

		// 没命中，向前走一步
		CurrentPos += StepVec;
	}

	return -1;
}

void AVisBarChart::GenerateSelectionFrame(float Width, float Height, float Thickness)
{
	{
        TArray<FVector> WireVerts;
        TArray<int32> WireTris;
        TArray<FVector> WireNormals;
        TArray<FVector2D> WireUVs;
        TArray<FVisMeshTangent> WireTangents;

        // 计算 Radius (半长)
        FVector BoxRadius(Width * 0.5f, Width * 0.5f, Height * 0.5f);

        // 调用库函数生成线框
        UKismetVisMeshLibrary::GenerateWireframeBoxMesh(BoxRadius, Thickness, WireVerts, WireTris, WireNormals, WireUVs, WireTangents);

        // 颜色填充
        TArray<FColor> WireColors;
        WireColors.Init(FColor::White, WireVerts.Num());

        // 偏移 Z 轴 (将中心点从 0 移到 Height/2)
        float ZOffset = Height * 0.5f;
        for (FVector& V : WireVerts)
        {
            V.Z += ZOffset;
        }

        // 创建 Section 0
        SelectionMeshComponent->CreateMeshSection(0, WireVerts, WireTris, WireNormals, WireUVs, WireUVs, WireUVs, WireUVs, WireColors, WireTangents, false);
        
        // 确保 Section 0 使用线框材质
        if (SelectionMeshMaterial)
        {
            SelectionMeshComponent->SetMaterial(0, SelectionMeshMaterial);
        }
    }

    {
        TArray<FVector> SolidVerts;
        TArray<int32> SolidTris;
        TArray<FVector> SolidNormals;
        TArray<FVector2D> SolidUVs;
        TArray<FVisMeshTangent> SolidTangents;

        // 生成实心盒子 (注意：GenerateBoxMesh 接受的是 Radius/半长)
        // 我们希望实体稍微比线框小一点点，或者正好填满。
        // 由于 HandleClick 传入的 Width 已经包含了 Gap，这里直接使用即可填满线框内部。
        FVector SolidExtents(Width * 0.5f, Width * 0.5f, Height * 0.5f);
        
        UKismetVisMeshLibrary::GenerateBoxMesh(SolidExtents, SolidVerts, SolidTris, SolidNormals, SolidUVs, SolidTangents);

        // 颜色填充
        TArray<FColor> SolidColors;
        SolidColors.Init(FColor::White, SolidVerts.Num());

        // 同样偏移 Z 轴
        float ZOffset = Height * 0.5f;
        for (FVector& V : SolidVerts)
        {
            V.Z += ZOffset;
        }

        // 创建 Section 1
        // 注意：这里 SectionIndex 是 1
        SelectionMeshComponent->CreateMeshSection(1, SolidVerts, SolidTris, SolidNormals, SolidUVs, SolidUVs, SolidUVs, SolidUVs, SolidColors, SolidTangents, false);

        // 设置材质为 HighlightMeshMaterial
        if (HighlightMeshMaterial)
        {
            SelectionMeshComponent->SetMaterial(1, HighlightMeshMaterial);
        }
    }
}

