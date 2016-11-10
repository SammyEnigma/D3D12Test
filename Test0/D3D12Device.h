// Base Vulkan header classes

#pragma once

#include "Util.h"
#include <dxgi1_4.h>
#include <d3d12.h>
#include <wrl.h>
#include <d3dcompiler.h>

#if ENABLE_VULKAN

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#endif

struct FInstance
{
#if ENABLE_VULKAN
	VkSurfaceKHR Surface = VK_NULL_HANDLE;
#endif
	void CreateInstance()
	{
		SetupDebugLayer();
		//DXGI_CREATE_FACTORY_DEBUG
		checkD3D12(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&DXGIFactory)));
	}

	void SetupDebugLayer();

	void DestroyInstance()
	{
		DebugInterface = nullptr;
		DXGIFactory = nullptr;
	}

#if ENABLE_VULKAN
	void CreateSurface(HINSTANCE hInstance, HWND hWnd)
	{
		VkWin32SurfaceCreateInfoKHR SurfaceInfo;
		MemZero(SurfaceInfo);
		SurfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		SurfaceInfo.hinstance = hInstance;
		SurfaceInfo.hwnd = hWnd;
		checkVk(vkCreateWin32SurfaceKHR(Instance, &SurfaceInfo, nullptr, &Surface));
	}

	void DestroySurface()
	{
		vkDestroySurfaceKHR(Instance, Surface, nullptr);
		Surface = VK_NULL_HANDLE;
	}
#endif
	void Create(HINSTANCE hInstance, HWND hWnd)
	{
		CreateInstance();
#if ENABLE_VULKAN
		CreateSurface(hInstance, hWnd);
#endif
	}

	void CreateDevice(struct FDevice& OutDevice);

	void Destroy()
	{
#if ENABLE_VULKAN
		DestroySurface();
#endif
		DestroyInstance();
	}

	Microsoft::WRL::ComPtr<ID3D12Debug> DebugInterface;
	Microsoft::WRL::ComPtr<IDXGIFactory4> DXGIFactory;
};

struct FDevice
{
	Microsoft::WRL::ComPtr<ID3D12Device> Device;
	Microsoft::WRL::ComPtr<IDXGIAdapter1> Adapter;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> Queue;

	void Create()
	{
		checkD3D12(D3D12CreateDevice(Adapter.Get(), D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), &Device));

		D3D12_COMMAND_QUEUE_DESC QueueDesc;
		MemZero(QueueDesc);
		QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		QueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		QueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		checkD3D12(Device->CreateCommandQueue(&QueueDesc, IID_PPV_ARGS(&Queue)));
	}

	void Destroy()
	{
		Queue = nullptr;
		Device = nullptr;
		Adapter = nullptr;
	}

	operator ID3D12Device* ()
	{
		return Device.Get();
	}
};

#if ENABLE_VULKAN
struct FFence
{
	VkFence Fence = VK_NULL_HANDLE;
	uint64 FenceSignaledCounter = 0;
	VkDevice Device = VK_NULL_HANDLE;

	enum EState
	{
		NotSignaled,
		Signaled,
	};
	EState State = EState::NotSignaled;

	void Create(VkDevice InDevice)
	{
		Device = InDevice;

		VkFenceCreateInfo Info;
		MemZero(Info);
		Info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		checkVk(vkCreateFence(Device, &Info, nullptr, &Fence));
	}

	void Destroy(VkDevice Device)
	{
		vkDestroyFence(Device, Fence, nullptr);
		Fence = VK_NULL_HANDLE;
	}

	void Wait(uint64 TimeInNanoseconds = 0xffffffff)
	{
		check(State == EState::NotSignaled);
		checkVk(vkWaitForFences(Device, 1, &Fence, true, TimeInNanoseconds));
		RefreshState();
	}

	bool IsNotSignaled() const
	{
		return State == EState::NotSignaled;
	}

	void RefreshState()
	{
		if (State == EState::NotSignaled)
		{
			VkResult Result = vkGetFenceStatus(Device, Fence);
			switch (Result)
			{
			case VK_SUCCESS:
				++FenceSignaledCounter;
				State = EState::Signaled;
				checkVk(vkResetFences(Device, 1, &Fence));
				break;
			case VK_NOT_READY:
				break;
			default:
				checkVk(Result);
				break;
			}
		}
	}
};
#endif

struct FCmdBuffer
{
#if ENABLE_VULKAN
	VkCommandBuffer CmdBuffer = VK_NULL_HANDLE;
	VkDevice Device = VK_NULL_HANDLE;

	FFence* Fence = nullptr;
#endif
	enum class EState
	{
		ReadyForBegin,
		Begun,
		Ended,
		Submitted,
		InsideRenderPass,
	};
	EState State = EState::ReadyForBegin;

#if ENABLE_VULKAN
	virtual void Destroy(VkDevice Device, VkCommandPool Pool)
	{
		if (State == EState::Submitted)
		{
			RefreshState();
			if (Fence->IsNotSignaled())
			{
				const uint64 TimeToWaitInNanoseconds = 5;
				Fence->Wait(TimeToWaitInNanoseconds);
			}
			RefreshState();
		}
		vkFreeCommandBuffers(Device, Pool, 1, &CmdBuffer);
		CmdBuffer = VK_NULL_HANDLE;
	}

	void BeginRenderPass(VkRenderPass RenderPass, const struct FFramebuffer& Framebuffer, bool bHasSecondary);

	void EndRenderPass()
	{
		check(State == EState::InsideRenderPass);

		vkCmdEndRenderPass(CmdBuffer);

		State = EState::Begun;
	}

	void WaitForFence()
	{
		if (State == EState::Submitted)
		{
			Fence->Wait();
			RefreshState();
		}
	}
#endif
	void RefreshState()
	{
#if ENABLE_VULKAN
		if (State == EState::Submitted)
		{
			uint64 PrevCounter = Fence->FenceSignaledCounter;
			Fence->RefreshState();
			if (PrevCounter != Fence->FenceSignaledCounter)
			{
				checkVk(vkResetCommandBuffer(CmdBuffer, 0));
				State = EState::ReadyForBegin;
			}
		}
#endif
	}

#if ENABLE_VULKAN
	virtual struct FSecondaryCmdBuffer* GetSecondary()
	{
		return nullptr;
	}

	void End()
	{
		check(State == EState::Begun);
		checkVk(vkEndCommandBuffer(CmdBuffer));
		State = EState::Ended;
	}
#endif
};

struct FPrimaryCmdBuffer : public FCmdBuffer
{
#if ENABLE_VULKAN
	FFence PrimaryFence;
#endif

	void Create(/*VkDevice InDevice, VkCommandPool Pool*/)
	{
#if ENABLE_VULKAN
		Device = InDevice;

		PrimaryFence.Create(Device);
		Fence = &PrimaryFence;

		VkCommandBufferAllocateInfo Info;
		MemZero(Info);
		Info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		Info.commandPool = Pool;
		Info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		Info.commandBufferCount = 1;

		checkVk(vkAllocateCommandBuffers(Device, &Info, &CmdBuffer));
#endif
	}

	void Begin()
	{
		check(State == EState::ReadyForBegin);
#if ENABLE_VULKAN
		VkCommandBufferBeginInfo Info;
		MemZero(Info);
		Info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		Info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		checkVk(vkBeginCommandBuffer(CmdBuffer, &Info));
#endif
		State = EState::Begun;
	}

#if ENABLE_VULKAN
	void ExecuteSecondary()
	{
		check(State != FCmdBuffer::EState::Ended && State != FCmdBuffer::EState::Submitted);
		if (!Secondary.empty())
		{
			vkCmdExecuteCommands(CmdBuffer, (uint32)Secondary.size(), &SecondaryList[0]);
			Secondary.resize(0);
			SecondaryList.resize(0);
		}
	}

	virtual void Destroy(VkDevice Device, VkCommandPool Pool) override
	{
		FCmdBuffer::Destroy(Device, Pool);
		Fence->Destroy(Device);
	}

	std::list<struct FSecondaryCmdBuffer*> Secondary;
	std::vector<VkCommandBuffer> SecondaryList;
#endif
};

#if ENABLE_VULKAN
struct FSecondaryCmdBuffer : public FCmdBuffer
{
	void BeginSecondary(FPrimaryCmdBuffer* ParentCmdBuffer, VkRenderPass RenderPass, VkFramebuffer Framebuffer)
	{
		check(State == EState::ReadyForBegin);

		VkCommandBufferInheritanceInfo Inheritance;
		MemZero(Inheritance);
		Inheritance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		//uint32_t                         subpass;
		//VkBool32                         occlusionQueryEnable;
		//VkQueryControlFlags              queryFlags;
		//VkQueryPipelineStatisticFlags    pipelineStatistics;
		VkCommandBufferBeginInfo Info;
		MemZero(Info);
		Info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		Info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		if (RenderPass != VK_NULL_HANDLE)
		{
			Info.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
			Inheritance.renderPass = RenderPass;
			Inheritance.framebuffer = Framebuffer;
		}
		else
		{
			check(Framebuffer == VK_NULL_HANDLE);
		}
		Info.pInheritanceInfo = &Inheritance;
		checkVk(vkBeginCommandBuffer(CmdBuffer, &Info));

		State = EState::Begun;

		ParentCmdBuffer->Secondary.push_back(this);
		ParentCmdBuffer->SecondaryList.push_back(CmdBuffer);
	}

	void CreateSecondary(VkDevice InDevice, VkCommandPool Pool, FFence* ParentFence)
	{
		Device = InDevice;

		VkCommandBufferAllocateInfo Info;
		MemZero(Info);
		Info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		Info.commandPool = Pool;
		Info.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
		Info.commandBufferCount = 1;

		checkVk(vkAllocateCommandBuffers(Device, &Info, &CmdBuffer));
	}

	virtual struct FSecondaryCmdBuffer* GetSecondary()
	{
		return this;
	}
};

class FCmdBufferFence
{
protected:
	FCmdBuffer* CmdBuffer;
	uint64 FenceSignaledCounter;

public:
	FCmdBufferFence(FCmdBuffer* InCmdBuffer)
	{
		CmdBuffer = InCmdBuffer;
		FenceSignaledCounter = InCmdBuffer->Fence->FenceSignaledCounter;
	}

	bool HasFencePassed() const
	{
		return FenceSignaledCounter < CmdBuffer->Fence->FenceSignaledCounter;
	}
};

struct FSemaphore
{
	VkSemaphore Semaphore = VK_NULL_HANDLE;

	void Create(VkDevice Device)
	{
		VkSemaphoreCreateInfo Info;
		MemZero(Info);
		Info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		checkVk(vkCreateSemaphore(Device, &Info, nullptr, &Semaphore));
	}

	void Destroy(VkDevice Device)
	{
		vkDestroySemaphore(Device, Semaphore, nullptr);
		Semaphore = VK_NULL_HANDLE;
	}
};
#endif


struct FCmdBufferMgr
{
#if ENABLE_VULKAN
	VkCommandPool Pool = VK_NULL_HANDLE;
#endif
	void Create()
	{
#if ENABLE_VULKAN
		VkCommandPoolCreateInfo PoolInfo;
		MemZero(PoolInfo);
		PoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		PoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		PoolInfo.queueFamilyIndex = QueueFamilyIndex;

		checkVk(vkCreateCommandPool(Device, &PoolInfo, nullptr, &Pool));
#endif
	}

	void Destroy()
	{
#if ENABLE_VULKAN
		for (auto* CB : CmdBuffers)
		{
			CB->RefreshState();
			CB->Destroy(Device, Pool);
			delete CB;
		}
		CmdBuffers.clear();

		for (auto* CB : SecondaryCmdBuffers)
		{
			CB->RefreshState();
			CB->Destroy(Device, Pool);
			delete CB;
		}
		SecondaryCmdBuffers.clear();

		vkDestroyCommandPool(Device, Pool, nullptr);
		Pool = VK_NULL_HANDLE;
#endif
	}

	FPrimaryCmdBuffer* AllocateCmdBuffer()
	{
		for (auto* CmdBuffer : CmdBuffers)
		{
			CmdBuffer->RefreshState();
			if (CmdBuffer->State == FPrimaryCmdBuffer::EState::ReadyForBegin)
			{
				return CmdBuffer;
			}
		}

		auto* NewCmdBuffer = new FPrimaryCmdBuffer;
		NewCmdBuffer->Create();
		CmdBuffers.push_back(NewCmdBuffer);
		return NewCmdBuffer;
	}

#if ENABLE_VULKAN
	FSecondaryCmdBuffer* AllocateSecondaryCmdBuffer(FFence* ParentFence)
	{
		for (auto* CmdBuffer : SecondaryCmdBuffers)
		{
			CmdBuffer->RefreshState();
			if (CmdBuffer->State == FPrimaryCmdBuffer::EState::ReadyForBegin)
			{
				CmdBuffer->Fence = ParentFence;
				return CmdBuffer;
			}
		}

		auto* NewCmdBuffer = new FSecondaryCmdBuffer;
		NewCmdBuffer->CreateSecondary(Device, Pool, ParentFence);
		SecondaryCmdBuffers.push_back(NewCmdBuffer);
		return NewCmdBuffer;
	}
/*
	FCmdBuffer* GetActiveCmdBuffer()
	{
		for (auto* CB : CmdBuffers)
		{
			switch (CB->State)
			{
			case FCmdBuffer::EState::Submitted:
				CB->RefreshState();
				break;
			case FCmdBuffer::EState::ReadyForBegin:
				return CB;
			default:
				break;
			}
		}

		for (auto* CB : SecondaryCmdBuffers)
		{
			switch (CB->State)
			{
			case FCmdBuffer::EState::Submitted:
				CB->RefreshState();
				break;
			case FCmdBuffer::EState::ReadyForBegin:
				return CB;
			default:
				break;
			}
		}

		return AllocateCmdBuffer();
	}*/
#endif
	FPrimaryCmdBuffer* GetActivePrimaryCmdBuffer()
	{
		for (auto* CB : CmdBuffers)
		{
			switch (CB->State)
			{
			case FPrimaryCmdBuffer::EState::Submitted:
				CB->RefreshState();
				break;
			case FPrimaryCmdBuffer::EState::ReadyForBegin:
				return CB;
			default:
				break;
			}
		}

		return AllocateCmdBuffer();
	}

	void Submit(/*FPrimaryCmdBuffer* CmdBuffer, VkQueue Queue, FSemaphore* WaitSemaphore, FSemaphore* SignaledSemaphore*/)
	{
#if ENABLE_VULKAN
		check(CmdBuffer->State == FPrimaryCmdBuffer::EState::Ended);
		check(CmdBuffer->Secondary.empty());
		VkPipelineStageFlags StageMask[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		VkSubmitInfo Info;
		MemZero(Info);
		Info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		Info.pWaitDstStageMask = StageMask;
		Info.commandBufferCount = 1;
		Info.pCommandBuffers = &CmdBuffer->CmdBuffer;
		if (WaitSemaphore)
		{
			Info.waitSemaphoreCount = 1;
			Info.pWaitSemaphores = &WaitSemaphore->Semaphore;
		}
		if (SignaledSemaphore)
		{
			Info.signalSemaphoreCount = 1;
			Info.pSignalSemaphores = &SignaledSemaphore->Semaphore;
		}
		checkVk(vkQueueSubmit(Queue, 1, &Info, CmdBuffer->Fence->Fence));
		CmdBuffer->Fence->State = FFence::EState::NotSignaled;
		CmdBuffer->State = FPrimaryCmdBuffer::EState::Submitted;
#endif
		Update();
	}

	void Update()
	{
		for (auto* CmdBuffer : CmdBuffers)
		{
			CmdBuffer->RefreshState();
		}

#if ENABLE_VULKAN
		for (auto* CmdBuffer : SecondaryCmdBuffers)
		{
			CmdBuffer->RefreshState();
		}
#endif
	}

	std::list<FPrimaryCmdBuffer*> CmdBuffers;
#if ENABLE_VULKAN
	std::list<FSecondaryCmdBuffer*> SecondaryCmdBuffers;
#endif
};

#if ENABLE_VULKAN
static inline uint32 GetFormatBitsPerPixel(VkFormat Format)
{
	switch (Format)
	{
	case VK_FORMAT_R32_SFLOAT:
		return 32;

	default:
		break;
	}
	check(0);
	return 0;
}
#endif
