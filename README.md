# VisMesh
## 1.项目概述

VisMesh 是一个基于 Unreal Engine (UE) 的 C++ 插件，实现了一个全 GPU 驱动的程序化网格渲染管线。其核心功能旨在提供网格可视化 (Mesh Visualization) 与自定义网格渲染 的能力。该插件不仅仅依赖于 UE 的材质系统，还包含了独立的 HLSL Shader (着色器) 代码，涉及更底层的渲染管线扩展，例如：

- 通过 Compute Shader 处理网格数据。

- 通过 Vertex Factory 自定义顶点处理。

## 2. 现有功能分析

### 2.1.VisMeshSubsystem

继承自 UWorldSubsystem。维护一个所有活跃 VisMeshComponent 的注册表。它创建并管理 SceneViewExtension（SVE）。

**设计意图**: 提供一个中心化的管理点，让 Render Thread 的 SVE 能够遍历所有组件并分发计算任务，而无需组件自己去 Hook 渲染流程。

### 2.2.VisMeshSceneViewExtension

连接 Game Thread 和 Render Thread 的桥梁。

**执行时机**: PreRenderViewFamily_RenderThread。这是在场景渲染开始之前。

**逻辑**: 遍历 Subsystem 中注册的所有组件对应的 Proxy，调用它们的 DispatchComputePass_RenderThread。这确保了在几何体被 Draw 之前，顶点数据已经被 Compute Shader 更新完毕。

### 2.3.VisMeshIndirectComponent

通过 Compute Shader (计算着色器) 在 GPU 端直接生成顶点数据和绘制参数（Indirect Arguments），并结合 SceneViewExtension (SVE) 机制在渲染线程注入计算任务，最终利用 DrawPrimitiveIndirect 进行渲染。

#### 2.3.1.组件分析

场景代理 (VisMeshIndirectSceneProxy):

**资源持有**:

- FPositionUAVVertexBuffer: 用于存储 Compute Shader 输出的顶点数据（RWBuffer）。

- IndirectArgsBuffer: 存储间接绘制参数（VertexCount, InstanceCount 等）。

- FLocalVertexFactory: 标准的顶点工厂，但数据源被替换为上述的 UAV Buffer。

**核心任务**:

- DispatchComputePass_RenderThread: 构建 RDG (Render Dependency Graph)，绑定 Shader 参数，分发 CS 任务。

- GetDynamicMeshElements: 提交 MeshBatch。关键在于它不提交具体的顶点索引列表，而是绑定 IndirectArgsBuffer，告诉 GPU "去那个 Buffer 里看我们要画多少个点"。

#### 2.3.2.性能分析

**优势**：

- 极高的吞吐量: 能够轻松处理数十万甚至百万级的简单几何体，只要显存足够。

- 零 CPU 消耗: 几何体更新完全不占用 Game Thread 或 Render Thread 的 CPU 时间。

- 动态性: 非常适合做 3D 频谱仪、地形、流体表面等每帧都在变化的网格。

**潜在瓶颈**：

- 显存带宽：目前方案采用Triangle List且无索引的方式，会占用大量带宽

### 2.4.VisMeshInstancedComponent

一个标准的、高性能的 Mass Instancing 实现。它通过自定义 Vertex Factory 和 Compute Shader 绕过了 UE 传统的 CPU 端 Instance 管理（ISM/HISM），直接在 Render Dependency Graph (RDG) 中完成数据的更新与渲染。

#### 2.4.1.VisMeshInstancedVertexFactory

Unreal Engine中通过VertexFactory来获取并处理具体的渲染数据。这是本方案中最复杂也是最关键的 C++ 部分。

#### 2.4.2.性能分析

对于 3D 柱状图 这种应用场景，所有物体都是相同的立方体，只是高度不同。

- Indirect 方案: 100万个柱子需要写入 1,000,000×36 verts×12 bytes≈432 MB 的数据每帧。

- Instanced 方案: 100万个柱子需要写入 1,000,000×3 float4×16 bytes≈48 MB 的数据。

**结论**: Instanced 方案节省了约 90% 的写入带宽。

**潜在问题**：

- 顶点工厂的复杂性：FVisMeshInstancedVertexFactory 需要编写对应的 .usf 文件（通常是 LocalVertexFactory.ush 的变体）来配合 C++ 代码。

### 2.5.VisMeshLineComponent

该组件用于在 GPU 端程序化生成 带有宽度 的 3D 线框（Wireframe）。没有使用图形 API 原生的线段（LineList 拓扑），而是采用 视口对齐公告板（View-Aligned Billboards） 技术，将每条线段构建为面朝相机的四边形（两个三角形）。

**目的**: 解决原生硬件线宽（glLineWidth）在现代图形 API（DX11/12, Vulkan）中通常被限制为 1px 的问题。

**代价**: 几何体复杂度大幅增加。画一个立方体线框需要生成 72 个顶点，而不是原生的 24 个顶点（LineList）。

#### 2.5.1.组件分析

##### 几何体生成策略：Billboard Lines

文件: VisMeshDrawCommon.ush，核心逻辑位于 AppendLineSegment 函数中。典型的 CPU 粒子/条带渲染算法的 GPU 移植版。

- 视口对齐 (Camera Facing):为了让线段看起来有宽度，生成的面片必须始终垂直于观察方向。算法通过计算 ToCamera 向量和线段方向 LineDir 的叉积 (cross) 来获得 Right 向量 。这个 Right 向量决定了线段宽度的扩展方向。

- 顶点展开:利用 Right * (Width * 0.5f) 计算偏移量 。生成 4 个角点 (p0 至 p3)，并写入 2 个三角形（6 个顶点）来构成一个矩形管线 。

这种技术生成的线在 3D 空间中实际上是扁平的纸片，但由于始终朝向相机，看起来像圆柱体。

#### 2.5.2.性能分析

**优点**：

- 线宽控制: 支持任意 float 宽度的线 (LineWidth)

- 零 CPU 开销: 所有 8 个角点和 12 条边的几何扩展都在 GPU 完成 。

**潜在问题**：

- 动态性与视口依赖：由于计算逻辑依赖相机位置 (CameraPos)，这意味着每当相机移动或旋转，几何体都必须重新计算。如果 Shader 不每帧运行，当旋转视角时，线段会变得“纸片化”甚至消失（因为它们不再垂直于视线）。

- 极高的显存带宽消耗

- Overdraw：线框渲染通常意味着大量的半透明或叠加。由于线是有宽度的实体几何，密集的柱状图会导致严重的像素重绘。

#### 2.5.3.未来优化方向

将 Billboard 逻辑移入 Vertex Shader。

- Buffer: 存储静态的 Line List (每个 Box 24 个顶点，或者使用 Index Buffer)。

- VS: 在 Vertex Shader 中计算 Cross(ToCamera, LineDir) 并偏移顶点。

优势: 彻底消除 Compute Shader 每帧重写几百万个 float 的带宽开销。数据是静态的，只有计算是动态的。

### 2.6.VisMeshProceduralComponent

针对高频、动态、海量网格数据生成的深度优化版本。解决原生 PMC 在处理大规模动态网格（如流体模拟、体素破坏、即时地形生成）时的性能瓶颈。

#### 2.6.1.核心差异

VisMesh 针对原生 `ProceduralMeshComponent` 存在的性能瓶颈进行了底层重构，采用了面向数据的设计思想 (Data-Oriented Design)。

| 特性 | 原生 ProceduralMeshComponent (PMC) | 优化版 VisMeshProceduralComponent | 优势分析 |
| :--- | :--- | :--- | :--- |
| **数据布局** | **AOS (Array of Structures)**<br>`TArray<FProcMeshVertex>` | **SOA (Structure of Arrays)**<br>`FVisMeshData` (Positions, Normals 等分离) | **缓存命中率更高**：便于 SIMD 指令优化，仅访问所需属性。 |
| **数据传递** | **深拷贝 (Deep Copy)**<br>从 TArray 参数拷贝 | **移动语义 (Move Semantics)**<br>支持 `MoveTemp` 所有权转移 | **零拷贝开销**：极大减少动态更新时的内存分配与复制。 |
| **并行处理** | **串行 (Serial)**<br>普通 `for` 循环 | **并行 (Parallel)**<br>GT/RT 均使用 `ParallelFor` | **多核利用率高**：海量顶点处理速度呈线性提升。 |
| **渲染更新** | **全量重建**<br>结构体遍历转换 | **极速 Memcpy**<br>LockBuffer 后直接内存拷贝 | **GPU 上传更快**：降低 CPU 到 GPU 的延迟。 |
| **物理更新** | **低效**<br>需提取 Position | **高效**<br>复用 SOA Position 数组 | **物理烘焙加速**：无二次分配开销。 |

#### 2.6.2.技术分析

##### 2.6.2.1.数据结构架构（AOS VS SOA）

原生 PMC: 使用 FProcMeshVertex 结构体，包含位置、法线、切线、颜色、UV。

- 劣势：当物理引擎只需要 Position 数据时，必须遍历整个结构体数组提取位置，导致大量缓存未命中 (Cache Miss)。更新某一项属性（如仅更新 VertexColor）时，需要重写整个顶点缓冲区。

优化版 VisMesh: 使用 FVisMeshData，内部为 TArray<FVector> Positions、TArray<FVector> Normals 等独立数组。

优势：

- 按需更新：UpdateMeshSection 允许仅传入非空的 Positions 数组，其他属性（如 UV、法线）若未变动则完全不触碰内存。

- 物理数据无需转换：物理碰撞更新 BodyInstance.UpdateTriMeshVertices 直接使用 Section.Data.Positions，无需任何数据转换。


##### 2.6.2.2.游戏线程 (Game Thread) 性能优化

Move Semantics (移动语义):

- 原生 PMC 的 CreateMeshSection 接收 const TArray&，强制进行内存拷贝。

- 优化版提供了 CreateMeshSection(..., FVisMeshData&& MeshData) 重载。通过 MoveTemp，C++ 代码可以直接将计算好的大数组“过继”给组件，耗时接近于 0。

并行计算 (ParallelFor):

- 在 CreateMeshSection 中，对于法线、颜色等数据的默认值填充或类型转换（如 LinearColor 转 FColor），全面采用了 ParallelFor。在大规模网格生成算法（如地形、Marching Cubes）中，这能显著降低主线程卡顿。

## 3.组件对比

VisMesh 针对不同的渲染需求实现了三种核心渲染模式，其性能特性与实现方式对比如下：

| 特性 | VisMeshIndirect (Solid) | VisMeshInstanced (Solid) | VisMeshLine (Wireframe) |
| :--- | :--- | :--- | :--- |
| **几何体** | 实体立方体 (36 Verts) | 实体立方体 (Instance Mesh) | Billboard 矩形 (72 Verts) |
| **更新频率** | 仅动画改变时更新 | 仅动画改变时更新 | **每帧更新** (相机移动时) |
| **相机依赖** | 无 (World Space 固定) | 无 (World Space 固定) | **强依赖** (需实时重算朝向) |
| **绘制方式** | `DrawIndirect` | `DrawInstancedIndirect` | `DrawIndirect` |

* **VisMeshIndirect**: 适用于静态或仅变换的实体渲染，开销较低。
* **VisMeshInstanced**: 利用实例化渲染大量相同的几何体，极大优化 Draw Call。
* **VisMeshLine**: 使用 Billboard 技术模拟线框，虽然视觉效果好，但由于需要根据相机位置实时计算面朝向，CPU/GPU 开销相对较高。