#include "VisMeshRenderResources.h"

#include "VisMeshSubsystem.h"

const TCHAR* FPositionUAVVertexBuffer::GetName() const
{
	return TEXT("PositionUAVVertexBuffer");
}

FPositionUAVVertexBuffer::FPositionUAVVertexBuffer(int32 InNumVertices)
	: FVertexBuffer()
	  , NumVertices(InNumVertices)
{
}

void FPositionUAVVertexBuffer::InitRHI(FRHICommandListBase& RHICmdList)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPositionUAVVertexBuffer::InitRHI);

	const uint32 Size = NumVertices * sizeof(FVector3f);

	// 1. 使用标准的资源创建信息结构
	FRHIResourceCreateInfo CreateInfo(TEXT("PostionBuffer"));

	// 2. 设置 Usage Flags
	// EBufferUsageFlags::VertexBuffer : 允许作为顶点缓冲绑定
	// EBufferUsageFlags::UnorderedAccess : 允许作为 UAV (RWBuffer) 在计算着色器中写入
	// EBufferUsageFlags::ShaderResource : 允许作为 SRV 读取
	EBufferUsageFlags Usage = EBufferUsageFlags::VertexBuffer | EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::ShaderResource;

	// 3. 创建 Buffer (使用经典的 CreateVertexBuffer API)
	VertexBufferRHI = RHICmdList.CreateVertexBuffer(Size, Usage, CreateInfo);


	if (VertexBufferRHI)
	{
		SRV = RHICmdList.CreateShaderResourceView(
			VertexBufferRHI,
			FRHIViewDesc::CreateBufferSRV()
			.SetType(FRHIViewDesc::EBufferType::Typed)
			.SetFormat(PF_R32_FLOAT));

		UAV = RHICmdList.CreateUnorderedAccessView(
			VertexBufferRHI,
			FRHIViewDesc::CreateBufferUAV()
			.SetType(FRHIViewDesc::EBufferType::Typed)
			.SetFormat(PF_R32_FLOAT)
		);
	}
}

void FPositionUAVVertexBuffer::ReleaseRHI()
{
	UAV.SafeRelease();
	SRV.SafeRelease();
	FVertexBuffer::ReleaseRHI();
}