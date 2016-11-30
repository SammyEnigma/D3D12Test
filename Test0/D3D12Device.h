// Base Vulkan header classes

#pragma once

#include "Util.h"
#include <dxgi1_4.h>
#include <d3d12.h>
#include <wrl.h>
#include <d3dcompiler.h>

struct FInstance
{
	void CreateInstance()
	{
		SetupDebugLayer();
		//DXGI_CREATE_FACTORY_DEBUG
		checkD3D12(CreateDXGIFactory1(IID_PPV_ARGS(&DXGIFactory)));
	}

	void SetupDebugLayer();

	void DestroyInstance()
	{
		DebugInterface = nullptr;
		DXGIFactory = nullptr;
	}

	void Create(HINSTANCE hInstance, HWND hWnd)
	{
		CreateInstance();
	}

	void CreateDevice(struct FDevice& OutDevice);

	void Destroy()
	{
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
		checkD3D12(D3D12CreateDevice(Adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), &Device));

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
	//HANDLE Event = nullptr;
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
		//Event = CreateEvent(nullptr, false, false, nullptr);
		//check(Event != nullptr);
		FenceSignaledCounter = 1;
	}

	void Destroy()
	{
		Fence = nullptr;
	}

	void Wait(uint64 TimeInNanoseconds = 0xffffffff)
	{
		while (State == EState::NotSignaled)
		{
			::Sleep(((uint32)TimeInNanoseconds + 1000) / 1000);
			RefreshState();
		}
#if ENABLE_VULKAN
		checkVk(vkWaitForFences(Device, 1, &Fence, true, TimeInNanoseconds));
#endif
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
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> Allocator;

	enum class EState
	{
		ReadyForBegin,
		Begun,
		Ended,
		Submitted,
		InsideRenderPass,
	};
	EState State = EState::ReadyForBegin;

	void Destroy()
	{
		if (State == EState::Submitted)
		{
			RefreshState();
			if (Fence.IsNotSignaled())
			{
				const uint64 TimeToWaitInNanoseconds = 5;
				Fence.Wait(TimeToWaitInNanoseconds);
			}
			RefreshState();
		}
		Fence.Destroy();
		CommandList = nullptr;
		Allocator = nullptr;
	}

#if ENABLE_VULKAN
	void BeginRenderPass(VkRenderPass RenderPass, const struct FFramebuffer& Framebuffer, bool bHasSecondary);

	void EndRenderPass()
	{
		check(State == EState::InsideRenderPass);

		vkCmdEndRenderPass(CmdBuffer);

		State = EState::Begun;
	}
#endif
	void WaitForFence()
	{
		if (State == EState::Submitted)
		{
			Fence.Wait();
			RefreshState();
		}
	}

	void RefreshState()
	{
		if (State == EState::Submitted)
		{
			uint64 PrevCounter = Fence.FenceSignaledCounter;
			Fence.RefreshState();
			if (PrevCounter != Fence.FenceSignaledCounter)
			{
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

	void Create(FDevice& InDevice)
	{
		checkD3D12(InDevice.Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), &Allocator));
		checkD3D12(InDevice.Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, Allocator.Get(), nullptr, _uuidof(ID3D12GraphicsCommandList), &CommandList));
		Fence.Create(InDevice);
		checkD3D12(CommandList->Close());
	}

	void Begin()
	{
		check(State == EState::ReadyForBegin);
		Allocator->Reset();
		CommandList->Reset(Allocator.Get(), nullptr);
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
	void Create(FDevice& Device)
	{
	}

	void Destroy()
	{
		for (auto* CB : CmdBuffers)
		{
			if (CB->State == FCmdBuffer::EState::Submitted)
			{
				CB->WaitForFence();
			}
			CB->Destroy();
			delete CB;
		}
		CmdBuffers.clear();
	}

	FCmdBuffer* AllocateCmdBuffer(FDevice& Device)
	{
		for (auto* CmdBuffer : CmdBuffers)
		{
			CmdBuffer->RefreshState();
			if (CmdBuffer->State == FCmdBuffer::EState::ReadyForBegin)
			{
				return CmdBuffer;
			}
		}

		auto* NewCmdBuffer = new FCmdBuffer;
		NewCmdBuffer->Create(Device);
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
				CB->RefreshState();
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
			CmdBuffer->RefreshState();
		}
	}

	std::list<FCmdBuffer*> CmdBuffers;
};

static inline uint32 GetFormatBitsPerPixel(DXGI_FORMAT Format)
{
	switch (Format)
	{
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_R32_SINT:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		return 32;

	default:
		break;
	}
	check(0);
	return 0;
}
