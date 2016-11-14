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

struct FFence
{
	Microsoft::WRL::ComPtr<ID3D12Fence> Fence;
	HANDLE Event = nullptr;
	uint64 FenceSignaledCounter = 0;

	enum EState
	{
		NotSignaled,
		Signaled,
	};
	EState State = EState::NotSignaled;

	void Create(FDevice& InDevice)
	{
		checkD3D12(InDevice.Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), &Fence));
		Event = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		check(Event != nullptr);
		FenceSignaledCounter = 1;
	}

	void Destroy()
	{
		Fence = nullptr;
	}

	void Wait(uint64 TimeInNanoseconds = 0xffffffff)
	{
		check(State == EState::NotSignaled);
#if ENABLE_VULKAN
		checkVk(vkWaitForFences(Device, 1, &Fence, true, TimeInNanoseconds));
#endif
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
			auto CompletedValue = Fence->GetCompletedValue();
			if (CompletedValue > FenceSignaledCounter)
			{
				check(CompletedValue == FenceSignaledCounter + 1);
				++FenceSignaledCounter;
				State = EState::Signaled;
			}
		}
	}
};

struct FCmdBuffer
{
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> CommandList;

	enum class EState
	{
		ReadyForBegin,
		Begun,
		Ended,
		Submitted,
		InsideRenderPass,
	};
	EState State = EState::ReadyForBegin;

	void Destroy(ID3D12CommandAllocator* Allocator)
	{
		if (State == EState::Submitted)
		{
			RefreshState(Allocator);
			if (Fence.IsNotSignaled())
			{
				const uint64 TimeToWaitInNanoseconds = 5;
				Fence.Wait(TimeToWaitInNanoseconds);
			}
			RefreshState(Allocator);
		}
#if ENABLE_VULKAN
		vkFreeCommandBuffers(Device, Pool, 1, &CmdBuffer);
		CmdBuffer = VK_NULL_HANDLE;
#endif
		Fence.Destroy();
		CommandList = nullptr;
	}

#if ENABLE_VULKAN
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
			Fence.Wait();
			RefreshState();
		}
	}
#endif
	void RefreshState(ID3D12CommandAllocator* Allocator)
	{
		if (State == EState::Submitted)
		{
			uint64 PrevCounter = Fence.FenceSignaledCounter;
			Fence.RefreshState();
			if (PrevCounter != Fence.FenceSignaledCounter)
			{
				checkD3D12(CommandList->Reset(Allocator, nullptr));
				State = EState::ReadyForBegin;
			}
		}
	}

	void End()
	{
		check(State == EState::Begun);
		checkD3D12(CommandList->Close());
		State = EState::Ended;
	}

	FFence Fence;

	void Create(FDevice& InDevice, ID3D12CommandAllocator* Allocator)
	{
		checkD3D12(InDevice.Device->CreateCommandList(1, D3D12_COMMAND_LIST_TYPE_DIRECT, Allocator, nullptr, _uuidof(ID3D12GraphicsCommandList), &CommandList));
		Fence.Create(InDevice);
	}

	void Begin()
	{
		check(State == EState::ReadyForBegin);
		State = EState::Begun;
	}
};

#if ENABLE_VULKAN
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
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> Allocator;

	void Create(FDevice& Device)
	{
		checkD3D12(Device.Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), &Allocator));
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
		for (auto* CB : CmdBuffers)
		{
			CB->RefreshState(Allocator.Get());
			CB->Destroy(Allocator.Get());
			delete CB;
		}
		CmdBuffers.clear();

#if ENABLE_VULKAN
		vkDestroyCommandPool(Device, Pool, nullptr);
		Pool = VK_NULL_HANDLE;
#endif
	}

	FCmdBuffer* AllocateCmdBuffer(FDevice& Device)
	{
		for (auto* CmdBuffer : CmdBuffers)
		{
			CmdBuffer->RefreshState(Allocator.Get());
			if (CmdBuffer->State == FCmdBuffer::EState::ReadyForBegin)
			{
				return CmdBuffer;
			}
		}

		auto* NewCmdBuffer = new FCmdBuffer;
		NewCmdBuffer->Create(Device, Allocator.Get());
		CmdBuffers.push_back(NewCmdBuffer);
		return NewCmdBuffer;
	}

	FCmdBuffer* GetActiveCmdBuffer(FDevice& Device)
	{
		for (auto* CB : CmdBuffers)
		{
			switch (CB->State)
			{
			case FCmdBuffer::EState::Submitted:
				CB->RefreshState(Allocator.Get());
				break;
			case FCmdBuffer::EState::ReadyForBegin:
				return CB;
			default:
				break;
			}
		}

		return AllocateCmdBuffer(Device);
	}

	void Submit(FDevice& Device, FCmdBuffer* CmdBuffer/*, VkQueue Queue, FSemaphore* WaitSemaphore, FSemaphore* SignaledSemaphore*/)
	{
		check(CmdBuffer->State == FCmdBuffer::EState::Ended);
		ID3D12CommandList* CmdList = CmdBuffer->CommandList.Get();
		Device.Queue->ExecuteCommandLists(1, &CmdList);
		checkD3D12(Device.Queue->Signal(CmdBuffer->Fence.Fence.Get(), CmdBuffer->Fence.FenceSignaledCounter + 1));
		CmdBuffer->Fence.State = FFence::EState::NotSignaled;
		CmdBuffer->State = FCmdBuffer::EState::Submitted;
		Update();
	}

	void Update()
	{
		for (auto* CmdBuffer : CmdBuffers)
		{
			CmdBuffer->RefreshState(Allocator.Get());
		}
	}

	std::list<FCmdBuffer*> CmdBuffers;
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
