#include "RenderBase/VisMeshRenderResources.h"

const TCHAR* FPositionUAVVertexBuffer::GetName() const
{
	return TEXT("PositionUAVVertexBuffer");
}

void FPositionUAVVertexBuffer::InitRHI(FRHICommandListBase& RHICmdList)
{
	// TRACE_CPUPROFILER_EVENT_SCOPE(FPositionUAVVertexBuffer::InitRHI);
	//
	// const uint32 Size = NumVertices * sizeof(FVector3f);
	//
	// // 1. 使用标准的资源创建信息结构
	// FRHIResourceCreateInfo CreateInfo(TEXT("PostionBuffer"));
	//
	// // 2. 设置 Usage Flags
	// // EBufferUsageFlags::VertexBuffer : 允许作为顶点缓冲绑定
	// // EBufferUsageFlags::UnorderedAccess : 允许作为 UAV (RWBuffer) 在计算着色器中写入
	// // EBufferUsageFlags::ShaderResource : 允许作为 SRV 读取
	// EBufferUsageFlags Usage = EBufferUsageFlags::VertexBuffer | EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::ShaderResource;
	//
	// // 3. 创建 Buffer (使用经典的 CreateVertexBuffer API)
	// VertexBufferRHI = RHICmdList.CreateVertexBuffer(Size, Usage, CreateInfo);
	//
	//
	// if (VertexBufferRHI)
	// {
	// 	SRV = RHICmdList.CreateShaderResourceView(
	// 		VertexBufferRHI,
	// 		FRHIViewDesc::CreateBufferSRV()
	// 		.SetType(FRHIViewDesc::EBufferType::Typed)
	// 		.SetFormat(PF_R32_FLOAT));
	//
	// 	UAV = RHICmdList.CreateUnorderedAccessView(
	// 		VertexBufferRHI,
	// 		FRHIViewDesc::CreateBufferUAV()
	// 		.SetType(FRHIViewDesc::EBufferType::Typed)
	// 		.SetFormat(PF_R32_FLOAT)
	// 	);
	// }

	TRACE_CPUPROFILER_EVENT_SCOPE(FPositionUAVVertexBuffer::InitRHI);

    const uint32 Stride = sizeof(FVector3f); // 12 bytes
    const uint32 Size = NumVertices * Stride;

    FRHIResourceCreateInfo CreateInfo(TEXT("PositionBuffer"));

    // 注意：StructuredBuffer 通常不需要 VertexBuffer 标记，除非你真的要把它绑定到 IA (Input Assembler)
    // 如果你要在 VS 中用 SV_VertexID 手动读取，可以去掉 VertexBuffer 标记
    EBufferUsageFlags Usage = EBufferUsageFlags::VertexBuffer | EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::ShaderResource | EBufferUsageFlags::StructuredBuffer;

    // 创建 Buffer (注意：UE5 中创建 StructuredBuffer 建议显式指定 Stride)
    VertexBufferRHI = RHICmdList.CreateVertexBuffer(Size, Usage, CreateInfo);

    if (VertexBufferRHI)
    {
       // SRV: 指定为 Structured
       SRV = RHICmdList.CreateShaderResourceView(
          VertexBufferRHI,
          FRHIViewDesc::CreateBufferSRV()
          .SetType(FRHIViewDesc::EBufferType::Structured) // 改为 Structured
          .SetStride(Stride)); // 设置步长为 12

       // UAV: 指定为 Structured
       UAV = RHICmdList.CreateUnorderedAccessView(
          VertexBufferRHI,
          FRHIViewDesc::CreateBufferUAV()
          .SetType(FRHIViewDesc::EBufferType::Structured) // 改为 Structured
          .SetStride(Stride));
    }
}

void FPositionUAVVertexBuffer::ReleaseRHI()
{
	UAV.SafeRelease();
	SRV.SafeRelease();
	FVertexBuffer::ReleaseRHI();
}


void FVisMeshSubBuffer::InitRHI(FRHICommandListBase& RHICmdList)
{
	const uint32 Stride = Vector4CountPerInstance * sizeof(FVector4f);
	const uint32 BufferSize = NumInstances * Stride;

	if (BufferSize > 0)
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("VisMeshSubBuffer"));
		EBufferUsageFlags Usage = EBufferUsageFlags::VertexBuffer | EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::ShaderResource;

		VertexBufferRHI = RHICmdList.CreateVertexBuffer(BufferSize, Usage, CreateInfo);

		if (VertexBufferRHI)
		{
			SRV = RHICmdList.CreateShaderResourceView(
            				VertexBufferRHI,
            				FRHIViewDesc::CreateBufferSRV()
            				.SetType(FRHIViewDesc::EBufferType::Typed)
            				.SetFormat(PF_A32B32G32R32F) // float4 format
            			);
            
            UAV = RHICmdList.CreateUnorderedAccessView(
            	VertexBufferRHI,
            	FRHIViewDesc::CreateBufferUAV()
            	.SetType(FRHIViewDesc::EBufferType::Typed)
            	.SetFormat(PF_A32B32G32R32F)
            );
		}
	}
}

void FVisMeshSubBuffer::ReleaseRHI()
{
	SRV.SafeRelease();
	UAV.SafeRelease();
	FVertexBuffer::ReleaseRHI();
}

void FVisMeshInstanceBuffer::BindToDataType(FInstancedVisMeshDataType& OutData) const
{
	// --------------------------------------------------------
	// 1. 绑定 SRV (用于 Manual Vertex Fetch / Vertex Shader读取)
	// --------------------------------------------------------
	FRHIShaderResourceView* OriginSRV = OriginBuffer.GetSRV();
	OutData.InstanceOriginSRV = OriginSRV ? OriginSRV : GNullColorVertexBuffer.VertexBufferSRV.GetReference();

	FRHIShaderResourceView* TransformSRV = TransformBuffer.GetSRV();
	OutData.InstanceTransformSRV = TransformSRV ? TransformSRV : GNullColorVertexBuffer.VertexBufferSRV.GetReference();

	FRHIShaderResourceView* LightmapSRV = LightmapBuffer.GetSRV();
	OutData.InstanceLightmapSRV = LightmapSRV ? LightmapSRV : GNullColorVertexBuffer.VertexBufferSRV.GetReference();

	OutData.InstanceCustomDataSRV = nullptr;

	// --------------------------------------------------------
	// 2. 绑定 Vertex Stream (用于 Input Assembler / 不支持 MVF 的平台)
	//    这里因为我们有独立的 FVertexBuffer 对象，可以完美绑定。
	// --------------------------------------------------------

	// [Origin] Attribute 8
	// 紧密排列 (Stride = sizeof(FVector4f))
	OutData.InstanceOriginComponent = FVertexStreamComponent(
		&OriginBuffer, 
		0, 
		16, 
		VET_Float4, 
		EVertexStreamUsage::ManualFetch | EVertexStreamUsage::Instancing
	);

	// [Transform] Attribute 9, 10, 11
	// 紧密排列 (Stride = 3 * sizeof(FVector4f))
	const uint32 TransformStride = 16;
	OutData.InstanceTransformComponent[0] = FVertexStreamComponent(&TransformBuffer, 0, TransformStride * 3, VET_Float4, EVertexStreamUsage::ManualFetch | EVertexStreamUsage::Instancing);
	OutData.InstanceTransformComponent[1] = FVertexStreamComponent(&TransformBuffer, TransformStride, TransformStride * 3, VET_Float4, EVertexStreamUsage::ManualFetch | EVertexStreamUsage::Instancing);
	OutData.InstanceTransformComponent[2] = FVertexStreamComponent(&TransformBuffer, 2 * TransformStride, TransformStride * 3, VET_Float4, EVertexStreamUsage::ManualFetch | EVertexStreamUsage::Instancing);

	// [Lightmap] Attribute 12
	// 紧密排列 (Stride = sizeof(FVector4f))
	OutData.InstanceLightmapAndShadowMapUVBiasComponent = FVertexStreamComponent(
		&LightmapBuffer, 
		0, 
		TransformStride, 
		VET_Float4, 
		EVertexStreamUsage::ManualFetch | EVertexStreamUsage::Instancing
	);
}
