#include "CommandQueue.h"
#include "Device.h"
#include "../Utils/Source/Log.h"

#include <d3d12.h>
#include <cassert>

void CommandQueue::Create(Device* pDevice, ECommandQueueType type)
{
	HRESULT hr = {};
	ID3D12Device* pDevice_ = pDevice->GetDevicePtr();
	D3D12_COMMAND_QUEUE_DESC qDesc = {};

	qDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	qDesc.NodeMask = 0;
	qDesc.Priority = 0;
	switch (type)
	{
	case CommandQueue::GFX     : qDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;  break;
	case CommandQueue::COMPUTE : qDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE; break;
	case CommandQueue::COPY    : qDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;    break;
	default: assert(false); break;
	}

	hr = pDevice_->CreateCommandQueue(&qDesc, IID_PPV_ARGS(&this->pQueue));
	if (FAILED(hr))
	{
		Log::Error("Couldn't create Command List: %s", "TODO:reason");
	}
}

void CommandQueue::Destroy()
{
	if (pQueue) pQueue->Release();
}
