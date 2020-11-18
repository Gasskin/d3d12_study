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
