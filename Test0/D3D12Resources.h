
#pragma once

#include "D3D12Device.h"
#include "D3D12Mem.h"

struct FDescriptorHandle
{
	D3D12_CPU_DESCRIPTOR_HANDLE CPU = {0};
	D3D12_GPU_DESCRIPTOR_HANDLE GPU = {0};
};

struct FBuffer
{
	void Create(LPCWSTR Name, FDevice& InDevice, uint64 InSize, FMemManager& MemMgr, D3D12_RESOURCE_STATES ResourceStates, D3D12_RESOURCE_FLAGS ResourceFlags, bool bUploadCPU)
	{
		Size = InSize;

		Alloc = MemMgr.AllocBuffer(Name, InDevice, InSize, ResourceStates, ResourceFlags, bUploadCPU);
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

struct FIndexBuffer
{
	uint32 NumIndices = 0;
	bool b32Bits = false;

	void Create(LPCWSTR Name, FDevice& InDevice, bool bIn32Bits, uint32 InNumIndices, FMemManager& MemMgr, D3D12_RESOURCE_STATES ResourceStates, bool bUploadCPU, D3D12_RESOURCE_FLAGS ResourceFlags = D3D12_RESOURCE_FLAG_NONE)
	{
		b32Bits = bIn32Bits;
		NumIndices = InNumIndices;
		uint32 Size = NumIndices * (b32Bits ? 4 : 2);
		Buffer.Create(Name, InDevice, Size, MemMgr, ResourceStates, ResourceFlags, bUploadCPU);
		View.BufferLocation = Buffer.Alloc->Resource->GetGPUVirtualAddress();
		View.SizeInBytes = Size;
		View.Format = b32Bits ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
	}

	void Destroy()
	{
		Buffer.Destroy();
	}

	FBuffer Buffer;

	D3D12_INDEX_BUFFER_VIEW View;
};

inline void CmdBind(FCmdBuffer* CmdBuffer, FIndexBuffer* IB)
{
	CmdBuffer->CommandList->IASetIndexBuffer(&IB->View);
}

struct FRWIndexBuffer
{
	void Create(FDevice& InDevice, struct FDescriptorPool& Pool, bool bIn32Bits, uint32 InNumIndices, FMemManager& MemMgr, bool bUploadCPU)
	{
		IB.Create(L"RWIndexBuffer", InDevice, bIn32Bits, InNumIndices, MemMgr, D3D12_RESOURCE_STATE_INDEX_BUFFER, bUploadCPU, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		MemZero(View);
		View.Format =  DXGI_FORMAT_UNKNOWN;
		View.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		View.Buffer.NumElements = IB.NumIndices;
		View.Buffer.StructureByteStride = IB.b32Bits ? 4 : 2;
	}

	void Destroy()
	{
		IB.Destroy();
	}

	FIndexBuffer IB;
	D3D12_UNORDERED_ACCESS_VIEW_DESC View;
};

struct FVertexBuffer
{
	void Create(LPCWSTR Name, FDevice& InDevice, uint32 Stride, uint32 Size, FMemManager& MemMgr, D3D12_RESOURCE_STATES ResourceStates, bool bUploadCPU, D3D12_RESOURCE_FLAGS ResourceFlags = D3D12_RESOURCE_FLAG_NONE)
	{
		Buffer.Create(Name, InDevice, Size, MemMgr, ResourceStates, ResourceFlags, bUploadCPU);
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

struct FRWVertexBuffer
{
	void Create(FDevice& InDevice, struct FDescriptorPool& Pool, uint32 Stride, uint32 Size, FMemManager& MemMgr, bool bUploadCPU)
	{
		VB.Create(L"RWVertexBuffer", InDevice, Stride, Size, MemMgr, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, bUploadCPU, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		MemZero(View);
		View.Format =  DXGI_FORMAT_UNKNOWN;
		View.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		View.Buffer.NumElements = Size / Stride;
		View.Buffer.StructureByteStride = Stride;
	}

	void Destroy()
	{
		VB.Destroy();
	}

	FVertexBuffer VB;
	D3D12_UNORDERED_ACCESS_VIEW_DESC View;
};

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
	FDescriptorHandle Handle;
	FBuffer Buffer;
};

struct FImage
{
	//Microsoft::WRL::ComPtr<ID3D12Resource> Texture;
	FResourceAllocation* Alloc = nullptr;

	void Create(FDevice& InDevice, uint32 InWidth, uint32 InHeight, DXGI_FORMAT InFormat, FMemManager& MemMgr, D3D12_RESOURCE_FLAGS ResourceFlags = D3D12_RESOURCE_FLAG_NONE
#if ENABLE_VULKAN
		uint32 InNumMips, VkSampleCountFlagBits InSamples
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
		Alloc = MemMgr.AllocTexture2D(InDevice, Width, Height, Format, false, ResourceFlags);
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
	DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
	FDescriptorHandle Handle;

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
		Buffer->Create(L"Upload", InDevice, Size, MemMgr, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_FLAG_NONE, true);
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
	void Create(FDevice& InDevice, uint32 InWidth, uint32 InHeight, DXGI_FORMAT Format, FDescriptorPool& Pool, FMemManager& MemMgr, D3D12_RESOURCE_FLAGS ResourceFlags = D3D12_RESOURCE_FLAG_NONE
#if ENABLE_VULKAN
		, uint32 InNumMips = 1, VkSampleCountFlagBits Samples = VK_SAMPLE_COUNT_1_BIT
#endif
	)
	{
		Image.Create(InDevice, InWidth, InHeight, Format, MemMgr, ResourceFlags
#if ENABLE_VULKAN
			, InNumMips, Samples
#endif
		);

		ImageView.Create(InDevice, Image, Format, Pool);

		MemZero(SRVView);
		SRVView.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		SRVView.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		SRVView.Format = Format;
		SRVView.Texture2D.MipLevels = 1;
	}

	void Destroy()
	{
		ImageView.Destroy();
		Image.Destroy();
	}

	FImage Image;
	FImageView ImageView;

	inline DXGI_FORMAT GetFormat() const
	{
		return ImageView.Format;
	}

#if ENABLE_VULKAN

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

	D3D12_SHADER_RESOURCE_VIEW_DESC SRVView;
};

struct FSampler
{
	FDescriptorHandle Handle;
	void Create(FDevice& InDevice, FDescriptorPool& Pool);

	void Destroy()
	{
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
	}


	static inline void AddRootTableParam(std::vector<D3D12_ROOT_PARAMETER>& OutRootParameters, std::vector<D3D12_DESCRIPTOR_RANGE>& Ranges, int32 StartRange, int32 NumRanges, D3D12_SHADER_VISIBILITY Stage)
	{
		D3D12_ROOT_PARAMETER RootParam;
		MemZero(RootParam);
		RootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		RootParam.ShaderVisibility = Stage;
		RootParam.DescriptorTable.NumDescriptorRanges = NumRanges;
		RootParam.DescriptorTable.pDescriptorRanges = &Ranges[StartRange];
		OutRootParameters.push_back(RootParam);
	}

	static inline void AddRootSRVParam(std::vector<D3D12_ROOT_PARAMETER>& OutRootParameters, int32 Binding, D3D12_SHADER_VISIBILITY Stage)
	{
		D3D12_ROOT_PARAMETER RootParam;
		MemZero(RootParam);
		RootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		RootParam.ShaderVisibility = Stage;
		RootParam.Descriptor.RegisterSpace = 0;
		RootParam.Descriptor.ShaderRegister = Binding;
		OutRootParameters.push_back(RootParam);
	}

	static inline void AddRootUAVParam(std::vector<D3D12_ROOT_PARAMETER>& OutRootParameters, int32 Binding, D3D12_SHADER_VISIBILITY Stage)
	{
		D3D12_ROOT_PARAMETER RootParam;
		MemZero(RootParam);
		RootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
		RootParam.ShaderVisibility = Stage;
		RootParam.Descriptor.RegisterSpace = 0;
		RootParam.Descriptor.ShaderRegister = Binding;
		OutRootParameters.push_back(RootParam);
	}

	static inline void AddRootCBVParam(std::vector<D3D12_ROOT_PARAMETER>& OutRootParameters, int32 Binding, D3D12_SHADER_VISIBILITY Stage)
	{
		D3D12_ROOT_PARAMETER RootParam;
		MemZero(RootParam);
		RootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		RootParam.ShaderVisibility = Stage;
		RootParam.Descriptor.RegisterSpace = 0;
		RootParam.Descriptor.ShaderRegister = Binding;
		OutRootParameters.push_back(RootParam);
	}

	static inline uint32 AddRange(std::vector<D3D12_DESCRIPTOR_RANGE>& OutRanges, int32 Register, D3D12_DESCRIPTOR_RANGE_TYPE RangeType)
	{
		uint32 RangeIndex = (uint32)OutRanges.size();

		D3D12_DESCRIPTOR_RANGE Range;
		MemZero(Range);
		Range.RangeType = RangeType;
		Range.NumDescriptors = 1;
		Range.BaseShaderRegister = Register;
		Range.RegisterSpace = 0;
		//Range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
		Range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		OutRanges.push_back(Range);
		return RangeIndex;
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

struct FComputePSO : public FPSO
{
	FShader CS;

	virtual void Destroy() override
	{
		FPSO::Destroy();
		CS.Destroy();
	}

	bool Create(FDevice& Device, const char* CSFilename)
	{
		if (!CS.Create(CSFilename, Device, "cs_5_0"))
		{
			return false;
		}

		CreateDescriptorSetLayout(Device);
		return true;
	}
};

struct FBasePipeline
{
	Microsoft::WRL::ComPtr<ID3D12PipelineState> PipelineState;
};

struct FDescriptorPool
{
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> RTVHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CSUHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DSVHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> SamplerHeap;

	D3D12_CPU_DESCRIPTOR_HANDLE RTVCPUStart = {0};
	D3D12_GPU_DESCRIPTOR_HANDLE RTVGPUStart = {0};
	SIZE_T RTVDescriptorSize = 0;

	D3D12_CPU_DESCRIPTOR_HANDLE CSUCPUStart ={0};
	D3D12_GPU_DESCRIPTOR_HANDLE CSUGPUStart ={0};
	SIZE_T CSUDescriptorSize = 0;

	D3D12_CPU_DESCRIPTOR_HANDLE DSVCPUStart ={0};
	D3D12_GPU_DESCRIPTOR_HANDLE DSVGPUStart ={0};
	SIZE_T DSVDescriptorSize = 0;

	D3D12_CPU_DESCRIPTOR_HANDLE SamplerCPUStart ={0};
	D3D12_GPU_DESCRIPTOR_HANDLE SamplerGPUStart ={0};
	SIZE_T SamplerDescriptorSize = 0;

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

		{
			HeapDesc.NumDescriptors = 32768;
			HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
			HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			checkD3D12(InDevice.Device->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&DSVHeap)));
		}

		{
			HeapDesc.NumDescriptors = 2048;
			HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			checkD3D12(InDevice.Device->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&SamplerHeap)));
		}

		RTVCPUStart = RTVHeap->GetCPUDescriptorHandleForHeapStart();
		RTVGPUStart = RTVHeap->GetGPUDescriptorHandleForHeapStart();
		RTVDescriptorSize = InDevice.Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		CSUCPUStart = CSUHeap->GetCPUDescriptorHandleForHeapStart();
		CSUGPUStart = CSUHeap->GetGPUDescriptorHandleForHeapStart();
		CSUDescriptorSize = InDevice.Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		DSVCPUStart = DSVHeap->GetCPUDescriptorHandleForHeapStart();
		DSVGPUStart = DSVHeap->GetGPUDescriptorHandleForHeapStart();
		DSVDescriptorSize = InDevice.Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

		SamplerCPUStart = SamplerHeap->GetCPUDescriptorHandleForHeapStart();
		SamplerGPUStart = SamplerHeap->GetGPUDescriptorHandleForHeapStart();
		SamplerDescriptorSize = InDevice.Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	}

	void Destroy()
	{
		CSUHeap = nullptr;
		RTVHeap = nullptr;
	}

	FDescriptorHandle AllocateRTV()
	{
		FDescriptorHandle Handle;
		Handle.CPU = CPUAllocateRTV();
		Handle.GPU = GPUAllocateRTV();
		return Handle;
	}

	FDescriptorHandle AllocateCSU()
	{
		FDescriptorHandle Handle;
		Handle.CPU = CPUAllocateCSU();
		Handle.GPU = GPUAllocateCSU();
		return Handle;
	}

	FDescriptorHandle AllocateSampler()
	{
		FDescriptorHandle Handle;
		Handle.CPU = CPUAllocateSampler();
		Handle.GPU = GPUAllocateSampler();
		return Handle;
	}

	FDescriptorHandle AllocateDSV()
	{
		FDescriptorHandle Handle;
		Handle.CPU = CPUAllocateDSV();
		Handle.GPU = GPUAllocateDSV();
		return Handle;
	}

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

	D3D12_CPU_DESCRIPTOR_HANDLE CPUAllocateDSV()
	{
		D3D12_CPU_DESCRIPTOR_HANDLE NewHandle = DSVCPUStart;
		DSVCPUStart.ptr += DSVDescriptorSize;
		return NewHandle;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE GPUAllocateDSV()
	{
		D3D12_GPU_DESCRIPTOR_HANDLE NewHandle = DSVGPUStart;
		DSVGPUStart.ptr += RTVDescriptorSize;
		return NewHandle;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE CPUAllocateSampler()
	{
		D3D12_CPU_DESCRIPTOR_HANDLE NewHandle = SamplerCPUStart;
		SamplerCPUStart.ptr += SamplerDescriptorSize;
		return NewHandle;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE GPUAllocateSampler()
	{
		D3D12_GPU_DESCRIPTOR_HANDLE NewHandle = SamplerGPUStart;
		SamplerGPUStart.ptr += SamplerDescriptorSize;
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

	D3D12_GRAPHICS_PIPELINE_STATE_DESC Desc = {0};
};

struct FComputePipeline : public FBasePipeline
{
	D3D12_COMPUTE_PIPELINE_STATE_DESC Desc ={0};

	void Create(FDevice* Device, FComputePSO* PSO)
	{
		Desc.pRootSignature = PSO->RootSignature.Get();

		Desc.CS.pShaderBytecode = PSO->CS.UCode->GetBufferPointer();
		Desc.CS.BytecodeLength = PSO->CS.UCode->GetBufferSize();

		checkD3D12(Device->Device->CreateComputePipelineState(&Desc, IID_PPV_ARGS(&PipelineState)));

#if ENABLE_VULKAN
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
#endif
	}
};

#if ENABLE_VULKAN
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
	//char s[256];
	//sprintf_s(s, sizeof(s), "*** %p: %d -> %d\n", Image, Src, Dest);
	//::OutputDebugStringA(s);
}

inline void ResourceBarrier(FCmdBuffer* CmdBuffer, FImage* Image, D3D12_RESOURCE_STATES Src, D3D12_RESOURCE_STATES Dest)
{
	ResourceBarrier(CmdBuffer, Image->Alloc->Resource.Get(), Src, Dest);
}

struct FSwapchain
{
	Microsoft::WRL::ComPtr<IDXGISwapChain3> Swapchain;
	const uint32 BufferCount = 3;

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
#endif

inline void BlitColorImage(FCmdBuffer* CmdBuffer, uint32 Width, uint32 Height, ID3D12Resource* SrcImage, /*VkImageLayout SrcCurrentLayout, */ID3D12Resource* DstImage/*, VkImageLayout DstCurrentLayout*/)
{
	check(CmdBuffer->State == FCmdBuffer::EState::Begun);
#if ENABLE_VULKAN
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
#endif
	CmdBuffer->CommandList->CopyResource(DstImage, SrcImage);
};

inline void CmdBind(FCmdBuffer* CmdBuffer, FGfxPipeline * GfxPipeline)
{
	CmdBuffer->CommandList->SetPipelineState(GfxPipeline->PipelineState.Get());
	CmdBuffer->CommandList->SetGraphicsRootSignature(GfxPipeline->Desc.pRootSignature);
}



template <typename TStruct>
inline void FUniformBuffer<TStruct>::Create(FDevice& InDevice, FDescriptorPool& Pool, FMemManager& MemMgr, bool bUploadCPU)
{
	uint32 Size = (sizeof(TStruct) + 255) & ~255;
	Buffer.Create(L"UniformBuffer", InDevice, Size, MemMgr, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_FLAG_NONE, bUploadCPU);

	MemZero(View);
	View.BufferLocation = Buffer.Alloc->Resource->GetGPUVirtualAddress();
	View.SizeInBytes = Size;

	Handle = Pool.AllocateCSU();
	InDevice.Device->CreateConstantBufferView(&View, Handle.CPU);
}

inline D3D12_UNORDERED_ACCESS_VIEW_DESC MakeTexture2DUAVDesc(DXGI_FORMAT Format)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc;
	MemZero(UAVDesc);
	UAVDesc.Format = Format;
	UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	return UAVDesc;
}
