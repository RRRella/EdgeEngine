#pragma once

struct ID3D12CommandQueue;
class Device;

class CommandQueue
{
public:
	enum ECommandQueueType
	{
		GFX = 0,
		COMPUTE,
		COPY,

		NUM_COMMAND_QUEUE_TYPES
	};

public:
	void Create(Device* pDevice, ECommandQueueType type);
	void Destroy();

	ID3D12CommandQueue* pQueue;
};