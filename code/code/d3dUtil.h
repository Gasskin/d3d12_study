#pragma once
#include <windows.h>
#include <wrl.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>
#include <string>
#include <memory>
#include <algorithm>
#include <vector>
#include <array>
#include <unordered_map>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <cassert>
#include "d3dx12.h"

//=======================================================================================
// string转wstring
//=======================================================================================
inline std::wstring AnsiToWString(const std::string& str)
{
    WCHAR buffer[512];
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
    return std::wstring(buffer);
}


//=======================================================================================
//错误类与debug宏
//d3d的api多以hresult为返回值
//ThrowIfFailed宏可以检测hresult并抛出异常（如果有的话）
//=======================================================================================
class DxException
{
public:
    DxException() = default;
    DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber);

    std::wstring ToString() const;

    HRESULT ErrorCode = S_OK;
    std::wstring FunctionName;
    std::wstring Filename;
    int LineNumber = -1;
};

#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                 \
    {                                                    \
        HRESULT hr__ = (x);                              \
        std::wstring wfn = AnsiToWString(__FILE__);      \
        if (FAILED(hr__))                                \
        {                                                \
            throw DxException(hr__, L#x, wfn, __LINE__); \
        }                                                \
    }
#endif

//=======================================================================================
//工具类
//=======================================================================================
class d3dUtil
{
public:

	static UINT CalcConstantBufferByteSize(UINT byteSize)
	{
		return (byteSize + 255) & ~255;
	}


	static Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmdList,
		const void* initData,
		UINT64 byteSize,
		Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);

	static Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
		const std::wstring& filename,
		const D3D_SHADER_MACRO* defines,
		const std::string& entrypoint,
		const std::string& target);
};

//=======================================================================================
//几何辅助体
//=======================================================================================
struct SubmeshGeometry
{
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	INT BaseVertexLocation = 0;

	DirectX::BoundingBox Bounds;
};

struct MeshGeometry
{
	std::string Name;

	Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> vPosBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> vColorBufferGPU = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> vPosBufferUploader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> vColorBufferUploader = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

	UINT vPosByteStride = 0;//顶点缓冲区总大小
	UINT vPosBufferByteSize = 0;//每一个顶点的大小
	UINT vColorByteStride = 0;//顶点缓冲区总大小
	UINT vColorBufferByteSize = 0;//每一个顶点的大小

	DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
	UINT IndexBufferByteSize = 0;

	std::unordered_map<std::string, SubmeshGeometry> DrawArgs;

	D3D12_VERTEX_BUFFER_VIEW vPosBufferView()const
	{
		D3D12_VERTEX_BUFFER_VIEW vbv;
		vbv.BufferLocation = vPosBufferGPU->GetGPUVirtualAddress();
		vbv.StrideInBytes = vPosByteStride;
		vbv.SizeInBytes = vPosBufferByteSize;

		return vbv;
	}

	D3D12_VERTEX_BUFFER_VIEW vColorBufferView()const
	{
		D3D12_VERTEX_BUFFER_VIEW vbv;
		vbv.BufferLocation = vColorBufferGPU->GetGPUVirtualAddress();
		vbv.StrideInBytes = vColorByteStride;
		vbv.SizeInBytes = vColorBufferByteSize;

		return vbv;
	}

	D3D12_INDEX_BUFFER_VIEW IndexBufferView()const
	{
		D3D12_INDEX_BUFFER_VIEW ibv;
		ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
		ibv.Format = IndexFormat;
		ibv.SizeInBytes = IndexBufferByteSize;

		return ibv;
	}

	void DisposeUploaders()
	{
		vPosBufferUploader = nullptr;
		vColorBufferUploader = nullptr;
		IndexBufferUploader = nullptr;
	}
};