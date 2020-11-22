#pragma once

#include "d3dUtil.h"
#include "MathHelper.h"
#include "UploadBuffer.h"

struct ObjectConstants
{
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
};

struct PassConstants
{
	DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
};

struct Vertex
{
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT4 Color;
};

struct FrameResource
{
public:

	FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT waveVertCount);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource();

	//ÿһ֡��Դ���������Լ��ķ������볣��������
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;
	std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;
	std::unique_ptr<UploadBuffer<Vertex>> WavesVB = nullptr;

	UINT64 Fence = 0;
};