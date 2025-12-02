// Fill out your copyright notice in the Description page of Project Settings.


#include "VisCharts/VisBarChart.h"

#include "Components/VisMeshProceduralComponent.h"
#include "Kismet/GameplayStatics.h"
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
}

// Called when the game starts or when spawned
void AVisBarChart::BeginPlay()
{
	Super::BeginPlay();

	// 测试数据
	// --- 1. 生成 100,000 个随机测试数据 ---
	const int32 NumData = 1000000;
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
			// --- 关键修改：转到局部空间 ---
			// 我们不求交平面，而是把整条射线转到 Actor 的局部空间
			// 这样 RaycastOnBarChart 里的计算就不需要考虑 Actor 的旋转和缩放了
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
				return;
			}
		}
	}

	// 未命中处理
	if (LastHoverIndex != -1)
	{
		HighlightMeshComponent->SetVisibility(false);
		LastHoverIndex = -1;
	}
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
	if (MainMeshMaterial)
	{
		MainMeshComponent->SetMaterial(0, MainMeshMaterial);
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

