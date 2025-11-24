// Fill out your copyright notice in the Description page of Project Settings.

#include "Utils/KismetVisMeshLibrary.h"

#include "GeomTools.h"
#include "StaticMeshResources.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VisMeshProceduralComponent.h"
#include "Engine/StaticMesh.h"
#include "Logging/MessageLog.h"
#include "Materials/MaterialInterface.h"
#include "Misc/UObjectToken.h"
#include "PhysicsEngine/BodySetup.h"
#include "RenderBase/VisMeshRenderResources.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(KismetVisMeshLibrary)

#define LOCTEXT_NAMESPACE "KismetVisMeshLibrary"

// --- 辅助函数：SOA 顶点操作 ---
/** 将源 SOA 数据中的指定索引顶点复制到目标 SOA 数据末尾，并返回新索引 */
int32 VisMeshCopyVertex(FVisMeshData& Dest, const FVisMeshData& Src, int32 SrcIdx)
{
	// Position (必须存在)
	int32 NewIndex = Dest.Positions.Add(Src.Positions[SrcIdx]);

	// Attributes (检查源数据是否存在，若存在则复制，若不存在但目标有数据则补零)
	// Normals
	if (Src.Normals.IsValidIndex(SrcIdx)) Dest.Normals.Add(Src.Normals[SrcIdx]);
	else if (Dest.Normals.Num() > 0) Dest.Normals.Add(FVector(0, 0, 1));

	// Tangents
	if (Src.Tangents.IsValidIndex(SrcIdx)) Dest.Tangents.Add(Src.Tangents[SrcIdx]);
	else if (Dest.Tangents.Num() > 0) Dest.Tangents.Add(FVisMeshTangent());

	// Colors
	if (Src.Colors.IsValidIndex(SrcIdx)) Dest.Colors.Add(Src.Colors[SrcIdx]);
	else if (Dest.Colors.Num() > 0) Dest.Colors.Add(FColor::White);

	// UVs
	if (Src.UV0.IsValidIndex(SrcIdx)) Dest.UV0.Add(Src.UV0[SrcIdx]);
	else if (Dest.UV0.Num() > 0) Dest.UV0.Add(FVector2D::ZeroVector);

	if (Src.UV1.IsValidIndex(SrcIdx)) Dest.UV1.Add(Src.UV1[SrcIdx]);
	else if (Dest.UV1.Num() > 0) Dest.UV1.Add(FVector2D::ZeroVector);

	if (Src.UV2.IsValidIndex(SrcIdx)) Dest.UV2.Add(Src.UV2[SrcIdx]);
	else if (Dest.UV2.Num() > 0) Dest.UV2.Add(FVector2D::ZeroVector);

	if (Src.UV3.IsValidIndex(SrcIdx)) Dest.UV3.Add(Src.UV3[SrcIdx]);
	else if (Dest.UV3.Num() > 0) Dest.UV3.Add(FVector2D::ZeroVector);

	return NewIndex;
}

/** 在源 SOA 数据的两个顶点之间进行插值，结果存入目标 SOA，返回新索引 */
int32 VisMeshInterpolateVertex(FVisMeshData& Dest, const FVisMeshData& Src, int32 Idx0, int32 Idx1, float Alpha)
{
	// Handle dodgy alpha
	if (FMath::IsNaN(Alpha) || !FMath::IsFinite(Alpha))
	{
		return VisMeshCopyVertex(Dest, Src, Idx1);
	}

	// Position
	int32 NewIndex = Dest.Positions.Add(FMath::Lerp(Src.Positions[Idx0], Src.Positions[Idx1], Alpha));

	// Normals
	if (Src.Normals.IsValidIndex(Idx0) && Src.Normals.IsValidIndex(Idx1))
	{
		Dest.Normals.Add(FMath::Lerp(Src.Normals[Idx0], Src.Normals[Idx1], Alpha));
	}
	else if (Dest.Normals.Num() > 0) Dest.Normals.Add(FVector(0, 0, 1));

	// Tangents
	if (Src.Tangents.IsValidIndex(Idx0) && Src.Tangents.IsValidIndex(Idx1))
	{
		FVisMeshTangent NewTangent;
		NewTangent.TangentX = FMath::Lerp(Src.Tangents[Idx0].TangentX, Src.Tangents[Idx1].TangentX, Alpha);
		NewTangent.bFlipTangentY = Src.Tangents[Idx0].bFlipTangentY;
		Dest.Tangents.Add(NewTangent);
	}
	else if (Dest.Tangents.Num() > 0) Dest.Tangents.Add(FVisMeshTangent());

	// Colors
	if (Src.Colors.IsValidIndex(Idx0) && Src.Colors.IsValidIndex(Idx1))
	{
		const FColor& C0 = Src.Colors[Idx0];
		const FColor& C1 = Src.Colors[Idx1];
		FColor Result;
		Result.R = FMath::Clamp(FMath::TruncToInt(FMath::Lerp(float(C0.R), float(C1.R), Alpha)), 0, 255);
		Result.G = FMath::Clamp(FMath::TruncToInt(FMath::Lerp(float(C0.G), float(C1.G), Alpha)), 0, 255);
		Result.B = FMath::Clamp(FMath::TruncToInt(FMath::Lerp(float(C0.B), float(C1.B), Alpha)), 0, 255);
		Result.A = FMath::Clamp(FMath::TruncToInt(FMath::Lerp(float(C0.A), float(C1.A), Alpha)), 0, 255);
		Dest.Colors.Add(Result);
	}
	else if (Dest.Colors.Num() > 0) Dest.Colors.Add(FColor::White);

	// UVs
	auto InterpolateUV = [&](const TArray<FVector2D>& SrcUV, TArray<FVector2D>& DestUV)
	{
		if (SrcUV.IsValidIndex(Idx0) && SrcUV.IsValidIndex(Idx1))
			DestUV.Add(FMath::Lerp(SrcUV[Idx0], SrcUV[Idx1], Alpha));
		else if (DestUV.Num() > 0) DestUV.Add(FVector2D::ZeroVector);
	};

	InterpolateUV(Src.UV0, Dest.UV0);
	InterpolateUV(Src.UV1, Dest.UV1);
	InterpolateUV(Src.UV2, Dest.UV2);
	InterpolateUV(Src.UV3, Dest.UV3);

	return NewIndex;
}

void UKismetVisMeshLibrary::GenerateBoxMesh(FVector BoxRadius, TArray<FVector>& Vertices, TArray<int32>& Triangles,TArray<FVector>& Normals, TArray<FVector2D>& UVs, TArray<FVisMeshTangent>& Tangents)
{
	// Generate verts
	FVector BoxVerts[8];
	BoxVerts[0] = FVector(-BoxRadius.X, BoxRadius.Y, BoxRadius.Z);
	BoxVerts[1] = FVector(BoxRadius.X, BoxRadius.Y, BoxRadius.Z);
	BoxVerts[2] = FVector(BoxRadius.X, -BoxRadius.Y, BoxRadius.Z);
	BoxVerts[3] = FVector(-BoxRadius.X, -BoxRadius.Y, BoxRadius.Z);

	BoxVerts[4] = FVector(-BoxRadius.X, BoxRadius.Y, -BoxRadius.Z);
	BoxVerts[5] = FVector(BoxRadius.X, BoxRadius.Y, -BoxRadius.Z);
	BoxVerts[6] = FVector(BoxRadius.X, -BoxRadius.Y, -BoxRadius.Z);
	BoxVerts[7] = FVector(-BoxRadius.X, -BoxRadius.Y, -BoxRadius.Z);

	// Generate triangles (from quads)
	Triangles.Reset();

	const int32 NumVerts = 24; // 6 faces x 4 verts per face

	Vertices.Reset();
	Vertices.AddUninitialized(NumVerts);

	Normals.Reset();
	Normals.AddUninitialized(NumVerts);

	Tangents.Reset();
	Tangents.AddUninitialized(NumVerts);

	Vertices[0] = BoxVerts[0];
	Vertices[1] = BoxVerts[1];
	Vertices[2] = BoxVerts[2];
	Vertices[3] = BoxVerts[3];
	ConvertQuadToTriangles(Triangles, 0, 1, 2, 3);
	Normals[0] = Normals[1] = Normals[2] = Normals[3] = FVector(0, 0, 1);
	Tangents[0] = Tangents[1] = Tangents[2] = Tangents[3] = FVisMeshTangent(0.f, -1.f, 0.f);

	Vertices[4] = BoxVerts[4];
	Vertices[5] = BoxVerts[0];
	Vertices[6] = BoxVerts[3];
	Vertices[7] = BoxVerts[7];
	ConvertQuadToTriangles(Triangles, 4, 5, 6, 7);
	Normals[4] = Normals[5] = Normals[6] = Normals[7] = FVector(-1, 0, 0);
	Tangents[4] = Tangents[5] = Tangents[6] = Tangents[7] = FVisMeshTangent(0.f, -1.f, 0.f);

	Vertices[8] = BoxVerts[5];
	Vertices[9] = BoxVerts[1];
	Vertices[10] = BoxVerts[0];
	Vertices[11] = BoxVerts[4];
	ConvertQuadToTriangles(Triangles, 8, 9, 10, 11);
	Normals[8] = Normals[9] = Normals[10] = Normals[11] = FVector(0, 1, 0);
	Tangents[8] = Tangents[9] = Tangents[10] = Tangents[11] = FVisMeshTangent(-1.f, 0.f, 0.f);

	Vertices[12] = BoxVerts[6];
	Vertices[13] = BoxVerts[2];
	Vertices[14] = BoxVerts[1];
	Vertices[15] = BoxVerts[5];
	ConvertQuadToTriangles(Triangles, 12, 13, 14, 15);
	Normals[12] = Normals[13] = Normals[14] = Normals[15] = FVector(1, 0, 0);
	Tangents[12] = Tangents[13] = Tangents[14] = Tangents[15] = FVisMeshTangent(0.f, 1.f, 0.f);

	Vertices[16] = BoxVerts[7];
	Vertices[17] = BoxVerts[3];
	Vertices[18] = BoxVerts[2];
	Vertices[19] = BoxVerts[6];
	ConvertQuadToTriangles(Triangles, 16, 17, 18, 19);
	Normals[16] = Normals[17] = Normals[18] = Normals[19] = FVector(0, -1, 0);
	Tangents[16] = Tangents[17] = Tangents[18] = Tangents[19] = FVisMeshTangent(1.f, 0.f, 0.f);

	Vertices[20] = BoxVerts[7];
	Vertices[21] = BoxVerts[6];
	Vertices[22] = BoxVerts[5];
	Vertices[23] = BoxVerts[4];
	ConvertQuadToTriangles(Triangles, 20, 21, 22, 23);
	Normals[20] = Normals[21] = Normals[22] = Normals[23] = FVector(0, 0, -1);
	Tangents[20] = Tangents[21] = Tangents[22] = Tangents[23] = FVisMeshTangent(0.f, 1.f, 0.f);

	// UVs
	UVs.Reset();
	UVs.AddUninitialized(NumVerts);

	UVs[0] = UVs[4] = UVs[8] = UVs[12] = UVs[16] = UVs[20] = FVector2D(0.f, 0.f);
	UVs[1] = UVs[5] = UVs[9] = UVs[13] = UVs[17] = UVs[21] = FVector2D(0.f, 1.f);
	UVs[2] = UVs[6] = UVs[10] = UVs[14] = UVs[18] = UVs[22] = FVector2D(1.f, 1.f);
	UVs[3] = UVs[7] = UVs[11] = UVs[15] = UVs[19] = UVs[23] = FVector2D(1.f, 0.f);
}

void VisMeshFindVertOverlaps(int32 TestVertIndex, const TArray<FVector>& Verts, TArray<int32>& VertOverlaps)
{
	// Check if Verts is empty or test is outside range
	if (TestVertIndex < Verts.Num())
	{
		const FVector TestVert = Verts[TestVertIndex];

		for (int32 VertIdx = 0; VertIdx < Verts.Num(); VertIdx++)
		{
			// First see if we overlap, and smoothing groups are the same
			if (TestVert.Equals(Verts[VertIdx]))
			{
				// If it, so we are at least considered an 'overlap' for normal gen
				VertOverlaps.Add(VertIdx);
			}
		}
	}
}

void UKismetVisMeshLibrary::CalculateTangentsForMesh(const TArray<FVector>& Vertices, const TArray<int32>& Triangles,const TArray<FVector2D>& UVs, TArray<FVector>& Normals, TArray<FVisMeshTangent>& Tangents)
{

	if (Vertices.Num() == 0)
	{
		return;
	}

	// Number of triangles
	const int32 NumTris = Triangles.Num() / 3;
	// Number of verts
	const int32 NumVerts = Vertices.Num();

	// Map of vertex to triangles in Triangles array
	TMultiMap<int32, int32> VertToTriMap;
	// Map of vertex to triangles to consider for normal calculation
	TMultiMap<int32, int32> VertToTriSmoothMap;

	// Normal/tangents for each face
	TArray<FVector3f> FaceTangentX, FaceTangentY, FaceTangentZ;
	FaceTangentX.AddUninitialized(NumTris);
	FaceTangentY.AddUninitialized(NumTris);
	FaceTangentZ.AddUninitialized(NumTris);

	// Iterate over triangles
	for (int TriIdx = 0; TriIdx < NumTris; TriIdx++)
	{
		int32 CornerIndex[3];
		FVector3f P[3];

		for (int32 CornerIdx = 0; CornerIdx < 3; CornerIdx++)
		{
			// Find vert index (clamped within range)
			int32 VertIndex = FMath::Min(Triangles[(TriIdx * 3) + CornerIdx], NumVerts - 1);

			CornerIndex[CornerIdx] = VertIndex;
			P[CornerIdx] = (FVector3f)Vertices[VertIndex];

			// Find/add this vert to index buffer
			TArray<int32> VertOverlaps;
			VisMeshFindVertOverlaps(VertIndex, Vertices, VertOverlaps);

			// Remember which triangles map to this vert
			VertToTriMap.AddUnique(VertIndex, TriIdx);
			VertToTriSmoothMap.AddUnique(VertIndex, TriIdx);

			// Also update map of triangles that 'overlap' this vert (ie don't match UV, but do match smoothing) and should be considered when calculating normal
			for (int32 OverlapIdx = 0; OverlapIdx < VertOverlaps.Num(); OverlapIdx++)
			{
				// For each vert we overlap..
				int32 OverlapVertIdx = VertOverlaps[OverlapIdx];

				// Add this triangle to that vert
				VertToTriSmoothMap.AddUnique(OverlapVertIdx, TriIdx);

				// And add all of its triangles to us
				TArray<int32> OverlapTris;
				VertToTriMap.MultiFind(OverlapVertIdx, OverlapTris);
				for (int32 OverlapTriIdx = 0; OverlapTriIdx < OverlapTris.Num(); OverlapTriIdx++)
				{
					VertToTriSmoothMap.AddUnique(VertIndex, OverlapTris[OverlapTriIdx]);
				}
			}
		}

		// Calculate triangle edge vectors and normal
		const FVector3f Edge21 = P[1] - P[2];
		const FVector3f Edge20 = P[0] - P[2];
		const FVector3f TriNormal = (Edge21 ^ Edge20).GetSafeNormal();

		// If we have UVs, use those to calc 
		if (UVs.Num() == Vertices.Num())
		{
			const FVector2D T1 = UVs[CornerIndex[0]];
			const FVector2D T2 = UVs[CornerIndex[1]];
			const FVector2D T3 = UVs[CornerIndex[2]];

			FMatrix	ParameterToLocal(
				FPlane(P[1].X - P[0].X, P[1].Y - P[0].Y, P[1].Z - P[0].Z, 0),
				FPlane(P[2].X - P[0].X, P[2].Y - P[0].Y, P[2].Z - P[0].Z, 0),
				FPlane(P[0].X, P[0].Y, P[0].Z, 0),
				FPlane(0, 0, 0, 1)
				);

			FMatrix ParameterToTexture(
				FPlane(T2.X - T1.X, T2.Y - T1.Y, 0, 0),
				FPlane(T3.X - T1.X, T3.Y - T1.Y, 0, 0),
				FPlane(T1.X, T1.Y, 1, 0),
				FPlane(0, 0, 0, 1)
				);

			// Use InverseSlow to catch singular matrices.  Inverse can miss this sometimes.
			const FMatrix TextureToLocal = ParameterToTexture.Inverse() * ParameterToLocal;

			FaceTangentX[TriIdx] = FVector4f(TextureToLocal.TransformVector(FVector(1, 0, 0)).GetSafeNormal());
			FaceTangentY[TriIdx] = FVector4f(TextureToLocal.TransformVector(FVector(0, 1, 0)).GetSafeNormal());
		}
		else
		{
			FaceTangentX[TriIdx] = Edge20.GetSafeNormal();
			FaceTangentY[TriIdx] = (FaceTangentX[TriIdx] ^ TriNormal).GetSafeNormal();
		}

		FaceTangentZ[TriIdx] = TriNormal;
	}


	// Arrays to accumulate tangents into
	TArray<FVector3f> VertexTangentXSum, VertexTangentYSum, VertexTangentZSum;
	VertexTangentXSum.AddZeroed(NumVerts);
	VertexTangentYSum.AddZeroed(NumVerts);
	VertexTangentZSum.AddZeroed(NumVerts);

	// For each vertex..
	for (int VertxIdx = 0; VertxIdx < Vertices.Num(); VertxIdx++)
	{
		// Find relevant triangles for normal
		TArray<int32> SmoothTris;
		VertToTriSmoothMap.MultiFind(VertxIdx, SmoothTris);

		for (int i = 0; i < SmoothTris.Num(); i++)
		{
			int32 TriIdx = SmoothTris[i];
			VertexTangentZSum[VertxIdx] += FaceTangentZ[TriIdx];
		}

		// Find relevant triangles for tangents
		TArray<int32> TangentTris;
		VertToTriMap.MultiFind(VertxIdx, TangentTris);

		for (int i = 0; i < TangentTris.Num(); i++)
		{
			int32 TriIdx = TangentTris[i];
			VertexTangentXSum[VertxIdx] += FaceTangentX[TriIdx];
			VertexTangentYSum[VertxIdx] += FaceTangentY[TriIdx];
		}
	}

	// Finally, normalize tangents and build output arrays

	Normals.Reset();
	Normals.AddUninitialized(NumVerts);

	Tangents.Reset();
	Tangents.AddUninitialized(NumVerts);

	for (int VertxIdx = 0; VertxIdx < NumVerts; VertxIdx++)
	{
		FVector3f& TangentX = VertexTangentXSum[VertxIdx];
		FVector3f& TangentY = VertexTangentYSum[VertxIdx];
		FVector3f& TangentZ = VertexTangentZSum[VertxIdx];

		TangentX.Normalize();
		TangentZ.Normalize();

		Normals[VertxIdx] = (FVector)TangentZ;

		// Use Gram-Schmidt orthogonalization to make sure X is orth with Z
		TangentX -= TangentZ * (TangentZ | TangentX);
		TangentX.Normalize();

		// See if we need to flip TangentY when generating from cross product
		const bool bFlipBitangent = ((TangentZ ^ TangentX) | TangentY) < 0.f;

		Tangents[VertxIdx] = FVisMeshTangent((FVector)TangentX, bFlipBitangent);
	}
}

void UKismetVisMeshLibrary::ConvertQuadToTriangles(TArray<int32>& Triangles, int32 Vert0, int32 Vert1, int32 Vert2,
	int32 Vert3)
{
	Triangles.Add(Vert0);
	Triangles.Add(Vert1);
	Triangles.Add(Vert3);

	Triangles.Add(Vert1);
	Triangles.Add(Vert2);
	Triangles.Add(Vert3);
}

void UKismetVisMeshLibrary::CreateGridMeshTriangles(int32 NumX, int32 NumY, bool bWinding, TArray<int32>& Triangles)
{
	Triangles.Reset();

	if (NumX >= 2 && NumY >= 2)
	{
		// Build Quads
		for (int XIdx = 0; XIdx < NumX - 1; XIdx++)
		{
			for (int YIdx = 0; YIdx < NumY - 1; YIdx++)
			{
				const int32 I0 = (XIdx + 0)*NumY + (YIdx + 0);
				const int32 I1 = (XIdx + 1)*NumY + (YIdx + 0);
				const int32 I2 = (XIdx + 1)*NumY + (YIdx + 1);
				const int32 I3 = (XIdx + 0)*NumY + (YIdx + 1);

				if (bWinding)
				{
					ConvertQuadToTriangles(Triangles, I0, I1, I2, I3);
				}
				else
				{
					ConvertQuadToTriangles(Triangles, I0, I3, I2, I1);
				}
			}
		}
	}
}

void UKismetVisMeshLibrary::CreateGridMeshWelded(int32 NumX, int32 NumY, TArray<int32>& Triangles, TArray<FVector>& Vertices, TArray<FVector2D>& UVs, float GridSpacing)
{
	Triangles.Empty();
	Vertices.Empty();
	UVs.Empty();

	if (NumX >= 2 && NumY >= 2)
	{
		FVector2D Extent = FVector2D((NumX - 1)* GridSpacing, (NumY - 1) * GridSpacing) / 2;

		for (int i = 0; i < NumY; i++)
		{
			for (int j = 0; j < NumX; j++)
			{
				Vertices.Add(FVector((float)j * GridSpacing - Extent.X, (float)i * GridSpacing - Extent.Y, 0));
				UVs.Add(FVector2D((float)j / ((float)NumX - 1), (float)i / ((float)NumY - 1)));
			}
		}

		for (int i = 0; i < NumY - 1; i++)
		{
			for (int j = 0; j < NumX - 1; j++)
			{
				int idx = j + (i * NumX);
				Triangles.Add(idx);
				Triangles.Add(idx + NumX);
				Triangles.Add(idx + 1);

				Triangles.Add(idx + 1);
				Triangles.Add(idx + NumX);
				Triangles.Add(idx + NumX + 1);
			}
		}
	}
}

void UKismetVisMeshLibrary::CreateGridMeshSplit(int32 NumX, int32 NumY, TArray<int32>& Triangles, TArray<FVector>& Vertices, TArray<FVector2D>& UVs, TArray<FVector2D>& UV1s, float GridSpacing)
{
	Triangles.Empty();
	Vertices.Empty();
	UVs.Empty();
	UV1s.Empty();

	if (NumX >= 2 && NumY >= 2)
	{

		FVector2D Extent = FVector2D(NumX * GridSpacing, NumY * GridSpacing) / 2;

		for (int i = 0; i < NumY - 1; i++)
		{
			for (int j = 0; j < NumX - 1; j++)
			{
				int idx = j + (i * (NumX - 1));
				Triangles.Add(idx * 4 + 3);
				Triangles.Add(idx * 4 + 1);
				Triangles.Add(idx * 4);

				Triangles.Add(idx * 4 + 3);
				Triangles.Add(idx * 4 + 2);
				Triangles.Add(idx * 4 + 1);

				float Z = FMath::Fmod(idx, 5.f) * GridSpacing;
				FVector CornerVert = FVector((float)j * GridSpacing - Extent.X, (float)i * GridSpacing - Extent.Y, Z);
				Vertices.Add(CornerVert);
				Vertices.Add(CornerVert + FVector(GridSpacing, 0, 0));
				Vertices.Add(CornerVert + FVector(GridSpacing, GridSpacing, 0));
				Vertices.Add(CornerVert + FVector(0, GridSpacing, 0));

				FVector2D UVCorner = FVector2D((float)j / ((float)NumX - 1), (float)i / ((float)NumY - 1));
				UVs.Add(FVector2D(0, 0));
				UVs.Add(FVector2D(1, 0));
				UVs.Add(FVector2D(1, 1));
				UVs.Add(FVector2D(0, 1));

				FVector2D QuadCenter = FVector2D(((float)j + 0.5) / ((float)NumX), ((float)i + 0.5) / ((float)NumY));
				UV1s.Add(QuadCenter);
				UV1s.Add(QuadCenter);
				UV1s.Add(QuadCenter);
				UV1s.Add(QuadCenter);
			}
		}
	}
}

static int32 GetNewIndexForOldVertIndex(int32 MeshVertIndex, TMap<int32, int32>& MeshToSectionVertMap, const FStaticMeshVertexBuffers& VertexBuffers, TArray<FVector>& Vertices, TArray<FVector>& Normals, TArray<FVector2D>& UVs, TArray<FVisMeshTangent>& Tangents)
{
	int32* NewIndexPtr = MeshToSectionVertMap.Find(MeshVertIndex);
	if (NewIndexPtr != nullptr)
	{
		return *NewIndexPtr;
	}
	else
	{
		// Copy position
		int32 SectionVertIndex = Vertices.Add((FVector)VertexBuffers.PositionVertexBuffer.VertexPosition(MeshVertIndex));

		// Copy normal
		Normals.Add(FVector4(VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(MeshVertIndex)));
		check(Normals.Num() == Vertices.Num());

		// Copy UVs
		UVs.Add(FVector2D(VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(MeshVertIndex, 0)));
		check(UVs.Num() == Vertices.Num());

		// Copy tangents
		FVector4 TangentX = (FVector4)VertexBuffers.StaticMeshVertexBuffer.VertexTangentX(MeshVertIndex);
		FVisMeshTangent NewTangent(TangentX, TangentX.W < 0.f);
		Tangents.Add(NewTangent);
		check(Tangents.Num() == Vertices.Num());

		MeshToSectionVertMap.Add(MeshVertIndex, SectionVertIndex);

		return SectionVertIndex;
	}
}

void UKismetVisMeshLibrary::GetSectionFromStaticMesh(UStaticMesh* InMesh, int32 LODIndex, int32 SectionIndex, TArray<FVector>& Vertices, TArray<int32>& Triangles, TArray<FVector>& Normals, TArray<FVector2D>& UVs, TArray<FVisMeshTangent>& Tangents)
{
	if(	InMesh != nullptr )
	{
		if (!InMesh->bAllowCPUAccess)
		{
			FMessageLog("PIE").Warning()
				->AddToken(FTextToken::Create(LOCTEXT("GetSectionFromStaticMeshStart", "Calling GetSectionFromStaticMesh on")))
				->AddToken(FUObjectToken::Create(InMesh))
				->AddToken(FTextToken::Create(LOCTEXT("GetSectionFromStaticMeshEnd", "but 'Allow CPU Access' is not enabled. This is required for converting StaticMesh to VisMeshComponent in cooked builds.")));
		}

		if (InMesh->GetRenderData() != nullptr && InMesh->GetRenderData()->LODResources.IsValidIndex(LODIndex))
		{
			const FStaticMeshLODResources& LOD = InMesh->GetRenderData()->LODResources[LODIndex];
			if (LOD.Sections.IsValidIndex(SectionIndex))
			{
				// Empty output buffers
				Vertices.Reset();
				Triangles.Reset();
				Normals.Reset();
				UVs.Reset();
				Tangents.Reset();

				// Map from vert buffer for whole mesh to vert buffer for section of interest
				TMap<int32, int32> MeshToSectionVertMap;

				const FStaticMeshSection& Section = LOD.Sections[SectionIndex];
				const uint32 OnePastLastIndex = Section.FirstIndex + Section.NumTriangles * 3;
				FIndexArrayView Indices = LOD.IndexBuffer.GetArrayView();

				// Iterate over section index buffer, copying verts as needed
				for (uint32 i = Section.FirstIndex; i < OnePastLastIndex; i++)
				{
					uint32 MeshVertIndex = Indices[i];

					// See if we have this vert already in our section vert buffer, and copy vert in if not 
					int32 SectionVertIndex = GetNewIndexForOldVertIndex(MeshVertIndex, MeshToSectionVertMap, LOD.VertexBuffers, Vertices, Normals, UVs, Tangents);

					// Add to index buffer
					Triangles.Add(SectionVertIndex);
				}
			}
		}
	}
}

void UKismetVisMeshLibrary::CopyVisMeshFromStaticMeshComponent(UStaticMeshComponent* StaticMeshComponent, int32 LODIndex, UVisMeshProceduralComponent* ProcMeshComponent, bool bCreateCollision)
{
	if( StaticMeshComponent != nullptr && 
		StaticMeshComponent->GetStaticMesh() != nullptr &&
		ProcMeshComponent != nullptr )
	{
		UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();

		//// MESH DATA

		int32 NumSections = StaticMesh->GetNumSections(LODIndex);
		for (int32 SectionIndex = 0; SectionIndex < NumSections; SectionIndex++)
		{
			// Buffers for copying geom data
			TArray<FVector> Vertices;
			TArray<int32> Triangles;
			TArray<FVector> Normals;
			TArray<FVector2D> UVs;
			TArray<FVector2D> UVs1;
			TArray<FVector2D> UVs2;
			TArray<FVector2D> UVs3;
			TArray<FVisMeshTangent> Tangents;

			// Get geom data from static mesh
			GetSectionFromStaticMesh(StaticMesh, LODIndex, SectionIndex, Vertices, Triangles, Normals, UVs, Tangents);

			// Create section using data
			TArray<FLinearColor> DummyColors;
			ProcMeshComponent->CreateMeshSection_LinearColor(SectionIndex, Vertices, Triangles, Normals, UVs, UVs1, UVs2, UVs3, DummyColors, Tangents, bCreateCollision);
		}

		//// SIMPLE COLLISION

		// Clear any existing collision hulls
		ProcMeshComponent->ClearCollisionConvexMeshes();

		if (StaticMesh->GetBodySetup() != nullptr)
		{
			// Iterate over all convex hulls on static mesh..
			const int32 NumConvex = StaticMesh->GetBodySetup()->AggGeom.ConvexElems.Num();
			for (int ConvexIndex = 0; ConvexIndex < NumConvex; ConvexIndex++)
			{
				// Copy convex verts to ProcMesh
				FKConvexElem& MeshConvex = StaticMesh->GetBodySetup()->AggGeom.ConvexElems[ConvexIndex];
				ProcMeshComponent->AddCollisionConvexMesh(MeshConvex.VertexData);
			}
		}

		//// MATERIALS

		for (int32 MatIndex = 0; MatIndex < StaticMeshComponent->GetNumMaterials(); MatIndex++)
		{
			ProcMeshComponent->SetMaterial(MatIndex, StaticMeshComponent->GetMaterial(MatIndex));
		}
	}
}

void UKismetVisMeshLibrary::GetSectionFromVisMesh(UVisMeshProceduralComponent* InProcMesh, int32 SectionIndex,TArray<FVector>& Vertices, TArray<int32>& Triangles, TArray<FVector>& Normals, TArray<FVector2D>& UVs,TArray<FVisMeshTangent>& Tangents)
{
	if (InProcMesh && SectionIndex >= 0 && SectionIndex < InProcMesh->GetNumSections())
	{
		const FVisMeshSection* Section = InProcMesh->GetVisMeshSection(SectionIndex);
		const FVisMeshData& Data = Section->Data;

		Vertices = Data.Positions;
		// Copy Attributes (with safety check if they exist)
		const int32 NumVerts = Vertices.Num();
		
		if (Data.Normals.Num() == NumVerts) Normals = Data.Normals;
		else Normals.Init(FVector(0, 0, 1), NumVerts);

		if (Data.UV0.Num() == NumVerts) UVs = Data.UV0;
		else UVs.Init(FVector2D::ZeroVector, NumVerts);

		if (Data.Tangents.Num() == NumVerts) Tangents = Data.Tangents;
		else Tangents.Init(FVisMeshTangent(), NumVerts);

		// Copy index buffer
		Triangles = Data.Triangles;
	}
}

//////////////////////////////////////////////////////////////////////////

/** Util that returns 1 ir on positive side of plane, -1 if negative, or 0 if split by plane */
int32 VisMeshBoxPlaneCompare(FBox InBox, const FPlane& InPlane)
{
	FVector BoxCenter, BoxExtents;
	InBox.GetCenterAndExtents(BoxCenter, BoxExtents);

	// Find distance of box center from plane
	FVector::FReal BoxCenterDist = InPlane.PlaneDot(BoxCenter);

	// See size of box in plane normal direction
	FVector::FReal BoxSize = FVector::BoxPushOut(InPlane, BoxExtents);

	if (BoxCenterDist > BoxSize)
	{
		return 1;
	}
	else if (BoxCenterDist < -BoxSize)
	{
		return -1;
	}
	else
	{
		return 0;
	}
}

/** Transform triangle from 2D to 3D static-mesh triangle (SOA Version) */
void VisMeshTransform2DPolygonTo3D(const FUtilPoly2D& InPoly, const FMatrix& InMatrix, FVisMeshData& OutData, FBox& OutBox)
{
	FVector PolyNormal = (FVector)-InMatrix.GetUnitAxis(EAxis::Z);
	FVisMeshTangent PolyTangent(InMatrix.GetUnitAxis(EAxis::X), false);

	for (int32 VertexIndex = 0; VertexIndex < InPoly.Verts.Num(); VertexIndex++)
	{
		const FUtilVertex2D& InVertex = InPoly.Verts[VertexIndex];

		FVector NewPos = InMatrix.TransformPosition(FVector(InVertex.Pos.X, InVertex.Pos.Y, 0.f));
		
		OutData.Positions.Add(NewPos);
		OutData.Normals.Add(PolyNormal);
		OutData.Tangents.Add(PolyTangent);
		OutData.Colors.Add(InVertex.Color);
		OutData.UV0.Add(InVertex.UV);
		// Zero fill others if needed
		if(OutData.UV1.Num() > 0) OutData.UV1.Add(FVector2D::ZeroVector);
		if(OutData.UV2.Num() > 0) OutData.UV2.Add(FVector2D::ZeroVector);
		if(OutData.UV3.Num() > 0) OutData.UV3.Add(FVector2D::ZeroVector);

		// Update bounding box
		OutBox += NewPos;
	}
}

/** Given a polygon, decompose into triangles. (SOA Version) */
bool VisMeshTriangulatePoly(TArray<int32>& OutTris, const FVisMeshData& PolyData, int32 VertBase, const FVector3f& PolyNormal)
{
	// Can't work if not enough verts for 1 triangle
	int32 NumVerts = PolyData.Positions.Num() - VertBase;
	if (NumVerts < 3)
	{
		// Warning: This simple fallback assumes at least 3 verts exist in total? 
		// Logic preserved from original, though 0, 2, 1 might be invalid if NumVerts < 3
		if (NumVerts == 3) 
		{
			OutTris.Add(0 + VertBase);
			OutTris.Add(2 + VertBase);
			OutTris.Add(1 + VertBase);
			return true;
		}
		return false; 
	}

	const int32 TriBase = OutTris.Num();

	TArray<int32> VertIndices;
	VertIndices.AddUninitialized(NumVerts);
	for (int VertIndex = 0; VertIndex < NumVerts; VertIndex++)
	{
		VertIndices[VertIndex] = VertBase + VertIndex;
	}

	while (VertIndices.Num() >= 3)
	{
		bool bFoundEar = false;
		for (int32 EarVertexIndex = 0; EarVertexIndex < VertIndices.Num(); EarVertexIndex++)
		{
			const int32 AIndex = (EarVertexIndex == 0) ? VertIndices.Num() - 1 : EarVertexIndex - 1;
			const int32 BIndex = EarVertexIndex;
			const int32 CIndex = (EarVertexIndex + 1) % VertIndices.Num();

			// Access Positions directly from SOA
			const FVector3f APos = (FVector3f)PolyData.Positions[VertIndices[AIndex]];
			const FVector3f BPos = (FVector3f)PolyData.Positions[VertIndices[BIndex]];
			const FVector3f CPos = (FVector3f)PolyData.Positions[VertIndices[CIndex]];

			const FVector3f ABEdge = BPos - APos;
			const FVector3f ACEdge = CPos - APos;
			const float TriangleDeterminant = (ABEdge ^ ACEdge) | PolyNormal;
			if (TriangleDeterminant > 0.f)
			{
				continue;
			}

			bool bFoundVertInside = false;
			for (int32 VertexIndex = 0; VertexIndex < VertIndices.Num(); VertexIndex++)
			{
				if (VertexIndex != AIndex && VertexIndex != BIndex && VertexIndex != CIndex)
				{
					const FVector3f TestPos = (FVector3f)PolyData.Positions[VertIndices[VertexIndex]];
					if (FGeomTools::PointInTriangle(APos, BPos, CPos, TestPos))
					{
						bFoundVertInside = true;
						break;
					}
				}
			}

			if (!bFoundVertInside)
			{
				OutTris.Add(VertIndices[AIndex]);
				OutTris.Add(VertIndices[CIndex]);
				OutTris.Add(VertIndices[BIndex]);

				VertIndices.RemoveAt(EarVertexIndex);
				bFoundEar = true;
				break;
			}
		}

		if (!bFoundEar)
		{
			OutTris.SetNum(TriBase);
			return false;
		}
	}

	return true;
}

/** Util to slice a convex hull with a plane */
void VisMeshSliceConvexElem(const FKConvexElem& InConvex, const FPlane& SlicePlane, TArray<FVector>& OutConvexVerts)
{
	// Get set of planes that make up hull
	TArray<FPlane> ConvexPlanes;
	InConvex.GetPlanes(ConvexPlanes);

	if (ConvexPlanes.Num() >= 4)
	{
		// Add on the slicing plane (need to flip as it culls geom in the opposite sense to our geom culling code)
		ConvexPlanes.Add(SlicePlane.Flip());

		// Create output convex based on new set of planes
		FKConvexElem SlicedElem;
		bool bSuccess = SlicedElem.HullFromPlanes(ConvexPlanes, InConvex.VertexData);
		if (bSuccess)
		{
			OutConvexVerts = SlicedElem.VertexData;
		}
	}
}

void UKismetVisMeshLibrary::SliceVisMesh(UVisMeshProceduralComponent* InProcMesh, FVector PlanePosition,FVector PlaneNormal, bool bCreateOtherHalf, UVisMeshProceduralComponent*& OutOtherHalfProcMesh,EVisMeshSliceCapOption CapOption, UMaterialInterface* CapMaterial)
{
	if (InProcMesh != nullptr)
	{
		// Transform plane from world to local space
		FTransform ProcCompToWorld = InProcMesh->GetComponentToWorld();
		FVector LocalPlanePos = ProcCompToWorld.InverseTransformPosition(PlanePosition);
		FVector LocalPlaneNormal = ProcCompToWorld.InverseTransformVectorNoScale(PlaneNormal);
		LocalPlaneNormal = LocalPlaneNormal.GetSafeNormal(); // Ensure normalized

		FPlane SlicePlane(LocalPlanePos, LocalPlaneNormal);

		// Set of sections to add to the 'other half' component
		TArray<FVisMeshSection> OtherSections;
		// Material for each section of other half
		TArray<UMaterialInterface*> OtherMaterials;

		// Set of new edges created by clipping polys by plane
		TArray<FUtilEdge3D> ClipEdges;

		for (int32 SectionIndex = 0; SectionIndex < InProcMesh->GetNumSections(); SectionIndex++)
		{
			FVisMeshSection* BaseSection = InProcMesh->GetVisMeshSection(SectionIndex);
			// If we have a section, and it has some valid geom
			if (BaseSection != nullptr && BaseSection->Data.Triangles.Num() > 0 && BaseSection->Data.Positions.Num() > 0)
			{
				// Compare bounding box of section with slicing plane
				int32 BoxCompare = VisMeshBoxPlaneCompare(BaseSection->SectionLocalBox, SlicePlane);

				// Box totally clipped, clear section
				if (BoxCompare == -1)
				{
					// Add entire section to other half
					if (bCreateOtherHalf)
					{
						OtherSections.Add(*BaseSection);
						OtherMaterials.Add(InProcMesh->GetMaterial(SectionIndex));
					}

					InProcMesh->ClearMeshSection(SectionIndex);
				}
				// Box totally on one side of plane, leave it alone, do nothing
				else if (BoxCompare == 1)
				{
					// ...
				}
				// Box intersects plane, need to clip some polys!
				else
				{
					// New section for geometry
					FVisMeshSection NewSection;

					// New section for 'other half' geometry (if desired)
					FVisMeshSection* NewOtherSection = nullptr;
					if (bCreateOtherHalf)
					{
						int32 OtherSectionIndex = OtherSections.Add(FVisMeshSection());
						NewOtherSection = &OtherSections[OtherSectionIndex];

						OtherMaterials.Add(InProcMesh->GetMaterial(SectionIndex)); // Remember material for this section
					}

					// Map of base vert index to sliced vert index
					TMap<int32, int32> BaseToSlicedVertIndex;
					TMap<int32, int32> BaseToOtherSlicedVertIndex;

					const int32 NumBaseVerts = BaseSection->Data.Positions.Num();
					
					// Distance of each base vert from slice plane
					TArray<float> VertDistance;
					VertDistance.AddUninitialized(NumBaseVerts);

					// Build vertex buffer 
					for (int32 BaseVertIndex = 0; BaseVertIndex < NumBaseVerts; BaseVertIndex++)
					{
						const FVector& BasePos = BaseSection->Data.Positions[BaseVertIndex];
						
						// Calc distance from plane
						VertDistance[BaseVertIndex] = SlicePlane.PlaneDot(BasePos);
						
						// See if vert is being kept in this section
						if (VertDistance[BaseVertIndex] > 0.f)
						{
							// Changed: Copy to sliced v buffer using SOA Helper
							int32 SlicedVertIndex = VisMeshCopyVertex(NewSection.Data, BaseSection->Data, BaseVertIndex);
							// Update section bounds
							NewSection.SectionLocalBox += BasePos;
							// Add to map
							BaseToSlicedVertIndex.Add(BaseVertIndex, SlicedVertIndex);
						}
						// Or add to other half if desired
						else if(NewOtherSection != nullptr)
						{
							int32 SlicedVertIndex = VisMeshCopyVertex(NewOtherSection->Data, BaseSection->Data, BaseVertIndex);
							NewOtherSection->SectionLocalBox += BasePos;
							BaseToOtherSlicedVertIndex.Add(BaseVertIndex, SlicedVertIndex);
						}
					}


					// Iterate over base triangles (ie 3 indices at a time)
					for (int32 BaseIndex = 0; BaseIndex < BaseSection->Data.Triangles.Num(); BaseIndex += 3)
					{
						int32 BaseV[3]; // Triangle vert indices in original mesh
						int32* SlicedV[3]; // Pointers to vert indices in new v buffer
						int32* SlicedOtherV[3]; // Pointers to vert indices in new 'other half' v buffer

						// For each vertex..
						for (int32 i = 0; i < 3; i++)
						{
							// Get triangle vert index
							BaseV[i] = BaseSection->Data.Triangles[BaseIndex + i];
							// Look up in sliced v buffer
							SlicedV[i] = BaseToSlicedVertIndex.Find(BaseV[i]);
							// Look up in 'other half' v buffer (if desired)
							if (bCreateOtherHalf)
							{
								SlicedOtherV[i] = BaseToOtherSlicedVertIndex.Find(BaseV[i]);
								// Each base vert _must_ exist in either BaseToSlicedVertIndex or BaseToOtherSlicedVertIndex 
								check((SlicedV[i] != nullptr) != (SlicedOtherV[i] != nullptr));
							}
						}

						// If all verts survived plane cull, keep the triangle
						if (SlicedV[0] != nullptr && SlicedV[1] != nullptr && SlicedV[2] != nullptr)
						{
							NewSection.Data.Triangles.Add(*SlicedV[0]);
							NewSection.Data.Triangles.Add(*SlicedV[1]);
							NewSection.Data.Triangles.Add(*SlicedV[2]);
						}
						// If all verts were removed by plane cull
						else if (SlicedV[0] == nullptr && SlicedV[1] == nullptr && SlicedV[2] == nullptr)
						{
							// If creating other half, add all verts to that
							if (NewOtherSection != nullptr)
							{
								NewOtherSection->Data.Triangles.Add(*SlicedOtherV[0]);
								NewOtherSection->Data.Triangles.Add(*SlicedOtherV[1]);
								NewOtherSection->Data.Triangles.Add(*SlicedOtherV[2]);
							}
						}
						// If partially culled, clip to create 1 or 2 new triangles
						else
						{
							int32 FinalVerts[4];
							int32 NumFinalVerts = 0;

							int32 OtherFinalVerts[4];
							int32 NumOtherFinalVerts = 0;

							FUtilEdge3D NewClipEdge;
							int32 ClippedEdges = 0;

							float PlaneDist[3];
							PlaneDist[0] = VertDistance[BaseV[0]];
							PlaneDist[1] = VertDistance[BaseV[1]];
							PlaneDist[2] = VertDistance[BaseV[2]];

							for (int32 EdgeIdx = 0; EdgeIdx < 3; EdgeIdx++)
							{
								int32 ThisVert = EdgeIdx;

								// If start vert is inside, add it.
								if (SlicedV[ThisVert] != nullptr)
								{
									check(NumFinalVerts < 4);
									FinalVerts[NumFinalVerts++] = *SlicedV[ThisVert];
								}
								// If not, add to other side
								else if(bCreateOtherHalf)
								{
									check(NumOtherFinalVerts < 4);
									OtherFinalVerts[NumOtherFinalVerts++] = *SlicedOtherV[ThisVert];
								}

								// If start and next vert are on opposite sides, add intersection
								int32 NextVert = (EdgeIdx + 1) % 3;

								if ((SlicedV[EdgeIdx] == nullptr) != (SlicedV[NextVert] == nullptr))
								{
									// Find distance along edge that plane is
									float Alpha = -PlaneDist[ThisVert] / (PlaneDist[NextVert] - PlaneDist[ThisVert]);
									// Interpolate vertex params to that point (SOA Helper)
									// Add to vertex buffer
									int32 InterpVertIndex = VisMeshInterpolateVertex(NewSection.Data, BaseSection->Data, BaseV[ThisVert], BaseV[NextVert], FMath::Clamp(Alpha, 0.0f, 1.0f));
									const FVector InterpPos = NewSection.Data.Positions[InterpVertIndex];
									// Update bounds
									NewSection.SectionLocalBox += InterpPos;

									// Save vert index for this poly
									check(NumFinalVerts < 4);
									FinalVerts[NumFinalVerts++] = InterpVertIndex;

									// If desired, add to the poly for the other half as well
									if (NewOtherSection != nullptr)
									{
										int32 OtherInterpVertIndex = VisMeshInterpolateVertex(NewOtherSection->Data, BaseSection->Data, BaseV[ThisVert], BaseV[NextVert], FMath::Clamp(Alpha, 0.0f, 1.0f));
										NewOtherSection->SectionLocalBox += InterpPos;
										check(NumOtherFinalVerts < 4);
										OtherFinalVerts[NumOtherFinalVerts++] = OtherInterpVertIndex;
									}

									// When we make a new edge on the surface of the clip plane, save it off.
									check(ClippedEdges < 2);
									if (ClippedEdges == 0)
									{
										NewClipEdge.V0 = (FVector3f)InterpPos;
									}
									else
									{
										NewClipEdge.V1 = (FVector3f)InterpPos;
									}

									ClippedEdges++;
								}
							}

							// Triangulate the clipped polygon.
							for (int32 VertexIndex = 2; VertexIndex < NumFinalVerts; VertexIndex++)
							{
								NewSection.Data.Triangles.Add(FinalVerts[0]);
								NewSection.Data.Triangles.Add(FinalVerts[VertexIndex - 1]);
								NewSection.Data.Triangles.Add(FinalVerts[VertexIndex]);
							}

							// If we are making the other half, triangulate that as well
							if (NewOtherSection != nullptr)
							{
								for (int32 VertexIndex = 2; VertexIndex < NumOtherFinalVerts; VertexIndex++)
								{
									NewOtherSection->Data.Triangles.Add(OtherFinalVerts[0]);
									NewOtherSection->Data.Triangles.Add(OtherFinalVerts[VertexIndex - 1]);
									NewOtherSection->Data.Triangles.Add(OtherFinalVerts[VertexIndex]);
								}
							}

							check(ClippedEdges != 1); // Should never clip just one edge of the triangle

							// If we created a new edge, save that off here as well
							if (ClippedEdges == 2)
							{
								ClipEdges.Add(NewClipEdge);
							}
						}
					}

					// Remove 'other' section from array if no valid geometry for it
					if (NewOtherSection != nullptr && (NewOtherSection->Data.Triangles.Num() == 0 || NewOtherSection->Data.Positions.Num() == 0))
					{
						OtherSections.RemoveAt(OtherSections.Num() - 1);
					}

					// If we have some valid geometry, update section
					if (NewSection.Data.Triangles.Num() > 0 && NewSection.Data.Positions.Num() > 0)
					{
						// Assign new geom to this section
						InProcMesh->SetVisMeshSection(SectionIndex, NewSection);
					}
					// If we don't, remove this section
					else
					{
						InProcMesh->ClearMeshSection(SectionIndex);
					}
				}
			}
		}

		// Create cap geometry (if some edges to create it from)
		if (CapOption != EVisMeshSliceCapOption::NoCap && ClipEdges.Num() > 0)
		{
			FVisMeshSection CapSection;
			int32 CapSectionIndex = INDEX_NONE;

			// If using an existing section, copy that info first
			if (CapOption == EVisMeshSliceCapOption::UseLastSectionForCap)
			{
				CapSectionIndex = InProcMesh->GetNumSections() - 1;
				CapSection = *InProcMesh->GetVisMeshSection(CapSectionIndex);
			}
			// Adding new section for cap
			else
			{
				CapSectionIndex = InProcMesh->GetNumSections();
			}

			// Project 3D edges onto slice plane to form 2D edges
			TArray<FUtilEdge2D> Edges2D;
			FUtilPoly2DSet PolySet;
			FGeomTools::ProjectEdges(Edges2D, PolySet.PolyToWorld, ClipEdges, SlicePlane);

			// Find 2D closed polygons from this edge soup
			FGeomTools::Buid2DPolysFromEdges(PolySet.Polys, Edges2D, FColor(255, 255, 255, 255));

			// Remember start point for vert and index buffer before adding and cap geom
			int32 CapVertBase = CapSection.Data.Positions.Num();
			int32 CapIndexBase = CapSection.Data.Triangles.Num();

			// Triangulate each poly
			for (int32 PolyIdx = 0; PolyIdx < PolySet.Polys.Num(); PolyIdx++)
			{
				// Generate UVs for the 2D polygon.
				FGeomTools::GeneratePlanarTilingPolyUVs(PolySet.Polys[PolyIdx], 64.f);

				// Remember start of vert buffer before adding triangles for this poly
				int32 PolyVertBase = CapSection.Data.Positions.Num();

				// Transform from 2D poly verts to 3D (Updated SOA version)
				VisMeshTransform2DPolygonTo3D(PolySet.Polys[PolyIdx], PolySet.PolyToWorld, CapSection.Data, CapSection.SectionLocalBox);

				// Triangulate this polygon (Updated SOA version)
				VisMeshTriangulatePoly(CapSection.Data.Triangles, CapSection.Data, PolyVertBase, (FVector3f)LocalPlaneNormal);
			}

			// Set geom for cap section
			InProcMesh->SetVisMeshSection(CapSectionIndex, CapSection);

			// If creating new section for cap, assign cap material to it
			if (CapOption == EVisMeshSliceCapOption::CreateNewSectionForCap)
			{
				InProcMesh->SetMaterial(CapSectionIndex, CapMaterial);
			}

			// If creating the other half, copy cap geom into other half sections
			if (bCreateOtherHalf)
			{
				// Find section we want to use for the cap on the 'other half'
				FVisMeshSection* OtherCapSection;
				if (CapOption == EVisMeshSliceCapOption::CreateNewSectionForCap)
				{
					OtherSections.Add(FVisMeshSection());
					OtherMaterials.Add(CapMaterial);
				}
				OtherCapSection = &OtherSections.Last();

				// Remember current base index for verts in 'other cap section'
				int32 OtherCapVertBase = OtherCapSection->Data.Positions.Num();

				// Copy verts from cap section into other cap section
				for (int32 VertIdx = CapVertBase; VertIdx < CapSection.Data.Positions.Num(); VertIdx++)
				{
					// Copy using SOA Helper
					int32 NewIdx = VisMeshCopyVertex(OtherCapSection->Data, CapSection.Data, VertIdx);
					
					// Flip normal and tangent
					// Note: We can access directly since CopyVertex ensures they exist if source exists
					if (OtherCapSection->Data.Normals.IsValidIndex(NewIdx))
					{
						OtherCapSection->Data.Normals[NewIdx] *= -1.f;
					}
					if (OtherCapSection->Data.Tangents.IsValidIndex(NewIdx))
					{
						OtherCapSection->Data.Tangents[NewIdx].TangentX *= -1.f;
					}
					
					// And update bounding box
					OtherCapSection->SectionLocalBox += OtherCapSection->Data.Positions[NewIdx];
				}

				// Find offset between main cap verts and other cap verts
				int32 VertOffset = OtherCapVertBase - CapVertBase;

				// Copy indices over as well
				for (int32 IndexIdx = CapIndexBase; IndexIdx < CapSection.Data.Triangles.Num(); IndexIdx += 3)
				{
					// Need to offset and change winding
					OtherCapSection->Data.Triangles.Add(CapSection.Data.Triangles[IndexIdx + 0] + VertOffset);
					OtherCapSection->Data.Triangles.Add(CapSection.Data.Triangles[IndexIdx + 2] + VertOffset);
					OtherCapSection->Data.Triangles.Add(CapSection.Data.Triangles[IndexIdx + 1] + VertOffset);
				}
			}
		}

		// Array of sliced collision shapes
		TArray< TArray<FVector> > SlicedCollision;
		TArray< TArray<FVector> > OtherSlicedCollision;

		UBodySetup* ProcMeshBodySetup = InProcMesh->GetBodySetup();

		for (int32 ConvexIndex = 0; ConvexIndex < ProcMeshBodySetup->AggGeom.ConvexElems.Num(); ConvexIndex++)
		{
			FKConvexElem& BaseConvex = ProcMeshBodySetup->AggGeom.ConvexElems[ConvexIndex];

			int32 BoxCompare = VisMeshBoxPlaneCompare(BaseConvex.ElemBox, SlicePlane);

			// If box totally clipped, add to other half (if desired)
			if (BoxCompare == -1)
			{
				if (bCreateOtherHalf)
				{
					OtherSlicedCollision.Add(BaseConvex.VertexData);
				}
			}
			// If box totally valid, just keep mesh as is
			else if (BoxCompare == 1)
			{
				SlicedCollision.Add(BaseConvex.VertexData);				// LWC_TODO: Perf pessimization
			}
			// Need to actually slice the convex shape
			else
			{
				TArray<FVector> SlicedConvexVerts;
				VisMeshSliceConvexElem(BaseConvex, SlicePlane, SlicedConvexVerts);
				// If we got something valid, add it
				if (SlicedConvexVerts.Num() >= 4)
				{
					SlicedCollision.Add(SlicedConvexVerts);
				}

				// Slice again to get the other half of the collision, if desired
				if (bCreateOtherHalf)
				{
					TArray<FVector> OtherSlicedConvexVerts;
					VisMeshSliceConvexElem(BaseConvex, SlicePlane.Flip(), OtherSlicedConvexVerts);
					if (OtherSlicedConvexVerts.Num() >= 4)
					{
						OtherSlicedCollision.Add(OtherSlicedConvexVerts);
					}
				}
			}
		}

		// Update collision of proc mesh
		InProcMesh->SetCollisionConvexMeshes(SlicedCollision);

		// If creating other half, create component now
		if (bCreateOtherHalf)
		{
			// Create new component with the same outer as the proc mesh passed in
			OutOtherHalfProcMesh = NewObject<UVisMeshProceduralComponent>(InProcMesh->GetOuter());

			// Set transform to match source component
			OutOtherHalfProcMesh->SetWorldTransform(InProcMesh->GetComponentTransform());

			// Add each section of geometry
			for (int32 SectionIndex = 0; SectionIndex < OtherSections.Num(); SectionIndex++)
			{
				OutOtherHalfProcMesh->SetVisMeshSection(SectionIndex, OtherSections[SectionIndex]);
				OutOtherHalfProcMesh->SetMaterial(SectionIndex, OtherMaterials[SectionIndex]);
			}

			// Copy collision settings from input mesh
			OutOtherHalfProcMesh->SetCollisionProfileName(InProcMesh->GetCollisionProfileName());
			OutOtherHalfProcMesh->SetCollisionEnabled(InProcMesh->GetCollisionEnabled());
			OutOtherHalfProcMesh->bUseComplexAsSimpleCollision = InProcMesh->bUseComplexAsSimpleCollision;

			// Assign sliced collision
			OutOtherHalfProcMesh->SetCollisionConvexMeshes(OtherSlicedCollision);

			// Finally register
			OutOtherHalfProcMesh->RegisterComponent();
		}
	}
}

#undef LOCTEXT_NAMESPACE
