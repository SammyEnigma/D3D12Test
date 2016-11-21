
#pragma once

#include "D3D12Device.h"
#include "D3D12Mem.h"

struct FBuffer
{
	void Create(FDevice& InDevice, uint64 InSize, FMemManager& MemMgr, bool bUploadCPU)
	{
		Size = InSize;

		Alloc = MemMgr.AllocBuffer(InDevice, InSize, bUploadCPU);
	}

	void Destroy()
	{
#if ENABLE_VULKAN
		SubAlloc->Release();
#endif
	}

	void* GetMappedData()
	{
		check(Alloc->MappedData);
		return Alloc->MappedData;
	}

	uint64 GetSize() const
	{
		return Size;
	}

	FBufferAllocation* Alloc = nullptr;
	uint64 Size = 0;
};

#if ENABLE_VULKAN
struct FIndexBuffer
{
	void Create(VkDevice InDevice, uint32 InNumIndices, VkIndexType InIndexType, FMemManager* MemMgr,
		VkBufferUsageFlags InUsageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VkMemoryPropertyFlags MemPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	{
		VkBufferUsageFlags UsageFlags = InUsageFlags | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		check(InIndexType == VK_INDEX_TYPE_UINT16 || InIndexType == VK_INDEX_TYPE_UINT32);
		IndexType = InIndexType;
		NumIndices = InNumIndices;
		uint32 IndexSize = InIndexType == VK_INDEX_TYPE_UINT16 ? 2 : 4;
		Buffer.Create(InDevice, InNumIndices * IndexSize, UsageFlags, MemPropertyFlags, MemMgr);
	}

	void Destroy()
	{
		Buffer.Destroy();
	}

	FBuffer Buffer;
	uint32 NumIndices = 0;
	VkIndexType IndexType = VK_INDEX_TYPE_UINT32;
};

inline void CmdBind(FCmdBuffer* CmdBuffer, FIndexBuffer* IB)
{
	vkCmdBindIndexBuffer(CmdBuffer->CmdBuffer, IB->Buffer.Buffer, IB->Buffer.GetBindOffset(), IB->IndexType);
}
#endif
struct FVertexBuffer
{
	void Create(FDevice& InDevice, uint32 Stride, uint32 Size, FMemManager& MemMgr, bool bUploadCPU)
	{
		Buffer.Create(InDevice, Size, MemMgr, bUploadCPU);
		View.BufferLocation = Buffer.Alloc->Resource->GetGPUVirtualAddress();
		View.StrideInBytes = Stride;
		View.SizeInBytes = Size;
	}

	void Destroy()
	{
		Buffer.Destroy();
	}

	FBuffer Buffer;

	D3D12_VERTEX_BUFFER_VIEW View;
};

inline void CmdBind(FCmdBuffer* CmdBuffer, FVertexBuffer* VB)
{
	CmdBuffer->CommandList->IASetVertexBuffers(0, 1, &VB->View);
}

template <typename TStruct>
struct FUniformBuffer
{
	void Create(FDevice& InDevice, struct FDescriptorPool& Pool, FMemManager& MemMgr, bool bUploadCPU);

	TStruct* GetMappedData()
	{
		return (TStruct*)Buffer.GetMappedData();
	}

	void Destroy()
	{
		Buffer.Destroy();
	}

	D3D12_CONSTANT_BUFFER_VIEW_DESC View;
	D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle = {0};
	FBuffer Buffer;
};

struct FImage
{
	//Microsoft::WRL::ComPtr<ID3D12Resource> Texture;
	FResourceAllocation* Alloc = nullptr;

	void Create(FDevice& InDevice, uint32 InWidth, uint32 InHeight, DXGI_FORMAT InFormat, FMemManager& MemMgr
#if ENABLE_VULKAN
		, VkImageUsageFlags UsageFlags, VkMemoryPropertyFlags MemPropertyFlags, uint32 InNumMips, VkSampleCountFlagBits InSamples
#endif
	)
	{
		//Device = InDevice;
		Width = InWidth;
		Height = InHeight;
		Format = InFormat;
#if ENABLE_VULKAN
		NumMips = InNumMips;
		Samples = InSamples;
#endif
		Alloc = MemMgr.AllocTexture2D(InDevice, Width, Height, Format, false);

#if ENABLE_VULKAN
		VkImageCreateInfo ImageInfo;
		MemZero(ImageInfo);
		ImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		ImageInfo.imageType = VK_IMAGE_TYPE_2D;
		ImageInfo.format = Format;
		ImageInfo.extent.width = Width;
		ImageInfo.extent.height = Height;
		ImageInfo.extent.depth = 1;
		ImageInfo.mipLevels = NumMips;
		ImageInfo.arrayLayers = 1;
		ImageInfo.samples = Samples;
		ImageInfo.tiling = (MemPropertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0 ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
		ImageInfo.usage = UsageFlags;
		ImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		checkVk(vkCreateImage(Device, &ImageInfo, nullptr, &Image));

		vkGetImageMemoryRequirements(Device, Image, &Reqs);

		SubAlloc = MemMgr->Alloc(Reqs, MemPropertyFlags, true);

		vkBindImageMemory(Device, Image, SubAlloc->GetHandle(), SubAlloc->GetBindOffset());
#endif
	}

	void Destroy()
	{
#if ENABLE_VULKAN
		vkDestroyImage(Device, Image, nullptr);
		Image = VK_NULL_HANDLE;

		SubAlloc->Release();
#endif
	}

#if ENABLE_VULKAN
	void* GetMappedData()
	{
		return SubAlloc->GetMappedData();
	}

	uint64 GetBindOffset()
	{
		return SubAlloc->GetBindOffset();
	}

	VkDevice Device;
	VkImage Image = VK_NULL_HANDLE;
#endif
	uint32 Width = 0;
	uint32 Height = 0;
	DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
#if ENABLE_VULKAN
	uint32 NumMips = 0;
	VkSampleCountFlagBits Samples = VK_SAMPLE_COUNT_1_BIT;
	VkMemoryRequirements Reqs;
	FMemSubAlloc* SubAlloc = nullptr;
#endif
};

struct FImageView
{
	D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle = {0};
	D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle ={0};
	D3D12_SHADER_RESOURCE_VIEW_DESC Desc;

	void Create(FDevice& InDevice, FImage& Image, DXGI_FORMAT InFormat, FDescriptorPool& Pool);

	void Destroy()
	{
#if ENABLE_VULKAN
		vkDestroyImageView(Device, ImageView, nullptr);
		ImageView = VK_NULL_HANDLE;
#endif
	}
};

struct FStagingBuffer : public FBuffer
{
	FCmdBuffer* CmdBuffer = nullptr;
	uint64 FenceCounter = 0;

	void SetFence(FCmdBuffer* InCmdBuffer)
	{
		CmdBuffer = InCmdBuffer;
		FenceCounter = InCmdBuffer->Fence.FenceSignaledCounter;
	}

	bool IsSignaled() const
	{
		check(CmdBuffer);
		return FenceCounter < CmdBuffer->Fence.FenceSignaledCounter;
	}
};

struct FStagingManager
{
	FMemManager* MemMgr = nullptr;
	void Create(FDevice& InDevice, FMemManager* InMemMgr)
	{
#if ENABLE_VULKAN
		Device = InDevice;
#endif
		MemMgr = InMemMgr;
	}

	void Destroy()
	{
		Update();
		for (auto& Entry : Entries)
		{
			check(Entry.bFree);
			Entry.Buffer->Destroy();
			delete Entry.Buffer;
		}
	}

	FStagingBuffer* RequestUploadBuffer(FDevice& InDevice, uint32 Size, FMemManager& MemMgr)
	{
		Update();
		for (auto& Entry : Entries)
		{
			if (Entry.bFree && Entry.Buffer->Size == Size)
			{
				Entry.bFree = false;
				Entry.Buffer->CmdBuffer = nullptr;
				Entry.Buffer->FenceCounter = 0;
				return Entry.Buffer;
			}
		}

		auto* Buffer = new FStagingBuffer;
		Buffer->Create(InDevice, Size, MemMgr, true);
		FEntry Entry;
		Entry.Buffer = Buffer;
		Entry.bFree = false;
		Entries.push_back(Entry);
		return Buffer;
	}

	FStagingBuffer* RequestUploadBufferForImage(FDevice& InDevice, const FImage* Image, FMemManager& MemMgr)
	{
		uint32 Size = Image->Width * Image->Height * GetFormatBitsPerPixel(Image->Format) / 8;
		return RequestUploadBuffer(InDevice, Size, MemMgr);
	}

	void Update()
	{
		for (auto& Entry : Entries)
		{
			if (!Entry.bFree)
			{
				if (Entry.Buffer->IsSignaled())
				{
					Entry.bFree = true;
				}
			}
		}
	}

	struct FEntry
	{
		FStagingBuffer* Buffer = nullptr;
		bool bFree = false;
	};
	std::vector<FEntry> Entries;
};

#if ENABLE_VULKAN
inline bool IsDepthOrStencilFormat(VkFormat Format)
{
	switch (Format)
	{
	case VK_FORMAT_D16_UNORM:
	case VK_FORMAT_X8_D24_UNORM_PACK32:
	case VK_FORMAT_D32_SFLOAT:
	case VK_FORMAT_S8_UINT:
	case VK_FORMAT_D16_UNORM_S8_UINT:
	case VK_FORMAT_D24_UNORM_S8_UINT:
	case VK_FORMAT_D32_SFLOAT_S8_UINT:
		return true;

	default:
		return false;
	}
}

inline VkImageAspectFlags GetImageAspectFlags(VkFormat Format)
{
	switch (Format)
	{
	case VK_FORMAT_D16_UNORM:
	case VK_FORMAT_X8_D24_UNORM_PACK32:
	case VK_FORMAT_D32_SFLOAT:
		return VK_IMAGE_ASPECT_DEPTH_BIT;

	case VK_FORMAT_S8_UINT:
		return VK_IMAGE_ASPECT_STENCIL_BIT;

	case VK_FORMAT_D16_UNORM_S8_UINT:
	case VK_FORMAT_D24_UNORM_S8_UINT:
	case VK_FORMAT_D32_SFLOAT_S8_UINT:
		return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

	default:
		return VK_IMAGE_ASPECT_COLOR_BIT;
	}
}
#endif

struct FImage2DWithView
{
	void Create(FDevice& InDevice, uint32 InWidth, uint32 InHeight, DXGI_FORMAT Format, FDescriptorPool& Pool, FMemManager& MemMgr
#if ENABLE_VULKAN
		, VkImageUsageFlags UsageFlags, VkMemoryPropertyFlags MemPropertyFlags, uint32 InNumMips = 1, VkSampleCountFlagBits Samples = VK_SAMPLE_COUNT_1_BIT
#endif
	)
	{
		Image.Create(InDevice, InWidth, InHeight, Format, MemMgr
#if ENABLE_VULKAN
			, UsageFlags, MemPropertyFlags, InNumMips, Samples
#endif
		);

		ImageView.Create(InDevice, Image, Format, Pool);
	}

	void Destroy()
	{
		ImageView.Destroy();
		Image.Destroy();
	}

	FImage Image;
	FImageView ImageView;
#if ENABLE_VULKAN

	inline VkFormat GetFormat() const
	{
		return ImageView.Format;
	}

	inline VkImage GetImage() const
	{
		return Image.Image;
	}

	inline VkImageView GetImageView() const
	{
		return ImageView.ImageView;
	}
#endif

	inline uint32 GetWidth() const
	{
		return Image.Width;
	}

	inline uint32 GetHeight() const
	{
		return Image.Height;
	}
};

struct FSampler
{
	D3D12_STATIC_SAMPLER_DESC SamplerDesc;
#if ENABLE_VULKAN
	VkSampler Sampler = VK_NULL_HANDLE;
	VkDevice Device = VK_NULL_HANDLE;
#endif
	void Create(FDevice& InDevice)
	{
		MemZero(SamplerDesc);
		SamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		SamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		SamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		SamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		SamplerDesc.MipLODBias = 0;
		SamplerDesc.MaxAnisotropy = 0;
		SamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		SamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		SamplerDesc.MinLOD = 0.0f;
		SamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		SamplerDesc.ShaderRegister = 0;
		SamplerDesc.RegisterSpace = 0;
		SamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		SamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;

#if ENABLE_VULKAN
		checkVk(vkCreateSampler(Device, &Info, nullptr, &Sampler));
#endif
	}

	void Destroy()
	{
#if ENABLE_VULKAN
		vkDestroySampler(Device, Sampler, nullptr);
		Sampler = VK_NULL_HANDLE;
#endif
	}
};

struct FShader
{
	bool Create(const char* Filename, FDevice& Device, const char* Profile)
	{
/*
		std::wstring FilenameW;
		{
			while (*Filename)
			{
				FilenameW += *Filename++;
			}
		}
		uint32 Flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
		checkD3D12(D3DCompileFromFile(FilenameW.c_str(), nullptr, nullptr, (LPCSTR)"Main", (LPCSTR)Profile, Flags, 0, &UCode, nullptr));
		return true;
*/
		SourceCode = LoadFile(Filename);
		if (SourceCode.empty())
		{
			return false;
		}

		uint32 Flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
		Microsoft::WRL::ComPtr<ID3DBlob> Errors;
		if (FAILED(D3DCompile(&SourceCode[0], SourceCode.size(), nullptr, nullptr, nullptr, (LPCSTR)"Main", (LPCSTR)Profile, 0, 0, &UCode, &Errors)))
		{
			char* ErrorA = (char*)Errors->GetBufferPointer();
			::OutputDebugStringA(ErrorA);
			::OutputDebugStringA("\n");
			check(0);
		}
		return true;
	}

	void Destroy()
	{
	}

	std::vector<char> SourceCode;
	Microsoft::WRL::ComPtr<ID3DBlob> UCode;
};

struct FPSO
{
	Microsoft::WRL::ComPtr<ID3D12RootSignature> RootSignature;

	virtual void SetupLayoutBindings(std::vector<D3D12_ROOT_PARAMETER>& OutRootParameters, std::vector<D3D12_DESCRIPTOR_RANGE>& OutRanges)
	{
	}

	virtual void Destroy()
	{
#if ENABLE_VULKAN
		if (DSLayout != VK_NULL_HANDLE)
		{
			vkDestroyDescriptorSetLayout(Device, DSLayout, nullptr);
		}
#endif
	}

	void CreateDescriptorSetLayout(FDevice& Device)
	{
		std::vector<D3D12_ROOT_PARAMETER> RootParameters;
		std::vector<D3D12_DESCRIPTOR_RANGE> Ranges;
		SetupLayoutBindings(RootParameters, Ranges);

		D3D12_ROOT_SIGNATURE_DESC Desc;
		MemZero(Desc);
		Desc.NumParameters = (uint32)RootParameters.size();
		Desc.pParameters = RootParameters.empty() ? nullptr : &RootParameters[0];
		Desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		Microsoft::WRL::ComPtr<ID3DBlob> Signature;
		Microsoft::WRL::ComPtr<ID3DBlob> Error;
		checkD3D12(D3D12SerializeRootSignature(&Desc, D3D_ROOT_SIGNATURE_VERSION_1, &Signature, &Error));
		checkD3D12(Device.Device->CreateRootSignature(0, Signature->GetBufferPointer(), Signature->GetBufferSize(), IID_PPV_ARGS(&RootSignature)));
	}

#if ENABLE_VULKAN
	virtual void SetupShaderStages(std::vector<VkPipelineShaderStageCreateInfo>& OutShaderStages)
	{
	}
#endif
};

struct FGfxPSO : public FPSO
{
	FShader VS;
	FShader PS;

	virtual void Destroy() override
	{
		FPSO::Destroy();
		PS.Destroy();
		VS.Destroy();
	}

	bool CreateVSPS(FDevice& Device, const char* VSFilename, const char* PSFilename)
	{
		if (!VS.Create(VSFilename, Device, "vs_5_0"))
		{
			return false;
		}

		if (!PS.Create(PSFilename, Device, "ps_5_0"))
		{
			return false;
		}

		CreateDescriptorSetLayout(Device);
		return true;
	}

	inline void AddRootParam(std::vector<D3D12_ROOT_PARAMETER>& OutRootParameters, std::vector<D3D12_DESCRIPTOR_RANGE>& Ranges, int32 Binding, D3D12_SHADER_VISIBILITY Stage)
	{
		D3D12_ROOT_PARAMETER RootParam;
		MemZero(RootParam);
		RootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		RootParam.ShaderVisibility = Stage;
		RootParam.DescriptorTable.NumDescriptorRanges = 1;
		RootParam.DescriptorTable.pDescriptorRanges = &Ranges[Binding];
		OutRootParameters.push_back(RootParam);
	}

	inline void AddRange(std::vector<D3D12_DESCRIPTOR_RANGE>& OutRanges, int32 Binding, D3D12_DESCRIPTOR_RANGE_TYPE RangeType)
	{
		auto RangeIndex = OutRanges.size();
		D3D12_DESCRIPTOR_RANGE Range;
		MemZero(Range);
		Range.RangeType = RangeType;
		Range.NumDescriptors = 1;
		Range.BaseShaderRegister = Binding;
		Range.RegisterSpace = 0;
		//Range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
		Range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		OutRanges.push_back(Range);
	}
#if ENABLE_VULKAN
	virtual void SetupShaderStages(std::vector<VkPipelineShaderStageCreateInfo>& OutShaderStages)
	{
		VkPipelineShaderStageCreateInfo Info;
		MemZero(Info);
		Info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		Info.stage = VK_SHADER_STAGE_VERTEX_BIT;
		Info.module = VS.ShaderModule;
		Info.pName = "main";
		OutShaderStages.push_back(Info);

		if (PS.ShaderModule != VK_NULL_HANDLE)
		{
			MemZero(Info);
			Info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			Info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			Info.module = PS.ShaderModule;
			Info.pName = "main";
			OutShaderStages.push_back(Info);
		}
	}
#endif
};

struct FVertexFormat
{
	std::vector<D3D12_INPUT_CLASSIFICATION> InputRates;
	std::vector<uint32> Strides;
	std::vector<D3D12_INPUT_ELEMENT_DESC> VertexAttributes;
	std::vector<std::string> SemanticNames;

	void AddVertexBuffer(uint32 Binding, uint32 Stride, D3D12_INPUT_CLASSIFICATION InputRate)
	{
		check(InputRates.size() == Binding);
		InputRates.push_back(InputRate);
		Strides.push_back(Stride);
	}

	void AddVertexAttribute(const char* InSemanticName, uint32 Binding, uint32 Location, DXGI_FORMAT Format, uint32 Offset)
	{
		D3D12_INPUT_ELEMENT_DESC VIADesc;
		MemZero(VIADesc);
		VIADesc.Format = Format;
		VIADesc.InputSlot = Binding;
		VIADesc.InputSlotClass = InputRates[Binding];
		VIADesc.AlignedByteOffset = Offset;
		VIADesc.SemanticName = InSemanticName;
		VertexAttributes.push_back(VIADesc);
		check(InSemanticName);
		SemanticNames.push_back(InSemanticName);
	}

#if ENABLE_VULKAN
	VkPipelineVertexInputStateCreateInfo GetCreateInfo()
	{
		VkPipelineVertexInputStateCreateInfo VIInfo;
		MemZero(VIInfo);
		VIInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		VIInfo.vertexBindingDescriptionCount = (uint32)VertexBuffers.size();
		VIInfo.pVertexBindingDescriptions = VertexBuffers.empty() ? nullptr : &VertexBuffers[0];
		VIInfo.vertexAttributeDescriptionCount = (uint32)VertexAttributes.size();
		VIInfo.pVertexAttributeDescriptions = VertexAttributes.empty() ? nullptr : &VertexAttributes[0];

		return VIInfo;
	}
#endif

	void SetCreateInfo(D3D12_GRAPHICS_PIPELINE_STATE_DESC& OutDesc)
	{
		OutDesc.InputLayout.NumElements = (uint32)VertexAttributes.size();
		OutDesc.InputLayout.pInputElementDescs = VertexAttributes.empty() ? 0 : &VertexAttributes[0];
	}
};

class FGfxPSOLayout
{
public:
	FGfxPSOLayout(FGfxPSO* InGfxPSO, FVertexFormat* InVF, 
#if ENABLE_VULKAN
		uint32 InWidth, uint32 InHeight, VkRenderPass InRenderPass, 
#endif
		bool bInWireframe)
		: GfxPSO(InGfxPSO)
		, VF(InVF)
#if ENABLE_VULKAN
		, Width(InWidth)
		, Height(InHeight)
		, RenderPass(InRenderPass)
#endif
		, bWireframe(bInWireframe)
	{
	}

	friend inline bool operator < (const FGfxPSOLayout& A, const FGfxPSOLayout& B)
	{
		return 
#if ENABLE_VULKAN
			A.Width < B.Width || A.Height < B.Height || 
#endif
			A.GfxPSO < B.GfxPSO || A.VF < B.VF || 
#if ENABLE_VULKAN
			A.RenderPass < B.RenderPass || 
#endif
			A.bWireframe < B.bWireframe;
	}
protected:
	FGfxPSO* GfxPSO;
	FVertexFormat* VF;
#if ENABLE_VULKAN
	uint32 Width;
	uint32 Height;
	VkRenderPass RenderPass;
#endif
	bool bWireframe;
};

#if ENABLE_VULKAN
struct FComputePSO : public FPSO
{
	FShader CS;

	virtual void Destroy(VkDevice Device) override
	{
		FPSO::Destroy(Device);
		CS.Destroy(Device);
	}

	bool Create(VkDevice Device, const char* CSFilename)
	{
		if (!CS.Create(CSFilename, Device))
		{
			return false;
		}

		CreateDescriptorSetLayout(Device);
		return true;
	}

	inline void AddBinding(std::vector<VkDescriptorSetLayoutBinding>& OutBindings, int32 Binding, VkDescriptorType DescType, uint32 NumDescriptors = 1)
	{
		VkDescriptorSetLayoutBinding NewBinding;
		MemZero(NewBinding);
		NewBinding.binding = Binding;
		NewBinding.descriptorType = DescType;
		NewBinding.descriptorCount = NumDescriptors;
		NewBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		OutBindings.push_back(NewBinding);
	}

	virtual void SetupShaderStages(std::vector<VkPipelineShaderStageCreateInfo>& OutShaderStages)
	{
		VkPipelineShaderStageCreateInfo Info;
		MemZero(Info);
		Info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		Info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		Info.module = CS.ShaderModule;
		Info.pName = "main";
		OutShaderStages.push_back(Info);
	}
};
#endif
struct FBasePipeline
{
	Microsoft::WRL::ComPtr<ID3D12PipelineState> PipelineState;
#if ENABLE_VULKAN
	void Destroy(VkDevice Device)
	{
		vkDestroyPipeline(Device, Pipeline, nullptr);
		Pipeline = VK_NULL_HANDLE;

		vkDestroyPipelineLayout(Device, PipelineLayout, nullptr);
		PipelineLayout = VK_NULL_HANDLE;
	}
#endif
};

struct FDescriptorPool
{
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> RTVHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CSUHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE RTVCPUStart = {0};
	D3D12_GPU_DESCRIPTOR_HANDLE RTVGPUStart = {0};
	SIZE_T RTVDescriptorSize = 0;

	D3D12_CPU_DESCRIPTOR_HANDLE CSUCPUStart ={0};
	D3D12_GPU_DESCRIPTOR_HANDLE CSUGPUStart ={0};
	SIZE_T CSUDescriptorSize = 0;

	void Create(FDevice& InDevice)
	{
		D3D12_DESCRIPTOR_HEAP_DESC HeapDesc;
		MemZero(HeapDesc);
		//HeapDesc.NodeMask = 1;
		HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE/*D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE*/;
		{
			HeapDesc.NumDescriptors = 4096;
			HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			checkD3D12(InDevice.Device->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&RTVHeap)));
		}

		{
			HeapDesc.NumDescriptors = 32768;
			HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			checkD3D12(InDevice.Device->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&CSUHeap)));
		}

		RTVCPUStart = RTVHeap->GetCPUDescriptorHandleForHeapStart();
		RTVGPUStart = RTVHeap->GetGPUDescriptorHandleForHeapStart();
		RTVDescriptorSize = InDevice.Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		CSUCPUStart = CSUHeap->GetCPUDescriptorHandleForHeapStart();
		CSUGPUStart = CSUHeap->GetGPUDescriptorHandleForHeapStart();
		CSUDescriptorSize = InDevice.Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

#if ENABLE_VULKAN
		AddPool(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 32768);
		AddPool(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 16384);
		AddPool(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 32768);
		AddPool(VK_DESCRIPTOR_TYPE_SAMPLER, 32768);
		AddPool(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 32768);
		AddPool(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 16384);
		AddPool(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 16384);
#endif
	}

	void Destroy()
	{
#if ENABLE_VULKAN
		vkDestroyDescriptorPool(Device, Pool, nullptr);
		Pool = VK_NULL_HANDLE;
#endif
		CSUHeap = nullptr;
		RTVHeap = nullptr;
	}

#if ENABLE_VULKAN
	VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetLayout DSLayout)
	{
	}
#endif

	D3D12_CPU_DESCRIPTOR_HANDLE CPUAllocateRTV()
	{
		D3D12_CPU_DESCRIPTOR_HANDLE NewHandle = RTVCPUStart;
		RTVCPUStart.ptr += RTVDescriptorSize;
		return NewHandle;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE GPUAllocateRTV()
	{
		D3D12_GPU_DESCRIPTOR_HANDLE NewHandle = RTVGPUStart;
		RTVGPUStart.ptr += RTVDescriptorSize;
		return NewHandle;
	}
	D3D12_CPU_DESCRIPTOR_HANDLE CPUAllocateCSU()
	{
		D3D12_CPU_DESCRIPTOR_HANDLE NewHandle = CSUCPUStart;
		CSUCPUStart.ptr += CSUDescriptorSize;
		return NewHandle;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE GPUAllocateCSU()
	{
		D3D12_GPU_DESCRIPTOR_HANDLE NewHandle = CSUGPUStart;
		CSUGPUStart.ptr += RTVDescriptorSize;
		return NewHandle;
	}
};

#if ENABLE_VULKAN
struct FFramebuffer
{
	VkFramebuffer Framebuffer = VK_NULL_HANDLE;
	VkDevice Device = VK_NULL_HANDLE;

	void Create(VkDevice InDevice, VkRenderPass RenderPass, VkImageView ColorAttachment, VkImageView DepthAttachment, uint32 InWidth, uint32 InHeight, VkImageView ResolveColor)
	{
		Device = InDevice;
		Width = InWidth;
		Height = InHeight;

		VkImageView Attachments[3] = { ColorAttachment, DepthAttachment, ResolveColor};

		VkFramebufferCreateInfo CreateInfo;
		MemZero(CreateInfo);
		CreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		CreateInfo.renderPass = RenderPass;
		CreateInfo.attachmentCount = 1 + (DepthAttachment != VK_NULL_HANDLE ? 1 : 0) + (ResolveColor != VK_NULL_HANDLE ? 1 : 0);
		CreateInfo.pAttachments = Attachments;
		CreateInfo.width = Width;
		CreateInfo.height = Height;
		CreateInfo.layers = 1;

		checkVk(vkCreateFramebuffer(Device, &CreateInfo, nullptr, &Framebuffer));
	}

	void Destroy()
	{
		vkDestroyFramebuffer(Device, Framebuffer, nullptr);
		Framebuffer = VK_NULL_HANDLE;
	}

	uint32 Width = 0;
	uint32 Height = 0;
};

class FRenderPassLayout
{
public:
	FRenderPassLayout() {}

	FRenderPassLayout(uint32 InWidth, uint32 InHeight, uint32 InNumColorTargets, VkFormat* InColorFormats,
		VkFormat InDepthStencilFormat = VK_FORMAT_UNDEFINED, VkSampleCountFlagBits InNumSamples = VK_SAMPLE_COUNT_1_BIT, VkFormat InResolveFormat = VK_FORMAT_UNDEFINED)
		: Width(InWidth)
		, Height(InHeight)
		, NumColorTargets(InNumColorTargets)
		, DepthStencilFormat(InDepthStencilFormat)
		, NumSamples(InNumSamples)
		, ResolveFormat(InResolveFormat)
	{
		Hash = Width | (Height << 16) | ((uint64)NumColorTargets << (uint64)33);
		Hash |= ((uint64)DepthStencilFormat << (uint64)56);
		Hash |= (uint64) InNumSamples << (uint64)50;

		MemZero(ColorFormats);
		uint32 ColorHash = 0;
		for (uint32 Index = 0; Index < InNumColorTargets; ++Index)
		{
			ColorFormats[Index] = InColorFormats[Index];
			ColorHash ^= (ColorFormats[Index] << (Index * 4));
		}

		Hash ^= ((uint64)ColorHash << (uint64)40);
		Hash ^= ((uint64)ResolveFormat << (uint64)42);
	}

	inline uint64 GetHash() const
	{
		return Hash;
	}

	inline VkSampleCountFlagBits GetNumSamples() const
	{
		return NumSamples;
	}

	enum
	{
		MAX_COLOR_ATTACHMENTS = 8
	};

protected:
	uint32 Width = 0;
	uint32 Height = 0;
	uint32 NumColorTargets = 0;
	VkFormat ColorFormats[MAX_COLOR_ATTACHMENTS];
	VkFormat DepthStencilFormat = VK_FORMAT_UNDEFINED;	// Undefined means no Depth/Stencil
	VkSampleCountFlagBits NumSamples = VK_SAMPLE_COUNT_1_BIT;
	VkFormat ResolveFormat = VK_FORMAT_UNDEFINED;

	uint64 Hash = 0;

	friend struct FRenderPass;
};


struct FRenderPass
{
	VkRenderPass RenderPass = VK_NULL_HANDLE;
	VkDevice Device = VK_NULL_HANDLE;

	void Create(VkDevice InDevice, const FRenderPassLayout& InLayout);

	void Destroy()
	{
		vkDestroyRenderPass(Device, RenderPass, nullptr);
		RenderPass = VK_NULL_HANDLE;
	}

	const FRenderPassLayout& GetLayout() const
	{
		return Layout;
	}

protected:
	FRenderPassLayout Layout;
};
#endif

struct FGfxPipeline : public FBasePipeline
{
	FGfxPipeline();
	void Create(FDevice* Device, FGfxPSO* PSO, FVertexFormat* VertexFormat
#if ENABLE_VULKAN
		, uint32 Width, uint32 Height, FRenderPass* RenderPass
#endif
	);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC Desc = {};
};

#if ENABLE_VULKAN
struct FComputePipeline : public FBasePipeline
{
	void Create(VkDevice Device, FComputePSO* PSO)
	{
		std::vector<VkPipelineShaderStageCreateInfo> ShaderStages;
		PSO->SetupShaderStages(ShaderStages);
		check(ShaderStages.size() == 1);

		VkPipelineLayoutCreateInfo CreateInfo;
		MemZero(CreateInfo);
		CreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		CreateInfo.setLayoutCount = 1;
		CreateInfo.pSetLayouts = &PSO->DSLayout;
		checkVk(vkCreatePipelineLayout(Device, &CreateInfo, nullptr, &PipelineLayout));

		VkComputePipelineCreateInfo PipelineInfo;
		MemZero(PipelineInfo);
		PipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		//VkPipelineCreateFlags              flags;
		PipelineInfo.stage = ShaderStages[0];
		PipelineInfo.layout = PipelineLayout;
		//VkPipeline                         basePipelineHandle;
		//int32_t                            basePipelineIndex;
		checkVk(vkCreateComputePipelines(Device, VK_NULL_HANDLE, 1, &PipelineInfo, nullptr, &Pipeline));
	}
};

class FWriteDescriptors
{
public:
	~FWriteDescriptors()
	{
		for (auto* Info : BufferInfos)
		{
			delete Info;
		}

		for (auto* Info : ImageInfos)
		{
			delete Info;
		}
	}

	inline void AddUniformBuffer(VkDescriptorSet DescSet, uint32 Binding, const FBuffer& Buffer)
	{
		VkDescriptorBufferInfo* BufferInfo = new VkDescriptorBufferInfo;
		MemZero(*BufferInfo);
		BufferInfo->buffer = Buffer.Buffer;
		BufferInfo->offset = Buffer.GetBindOffset();
		BufferInfo->range = Buffer.GetSize();
		BufferInfos.push_back(BufferInfo);

		VkWriteDescriptorSet DSWrite;
		MemZero(DSWrite);
		DSWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		DSWrite.dstSet = DescSet;
		DSWrite.dstBinding = Binding;
		DSWrite.descriptorCount = 1;
		DSWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		DSWrite.pBufferInfo = BufferInfo;
		DSWrites.push_back(DSWrite);
	}

	template< typename TStruct>
	inline void AddUniformBuffer(VkDescriptorSet DescSet, uint32 Binding, const FUniformBuffer<TStruct>& Buffer)
	{
		AddUniformBuffer(DescSet, Binding, Buffer.Buffer);
	}

	inline void AddStorageBuffer(VkDescriptorSet DescSet, uint32 Binding, const FBuffer& Buffer)
	{
		VkDescriptorBufferInfo* BufferInfo = new VkDescriptorBufferInfo;
		MemZero(*BufferInfo);
		BufferInfo->buffer = Buffer.Buffer;
		BufferInfo->offset = Buffer.GetBindOffset();
		BufferInfo->range = Buffer.GetSize();
		BufferInfos.push_back(BufferInfo);

		VkWriteDescriptorSet DSWrite;
		MemZero(DSWrite);
		DSWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		DSWrite.dstSet = DescSet;
		DSWrite.dstBinding = Binding;
		DSWrite.descriptorCount = 1;
		DSWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		DSWrite.pBufferInfo = BufferInfo;
		DSWrites.push_back(DSWrite);
	}

	inline void AddCombinedImageSampler(VkDescriptorSet DescSet, uint32 Binding, const FSampler& Sampler, const FImageView& ImageView)
	{
		VkDescriptorImageInfo* ImageInfo = new VkDescriptorImageInfo;
		MemZero(*ImageInfo);
		ImageInfo->imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		ImageInfo->imageView = ImageView.ImageView;
		ImageInfo->sampler = Sampler.Sampler;
		ImageInfos.push_back(ImageInfo);

		VkWriteDescriptorSet DSWrite;
		MemZero(DSWrite);
		DSWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		DSWrite.dstSet = DescSet;
		DSWrite.dstBinding = Binding;
		DSWrite.descriptorCount = 1;
		DSWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		DSWrite.pImageInfo = ImageInfo;
		DSWrites.push_back(DSWrite);
	}

	inline void AddStorageImage(VkDescriptorSet DescSet, uint32 Binding, const FImageView& ImageView)
	{
		VkDescriptorImageInfo* ImageInfo = new VkDescriptorImageInfo;
		MemZero(*ImageInfo);
		ImageInfo->imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		ImageInfo->imageView = ImageView.ImageView;
		ImageInfos.push_back(ImageInfo);

		VkWriteDescriptorSet DSWrite;
		MemZero(DSWrite);
		DSWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		DSWrite.dstSet = DescSet;
		DSWrite.dstBinding = Binding;
		DSWrite.descriptorCount = 1;
		DSWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		DSWrite.pImageInfo = ImageInfo;
		DSWrites.push_back(DSWrite);
	}
	std::vector<VkWriteDescriptorSet> DSWrites;

protected:
	std::vector<VkDescriptorBufferInfo*> BufferInfos;
	std::vector<VkDescriptorImageInfo*> ImageInfos;
};
#endif

inline void ResourceBarrier(FCmdBuffer* CmdBuffer, ID3D12Resource* Image, D3D12_RESOURCE_STATES Src, D3D12_RESOURCE_STATES Dest)
{
	D3D12_RESOURCE_BARRIER Barrier;
	MemZero(Barrier);
	Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	Barrier.Transition.pResource = Image;
	Barrier.Transition.StateBefore = Src;
	Barrier.Transition.StateAfter = Dest;
	Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	CmdBuffer->CommandList->ResourceBarrier(1, &Barrier);
}

inline void ResourceBarrier(FCmdBuffer* CmdBuffer, FImage* Image, D3D12_RESOURCE_STATES Src, D3D12_RESOURCE_STATES Dest)
{
	ResourceBarrier(CmdBuffer, Image->Alloc->Resource.Get(), Src, Dest);
}

#if ENABLE_VULKAN
inline void BufferBarrier(FCmdBuffer* CmdBuffer, VkPipelineStageFlags SrcStage, VkPipelineStageFlags DestStage, VkBuffer Buffer, VkDeviceSize Offset, VkDeviceSize Size, VkAccessFlags SrcMask, VkAccessFlags DstMask)
{
	VkBufferMemoryBarrier Barrier;
	MemZero(Barrier);
	Barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	Barrier.srcAccessMask = SrcMask;
	Barrier.dstAccessMask = DstMask;
	Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	Barrier.buffer = Buffer;
	Barrier.offset = Offset;
	Barrier.size = Size;
	vkCmdPipelineBarrier(CmdBuffer->CmdBuffer, SrcStage, DestStage, 0, 0, nullptr, 1, &Barrier, 0, nullptr);
}

inline void BufferBarrier(FCmdBuffer* CmdBuffer, VkPipelineStageFlags SrcStage, VkPipelineStageFlags DestStage, FBuffer* Buffer, VkAccessFlags SrcMask, VkAccessFlags DstMask)
{
	BufferBarrier(CmdBuffer, SrcStage, DestStage, Buffer->Buffer, Buffer->GetBindOffset(), Buffer->GetSize(), SrcMask, DstMask);
}
#endif
struct FSwapchain
{
	Microsoft::WRL::ComPtr<IDXGISwapChain3> Swapchain;
	const uint32 BufferCount = 3;
#if ENABLE_VULKAN
	enum
	{
		SWAPCHAIN_IMAGE_FORMAT = VK_FORMAT_B8G8R8A8_UNORM,
		BACKBUFFER_VIEW_FORMAT = VK_FORMAT_R8G8B8A8_UNORM,
	};
#endif
	uint32 Width = 0;
	uint32 Height = 0;
	void Create(IDXGIFactory4* DXGI, HWND Hwnd, FDevice& Device, uint32& WindowWidth, uint32& WindowHeight, FDescriptorPool& Pool);

	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> Images;
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> ImageViews;
#if ENABLE_VULKAN
	std::vector<FSemaphore> PresentCompleteSemaphores;
	std::vector<FSemaphore> RenderingSemaphores;
	uint32 PresentCompleteSemaphoreIndex = 0;
	uint32 RenderingSemaphoreIndex = 0;
	VkExtent2D SurfaceResolution;
#endif
	uint32 AcquiredImageIndex = 0;

	inline ID3D12Resource* GetAcquiredImage()
	{
		return Images[AcquiredImageIndex].Get();
	}

	inline D3D12_CPU_DESCRIPTOR_HANDLE GetAcquiredImageView()
	{
		return ImageViews[AcquiredImageIndex];
	}

	void Destroy()
	{
#if ENABLE_VULKAN
		for (auto& RS : RenderingSemaphores)
		{
			RS.Destroy(Device);
		}

		for (auto& PS : PresentCompleteSemaphores)
		{
			PS.Destroy(Device);
		}

		for (auto& ImageView : ImageViews)
		{
			ImageView.Destroy();
		}

#endif
		//Swapchain1->Release();
		//Swapchain->SetFullscreenState(false, NULL);
		//Swapchain->Release();
		Swapchain = nullptr;
	}

	inline uint32 GetWidth() const
	{
		return Width;
	}


	inline uint32 GetHeight() const
	{
		return Height;
	}

	//void ClearAndTransitionToPresent(FDevice& Device, FCmdBuffer* CmdBuffer, struct FDescriptorPool* DescriptorPool);

	void Present(ID3D12CommandQueue* Queue)
	{
		uint32 SyncInterval = 1;
		checkD3D12(Swapchain->Present(SyncInterval, 0));
#if ENABLE_VULKAN
		VkPresentInfoKHR Info;
		MemZero(Info);
		Info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		Info.waitSemaphoreCount = 1;
		Info.pWaitSemaphores = &RenderingSemaphores[AcquiredImageIndex].Semaphore;
		Info.swapchainCount = 1;
		Info.pSwapchains = &Swapchain;
		Info.pImageIndices = &AcquiredImageIndex;
		checkVk(vkQueuePresentKHR(PresentQueue, &Info));
#endif
		AcquiredImageIndex = (AcquiredImageIndex + 1) % BufferCount;
	}
};

#if ENABLE_VULKAN
inline void FlushMappedBuffer(VkDevice Device, FBuffer* Buffer)
{
	VkMappedMemoryRange Range;
	MemZero(Range);
	Range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	Range.offset = Buffer->GetBindOffset();
	Range.memory = Buffer->SubAlloc->GetHandle();
	Range.size = Buffer->GetSize();
	vkInvalidateMappedMemoryRanges(Device, 1, &Range);
}

inline void CopyBuffer(FCmdBuffer* CmdBuffer, FBuffer* SrcBuffer, FBuffer* DestBuffer)
{
	VkBufferCopy Region;
	MemZero(Region);
	Region.srcOffset = SrcBuffer->GetBindOffset();
	Region.size = SrcBuffer->GetSize();
	Region.dstOffset = DestBuffer->GetBindOffset();
	vkCmdCopyBuffer(CmdBuffer->CmdBuffer, SrcBuffer->Buffer, DestBuffer->Buffer, 1, &Region);
}

template <typename TFillLambda>
inline void MapAndFillBufferSync(FStagingBuffer* StagingBuffer, FCmdBuffer* CmdBuffer, FBuffer* DestBuffer, TFillLambda Fill, uint32 Size)
{
	void* Data = StagingBuffer->GetMappedData();
	check(Data);
	Fill(Data);

	CopyBuffer(CmdBuffer, StagingBuffer, DestBuffer);
	StagingBuffer->SetFence(CmdBuffer);
}
#endif

template <typename TFillLambda>
inline void MapAndFillImageSync(FStagingBuffer* StagingBuffer, FCmdBuffer* CmdBuffer, FImage* DestImage, TFillLambda Fill)
{
	void* Data = StagingBuffer->GetMappedData();
	check(Data);
	Fill(Data, DestImage->Width, DestImage->Height);

	D3D12_TEXTURE_COPY_LOCATION Src;
	MemZero(Src);
	Src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	Src.pResource = StagingBuffer->Alloc->Resource.Get();
	Src.PlacedFootprint.Footprint.Format = DestImage->Format;
	Src.PlacedFootprint.Footprint.Width = DestImage->Width;
	Src.PlacedFootprint.Footprint.Height = DestImage->Height;
	Src.PlacedFootprint.Footprint.Depth = 1;
	Src.PlacedFootprint.Footprint.RowPitch = DestImage->Width * GetFormatBitsPerPixel(DestImage->Format) / 8;
	D3D12_TEXTURE_COPY_LOCATION Dest;
	MemZero(Dest);
	Dest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	Dest.pResource = DestImage->Alloc->Resource.Get();

	D3D12_BOX SrcBox;
	MemZero(SrcBox);
	SrcBox.right = DestImage->Width;
	SrcBox.bottom = DestImage->Height;
	SrcBox.back = 1;
	CmdBuffer->CommandList->CopyTextureRegion(&Dest, 0, 0, 0, &Src, &SrcBox);

	StagingBuffer->SetFence(CmdBuffer);
}

#if ENABLE_VULKAN
inline void CopyColorImage(FCmdBuffer* CmdBuffer, uint32 Width, uint32 Height, VkImage SrcImage, VkImageLayout SrcCurrentLayout, VkImage DstImage, VkImageLayout DstCurrentLayout)
{
	check(CmdBuffer->State == FCmdBuffer::EState::Begun);
	VkImageCopy CopyRegion;
	MemZero(CopyRegion);
	CopyRegion.extent.width = Width;
	CopyRegion.extent.height = Height;
	CopyRegion.extent.depth = 1;
	CopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	CopyRegion.srcSubresource.layerCount = 1;
	CopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	CopyRegion.dstSubresource.layerCount = 1;
	if (SrcCurrentLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
	{
		ImageBarrier(CmdBuffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, SrcImage, SrcCurrentLayout, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
	}
	if (DstCurrentLayout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		ImageBarrier(CmdBuffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, DstImage, DstCurrentLayout, 0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
	}
	vkCmdCopyImage(CmdBuffer->CmdBuffer, SrcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, DstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &CopyRegion);
};

inline void BlitColorImage(FCmdBuffer* CmdBuffer, uint32 Width, uint32 Height, VkImage SrcImage, VkImageLayout SrcCurrentLayout, VkImage DstImage, VkImageLayout DstCurrentLayout)
{
	check(CmdBuffer->State == FCmdBuffer::EState::Begun);
	VkImageBlit BlitRegion;
	MemZero(BlitRegion);
	BlitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	BlitRegion.srcOffsets[1].x = Width;
	BlitRegion.srcOffsets[1].y = Height;
	BlitRegion.srcOffsets[1].z = 1;
	BlitRegion.srcSubresource.layerCount = 1;
	BlitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	BlitRegion.dstOffsets[1].x = Width;
	BlitRegion.dstOffsets[1].y = Height;
	BlitRegion.dstOffsets[1].z = 1;
	BlitRegion.dstSubresource.layerCount = 1;
	if (SrcCurrentLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
	{
		ImageBarrier(CmdBuffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, SrcImage, SrcCurrentLayout, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
	}
	if (DstCurrentLayout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		ImageBarrier(CmdBuffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, DstImage, DstCurrentLayout, 0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
	}
	vkCmdBlitImage(CmdBuffer->CmdBuffer, SrcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, DstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &BlitRegion, VK_FILTER_NEAREST);
};
#endif

inline void CmdBind(FCmdBuffer* CmdBuffer, FGfxPipeline * GfxPipeline)
{
	CmdBuffer->CommandList->SetPipelineState(GfxPipeline->PipelineState.Get());
	CmdBuffer->CommandList->SetGraphicsRootSignature(GfxPipeline->Desc.pRootSignature);
}



template <typename TStruct>
inline void FUniformBuffer<TStruct>::Create(FDevice& InDevice, FDescriptorPool& Pool, FMemManager& MemMgr, bool bUploadCPU)
{
	uint32 Size = (sizeof(TStruct) + 255) & ~255;
	Buffer.Create(InDevice, Size, MemMgr, bUploadCPU);

	MemZero(View);
	View.BufferLocation = Buffer.Alloc->Resource->GetGPUVirtualAddress();
	View.SizeInBytes = Size;

	D3D12_CPU_DESCRIPTOR_HANDLE Handle = Pool.CPUAllocateCSU();
	InDevice.Device->CreateConstantBufferView(&View, Handle);

	GPUHandle = Pool.GPUAllocateCSU();
}
