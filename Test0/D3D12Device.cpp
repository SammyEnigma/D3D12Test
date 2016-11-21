// Implementations

#include "stdafx.h"
#include "D3D12Device.h"
#include "D3D12Resources.h"

static bool GSkipValidation = false;

void FInstance::SetupDebugLayer()
{
	if (!GSkipValidation)
	{
		checkD3D12(D3D12GetDebugInterface(IID_PPV_ARGS(&DebugInterface)));
		if (DebugInterface)
		{
			DebugInterface->EnableDebugLayer();
		}
	}
}

void FSwapchain::Create(IDXGIFactory4* DXGI, HWND Hwnd, FDevice& Device, uint32& WindowWidth, uint32& WindowHeight, FDescriptorPool& Pool)
{
	Width = WindowWidth;
	Height = WindowHeight;

	DXGI_SWAP_CHAIN_DESC1 Desc;
	MemZero(Desc);
	Desc.BufferCount = BufferCount;
	Desc.Width = WindowWidth;
	Desc.Height = WindowHeight;
	Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Desc.BufferUsage = /*DXGI_USAGE_BACK_BUFFER | DXGI_USAGE_SHADER_INPUT | */DXGI_USAGE_RENDER_TARGET_OUTPUT;
	Desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	Desc.SampleDesc.Count = 1;
	//Desc.BufferDesc.RefreshRate.Numerator = 60;
	//Desc.BufferDesc.RefreshRate.Denominator = 1;
	//Desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
	//Desc.OutputWindow = Hwnd;
	//Desc.Windowed = true;
	Microsoft::WRL::ComPtr<IDXGISwapChain1> Swapchain1;
	checkD3D12(DXGI->CreateSwapChainForHwnd(Device.Queue.Get(), Hwnd, &Desc, nullptr, nullptr, &Swapchain1));

	checkD3D12(DXGI->MakeWindowAssociation(Hwnd, DXGI_MWA_NO_ALT_ENTER));

	Swapchain1.As(&Swapchain);

	for (uint32 Index = 0; Index < BufferCount; ++Index)
	{
		ID3D12Resource* Resource;
		checkD3D12(Swapchain->GetBuffer(Index, IID_PPV_ARGS(&Resource)));
		Images.push_back(Resource);
		D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle = Pool.CPUAllocateRTV();
		Device.Device->CreateRenderTargetView(Images[Index].Get(), nullptr, RTVHandle);
		ImageViews.push_back(RTVHandle);

#if ENABLE_VULKAN
		ImageViews[Index].Create(Device, Images[Index], VK_IMAGE_VIEW_TYPE_2D, (VkFormat)BACKBUFFER_VIEW_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);
		PresentCompleteSemaphores[Index].Create(Device);
		RenderingSemaphores[Index].Create(Device);
#endif
	}
}

#if ENABLE_VULKAN
void FRenderPass::Create(VkDevice InDevice, const FRenderPassLayout& InLayout)
{
	Device = InDevice;
	Layout = InLayout;

	std::vector<VkAttachmentDescription> Descriptions;
	std::vector<VkAttachmentReference> AttachmentReferences;
	std::vector<VkAttachmentReference> ResolveReferences;
	int32 DepthReferenceIndex = -1;
	/*
	VkAttachmentDescription AttachmentDesc[1 + FRenderPassLayout::MAX_COLOR_ATTACHMENTS];
	VkAttachmentReference AttachmentRef[1 + FRenderPassLayout::MAX_COLOR_ATTACHMENTS];
	VkAttachmentReference ResolveRef[1 + FRenderPassLayout::MAX_COLOR_ATTACHMENTS];
	MemZero(AttachmentDesc);
	MemZero(AttachmentRef);
	MemZero(ResolveRef);

	VkAttachmentDescription* CurrentDesc = AttachmentDesc;
	VkAttachmentReference* CurrentRef = AttachmentRef;
	uint32 Index = 0;

	VkAttachmentReference* DepthRef = nullptr;
	if (Layout.DepthStencilFormat != VK_FORMAT_UNDEFINED)
	{
		CurrentDesc->format = Layout.DepthStencilFormat;
		CurrentDesc->samples = Layout.NumSamples;
		CurrentDesc->loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		CurrentDesc->storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		CurrentDesc->stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		CurrentDesc->stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		CurrentDesc->initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		CurrentDesc->finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		++CurrentDesc;

		DepthRef = CurrentRef;
		CurrentRef->attachment = Index;
		CurrentRef->layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		++CurrentRef;
		++Index;
	}
*/
	for (uint32 Index = 0; Index < Layout.NumColorTargets; ++Index)
	{
		VkAttachmentDescription Desc;
		MemZero(Desc);
		Desc.format = Layout.ColorFormats[Index];
		Desc.samples = Layout.NumSamples;
		Desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		Desc.storeOp = (InLayout.ResolveFormat != VK_FORMAT_UNDEFINED) ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
		Desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		Desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		Desc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		Desc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		Descriptions.push_back(Desc);

		VkAttachmentReference Reference;
		MemZero(Reference);
		Reference.attachment = (uint32)Descriptions.size() - 1;
		Reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		AttachmentReferences.push_back(Reference);
	}

	if (Layout.DepthStencilFormat != VK_FORMAT_UNDEFINED)
	{
		VkAttachmentDescription Desc;
		MemZero(Desc);
		Desc.format = Layout.DepthStencilFormat;
		Desc.samples = Layout.NumSamples;
		Desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		Desc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		Desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		Desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		Desc.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		Desc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		Descriptions.push_back(Desc);

		DepthReferenceIndex = (int32)AttachmentReferences.size();
		VkAttachmentReference Reference;
		MemZero(Reference);
		Reference.attachment = (uint32)Descriptions.size() - 1;
		Reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		AttachmentReferences.push_back(Reference);
	}

	if (InLayout.ResolveFormat != VK_FORMAT_UNDEFINED)
	{
		VkAttachmentDescription Desc;
		MemZero(Desc);
		Desc.format = Layout.ColorFormats[0];
		Desc.samples = VK_SAMPLE_COUNT_1_BIT;
		Desc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		Desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		Desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		Desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		Desc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		Desc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		Descriptions.push_back(Desc);

		VkAttachmentReference Reference;
		MemZero(Reference);
		Reference.attachment = (uint32)Descriptions.size() - 1;
		Reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		ResolveReferences.push_back(Reference);
	}

/*
	if (InLayout.ResolveFormat != VK_FORMAT_UNDEFINED)
	{
		CurrentDesc->format = Layout.ResolveFormat;
		CurrentDesc->samples = VK_SAMPLE_COUNT_1_BIT;
		CurrentDesc->loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		CurrentDesc->storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		CurrentDesc->stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		CurrentDesc->stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		CurrentDesc->initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		CurrentDesc->finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		ResolveRef.attachment = Index;
		ResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		++CurrentDesc;
		++Index;
	}
*/
	VkSubpassDescription Subpass;
	MemZero(Subpass);
	Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	Subpass.colorAttachmentCount = Layout.NumColorTargets;
	Subpass.pColorAttachments = &AttachmentReferences[0];
	Subpass.pResolveAttachments = (InLayout.ResolveFormat != VK_FORMAT_UNDEFINED) ? &ResolveReferences[0] : nullptr;
	Subpass.pDepthStencilAttachment = DepthReferenceIndex != -1 ? &AttachmentReferences[DepthReferenceIndex] : nullptr;

	if (InLayout.ResolveFormat != VK_FORMAT_UNDEFINED)
	{
		//Subpass.pResolveAttachments = &ResolveRef;
	}

	VkRenderPassCreateInfo RenderPassInfo;
	MemZero(RenderPassInfo);
	RenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	RenderPassInfo.attachmentCount = (uint32)Descriptions.size();
	RenderPassInfo.pAttachments = &Descriptions[0];
	RenderPassInfo.subpassCount = 1;
	RenderPassInfo.pSubpasses = &Subpass;

	checkVk(vkCreateRenderPass(Device, &RenderPassInfo, nullptr, &RenderPass));
}
#endif

FGfxPipeline::FGfxPipeline()
{
#if ENABLE_VULKAN
	MemZero(IAInfo);
	IAInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	IAInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	//IAInfo.primitiveRestartEnable = VK_FALSE;

	MemZero(Viewport);
	//Viewport.x = 0;
	//Viewport.y = 0;
	//Viewport.width = (float)Width;
	//Viewport.height = (float)Height;
	//Viewport.minDepth = 0;
	Viewport.maxDepth = 1;

	MemZero(Scissor);
	//scissors.offset = { 0, 0 };
	//Scissor.extent = { Width, Height };

	MemZero(ViewportInfo);
	ViewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	ViewportInfo.viewportCount = 1;
	ViewportInfo.pViewports = &Viewport;
	ViewportInfo.scissorCount = 1;
	ViewportInfo.pScissors = &Scissor;
#endif
	Desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	Desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	Desc.RasterizerState.FrontCounterClockwise = FALSE;
	Desc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	Desc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	Desc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	Desc.RasterizerState.DepthClipEnable = TRUE;
	Desc.RasterizerState.MultisampleEnable = FALSE;
	Desc.RasterizerState.AntialiasedLineEnable = FALSE;
	Desc.RasterizerState.ForcedSampleCount = 0;
	Desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
#if ENABLE_VULKAN
	RSInfo.lineWidth = 1;

	MemZero(MSInfo);
	MSInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	MSInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	//MSInfo.sampleShadingEnable = VK_FALSE;
	//MSInfo.minSampleShading = 0;
	//MSInfo.pSampleMask = NULL;
	//MSInfo.alphaToCoverageEnable = VK_FALSE;
	//MSInfo.alphaToOneEnable = VK_FALSE;

	MemZero(Stencil);
	Stencil.failOp = VK_STENCIL_OP_KEEP;
	Stencil.passOp = VK_STENCIL_OP_KEEP;
	Stencil.depthFailOp = VK_STENCIL_OP_KEEP;
	Stencil.compareOp = VK_COMPARE_OP_ALWAYS;
	Stencil.compareMask = 0xff;
	Stencil.writeMask = 0xff;
	//Stencil.reference = 0;

	MemZero(DSInfo);
	DSInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	DSInfo.depthTestEnable = VK_TRUE;
	DSInfo.depthWriteEnable = VK_TRUE;
	DSInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	//DSInfo.depthBoundsTestEnable = VK_FALSE;
	//DSInfo.stencilTestEnable = VK_FALSE;
	DSInfo.front = Stencil;
	//DSInfo.front.passOp = VK_STENCIL_OP_INCREMENT_AND_WRAP;
	DSInfo.back = Stencil;
	//DSInfo.back.passOp = VK_STENCIL_OP_DECREMENT_AND_WRAP;
	//DSInfo.minDepthBounds = 0;
	//DSInfo.maxDepthBounds = 0;
#endif

	Desc.BlendState.AlphaToCoverageEnable = FALSE;
	Desc.BlendState.IndependentBlendEnable = FALSE;
	const D3D12_RENDER_TARGET_BLEND_DESC DefaultRenderTargetBlendDesc =
	{
		FALSE,FALSE,
		D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
		D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
		D3D12_LOGIC_OP_NOOP,
		D3D12_COLOR_WRITE_ENABLE_ALL,
	};
	for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
	{
		Desc.BlendState.RenderTarget[i] = DefaultRenderTargetBlendDesc;
	}
#if ENABLE_VULKAN
	MemZero(CBInfo);
	CBInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	//CBInfo.logicOpEnable = VK_FALSE;
	CBInfo.logicOp = VK_LOGIC_OP_CLEAR;
	CBInfo.attachmentCount = 1;
	CBInfo.pAttachments = &AttachState;
	//CBInfo.blendConstants[0] = 0.0;
	//CBInfo.blendConstants[1] = 0.0;
	//CBInfo.blendConstants[2] = 0.0;
	//CBInfo.blendConstants[3] = 0.0;

	Dynamic[0] = VK_DYNAMIC_STATE_VIEWPORT;
	Dynamic[1] = VK_DYNAMIC_STATE_SCISSOR;

	MemZero(DynamicInfo);
	DynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	DynamicInfo.dynamicStateCount = 2;
	DynamicInfo.pDynamicStates = Dynamic;
#endif
}

void FGfxPipeline::Create(FDevice* Device, FGfxPSO* PSO, FVertexFormat* VertexFormat
#if ENABLE_VULKAN
	, uint32 Width, uint32 Height, FRenderPass* RenderPass
#endif
)
{
	Desc.pRootSignature = PSO->RootSignature.Get();

	Desc.VS.pShaderBytecode = PSO->VS.UCode->GetBufferPointer();
	Desc.VS.BytecodeLength = PSO->VS.UCode->GetBufferSize();
	Desc.PS.pShaderBytecode = PSO->PS.UCode->GetBufferPointer();
	Desc.PS.BytecodeLength = PSO->PS.UCode->GetBufferSize();

	VertexFormat->SetCreateInfo(Desc);

#if ENABLE_VULKAN
	//Viewport.x = 0;
	//Viewport.y = 0;
	Viewport.width = (float)Width;
	Viewport.height = (float)Height;
	//Viewport.minDepth = 0;
	//Viewport.maxDepth = 1;

	MSInfo.rasterizationSamples = RenderPass->GetLayout().GetNumSamples();

	//scissors.offset = { 0, 0 };
	Scissor.extent = { Width, Height };

	VkGraphicsPipelineCreateInfo PipelineInfo;
	MemZero(PipelineInfo);
	PipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	PipelineInfo.stageCount = (uint32)ShaderStages.size();
	PipelineInfo.pStages = &ShaderStages[0];
	PipelineInfo.pVertexInputState = &VIInfo;
	PipelineInfo.pInputAssemblyState = &IAInfo;
	//PipelineInfo.pTessellationState = NULL;
	PipelineInfo.pViewportState = &ViewportInfo;
	PipelineInfo.pRasterizationState = &RSInfo;
	PipelineInfo.pMultisampleState = &MSInfo;
	PipelineInfo.pDepthStencilState = &DSInfo;
	PipelineInfo.pColorBlendState = &CBInfo;
	PipelineInfo.pDynamicState = &DynamicInfo;
	PipelineInfo.layout = PipelineLayout;
#endif

	Desc.DepthStencilState.DepthEnable = FALSE;
	Desc.DepthStencilState.StencilEnable = FALSE;
	Desc.SampleMask = UINT_MAX;
	Desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	Desc.NumRenderTargets = 1;
	Desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	Desc.SampleDesc.Count = 1;
	checkD3D12(Device->Device->CreateGraphicsPipelineState(&Desc, IID_PPV_ARGS(&PipelineState)));
}

#if ENABLE_VULKAN
FMemPage::FMemPage(VkDevice InDevice, VkDeviceSize Size, uint32 InMemTypeIndex, bool bInMapped)
	: Allocation(InDevice, Size, MemTypeIndex, bInMapped)
	, MemTypeIndex(InMemTypeIndex)
{
	FRange Block;
	Block.Begin = 0;
	Block.End = Size;
	FreeList.push_back(Block);
}

FMemPage::~FMemPage()
{
	check(FreeList.size() == 1);
	check(SubAllocations.empty());
	Allocation.Destroy();
}

FMemSubAlloc* FMemPage::TryAlloc(uint64 Size, uint64 Alignment)
{
	for (auto& Range : FreeList)
	{
		uint64 AlignedOffset = Align(Range.Begin, Alignment);
		if (AlignedOffset + Size <= Range.End)
		{
			auto* SubAlloc = new FMemSubAlloc(Range.Begin, AlignedOffset, Size, this);
			SubAllocations.push_back(SubAlloc);
			Range.Begin = AlignedOffset + Size;
			return SubAlloc;
		}
	}

	return nullptr;
}

void FMemPage::Release(FMemSubAlloc* SubAlloc)
{
	FRange NewRange;
	NewRange.Begin = SubAlloc->AllocatedOffset;
	NewRange.End = SubAlloc->AllocatedOffset + SubAlloc->Size;
	FreeList.push_back(NewRange);
	SubAllocations.remove(SubAlloc);
	delete SubAlloc;

	{
		std::sort(FreeList.begin(), FreeList.end(),
			[](const FRange& Left, const FRange& Right)
		{
			return Left.Begin < Right.Begin;
		});

		for (uint32 Index = (uint32)FreeList.size() - 1; Index > 0; --Index)
		{
			auto& Current = FreeList[Index];
			auto& Prev = FreeList[Index - 1];
			if (Current.Begin == Prev.End)
			{
				Prev.End = Current.End;
				for (uint32 SubIndex = Index; SubIndex < FreeList.size() - 1; ++SubIndex)
				{
					FreeList[SubIndex] = FreeList[SubIndex + 1];
				}
				FreeList.resize(FreeList.size() - 1);
			}
		}
	}
}

void FCmdBuffer::BeginRenderPass(VkRenderPass RenderPass, const FFramebuffer& Framebuffer, bool bHasSecondary)
{
	check(State == EState::Begun);

	static uint32 N = 0;
	N = (N + 1) % 256;

	VkClearValue ClearValues[2] = { { N / 255.0f, 1.0f, 0.0f, 1.0f },{ 1.0, 0.0 } };
	ClearValues[1].depthStencil.depth = 1.0f;
	ClearValues[1].depthStencil.stencil = 0;
	VkRenderPassBeginInfo BeginInfo = {};
	BeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	BeginInfo.renderPass = RenderPass;
	BeginInfo.framebuffer = Framebuffer.Framebuffer;
	BeginInfo.renderArea = { 0, 0, Framebuffer.Width, Framebuffer.Height };
	BeginInfo.clearValueCount = 2;
	BeginInfo.pClearValues = ClearValues;
	vkCmdBeginRenderPass(CmdBuffer, &BeginInfo, bHasSecondary ? VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS : VK_SUBPASS_CONTENTS_INLINE);

	State = EState::InsideRenderPass;
}
#endif

/*
void FSwapchain::ClearAndTransitionToPresent(FDevice& Device, FCmdBuffer* CmdBuffer, FDescriptorPool* DescriptorPool)
{
	for (uint32 Index = 0; Index < (uint32)Images.size(); ++Index)
	{
		ResourceBarrier(CmdBuffer, Images[Index].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
#if ENABLE_VULKAN
		ImageBarrier(CmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, Images[Index], VK_IMAGE_LAYOUT_UNDEFINED, 0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
#endif
		//D3D12_CPU_DESCRIPTOR_HANDLE RTView = DescriptorPool->AllocateCPURTV();
		//Device.Device->CreateRenderTargetView(Images[Index].Get(), nullptr, RTView);

		//CmdBuffer->CommandList->OMSetRenderTargets(1, &RTView, false, nullptr);
		float Color[4] = {1, 0, 0, 1};
		//CmdBuffer->CommandList->ClearRenderTargetView(RTView, Color, 0, nullptr);
#if ENABLE_VULKAN
		ImageBarrier(CmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, Images[Index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_MEMORY_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
#endif
	}
}
*/

void FImageView::Create(FDevice& InDevice, FImage& Image, DXGI_FORMAT InFormat, FDescriptorPool& Pool)
{
	MemZero(Desc);
	Desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	Desc.Format = InFormat;
	Desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	Desc.Texture2D.MipLevels = 1;
	CPUHandle = Pool.CPUAllocateCSU();
	InDevice.Device->CreateShaderResourceView(Image.Alloc->Resource.Get(), &Desc, CPUHandle);

#if ENABLE_VULKAN
	Format = InFormat;

	VkImageViewCreateInfo Info;
	MemZero(Info);
	Info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	Info.image = Image;
	Info.viewType = ViewType;
	Info.format = Format;
	Info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	Info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	Info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	Info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	Info.subresourceRange.aspectMask = ImageAspect;
	Info.subresourceRange.levelCount = 1;
	Info.subresourceRange.layerCount = 1;
	checkVk(vkCreateImageView(Device, &Info, nullptr, &ImageView));
#endif
	GPUHandle = Pool.GPUAllocateCSU();
}
