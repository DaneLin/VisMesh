#include "Examples/ProceduralExampleActor.h"

#include "Utils/KismetVisMeshLibrary.h"

AProceduralExampleActor::AProceduralExampleActor()
{
	// 1. 创建组件并设为根节点
	VisMeshComp = CreateDefaultSubobject<UVisMeshProceduralComponent>(TEXT("VisMeshComp"));
	RootComponent = VisMeshComp;
}

void AProceduralExampleActor::BeginPlay()
{
	Super::BeginPlay();

	// 2. 准备网格数据 (画一个简单的三角形)
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FLinearColor> Colors;
	TArray<FVisMeshTangent> Tangents;

	UKismetVisMeshLibrary::GenerateBoxMesh(FVector(50,50,50), Vertices, Triangles, Normals, UV0, Tangents);
	// // --- 顶点位置 (相对坐标) ---
	// Vertices.Add(FVector(0, 0, 0));   // 0
	// Vertices.Add(FVector(0, 100, 0)); // 1
	// Vertices.Add(FVector(0, 0, 100)); // 2
	//
	// // --- 顶点索引 (逆时针顺序) ---
	// Triangles.Add(0);
	// Triangles.Add(1);
	// Triangles.Add(2);
	//
	// // --- 法线 (指向 -X 方向) ---
	// Normals.Init(FVector(-1, 0, 0), 3);
	//
	// // --- UV (简单映射) ---
	// UV0.Add(FVector2D(0, 0));
	// UV0.Add(FVector2D(1, 0));
	// UV0.Add(FVector2D(0, 1));

	// --- 顶点颜色 (红色) ---
	Colors.Init(FLinearColor::Red, 3);

	// --- 切线 (可选，留空或填充默认) ---
	// Tangents.Init(FVisMeshTangent(0, 1, 0), 3); 

	// 3. 调用 CreateMeshSection
	// 使用源码中 line 66 提供的简化版重载 (不需要填 UV1-3)
	VisMeshComp->CreateMeshSection_LinearColor(
		0,                  // Section Index
		Vertices,           // 顶点
		Triangles,          // 索引
		Normals,            // 法线
		UV0,                // UV0
		Colors,             // 颜色 (LinearColor)
		Tangents,           // 切线
		true,               // bCreateCollision (开启碰撞)
		false               // bSRGBConversion
	);
}