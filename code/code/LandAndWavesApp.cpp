#include "d3dApp.h"
#include "UploadBuffer.h"
#include "MathHelper.h"
#include <WindowsX.h>
#include "GeometryGenerator.h"
#include "FrameResource.h"
#include "Waves.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

//====================================================
//额外信息
//====================================================

//帧资源数量
const int gNumFrameResources = 3;

//渲染项
struct RenderItem
{
	RenderItem() = default;

	//该几何体的世界矩阵
	XMFLOAT4X4 world = MathHelper::Identity4x4();

	//该几何体的常量数据在objConstantBuffer中的索引
	UINT objCBIndex = -1;

	//帧资源数量
	int NumFramesDirty = gNumFrameResources;

	//该几何体的图元拓扑类型
	D3D12_PRIMITIVE_TOPOLOGY primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	//该几何体的绘制三参数
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	UINT BaseVertexLocation = 0;

	MeshGeometry* geo = nullptr;
};

//====================================================
//主类
//====================================================
class LandAndWavesApp :public D3DApp
{
public:
	LandAndWavesApp(HINSTANCE hInstance);
	LandAndWavesApp(const LandAndWavesApp& rhs) = delete;
	LandAndWavesApp& operator=(const LandAndWavesApp& rhs) = delete;
	~LandAndWavesApp();

	virtual bool Initialize()override;

private:
	virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)override;
	virtual void	Update(const GameTimer& gt)override;
	virtual void	Draw(const GameTimer& gt)override;
	virtual void	OnResize()override;
private:
	void BuildDescriptorHeaps();//创建描述符堆
	void BuildConstantBuffers();//创建常量缓冲区与视图
	void BuildRootSignature();//创建根签名
	void BuildShadersAndInputLayout();//创建顶点输入布局
	void BuildGeometry();//构建几何体（顶点与索引）

	void BuildPSO_SOLID();//创建流水线对象
	void BuildPSO_WIREFRAME();//创建流水线对象

	void BuildRenderItems();//渲染项
	void DrawRenderItems();

	void BuildFrameResources();//帧资源

	float GetHillsHeight(float x,float z);
	void BuildWavesGeometryBuffers();
	void UpdateWaves(const GameTimer& gt);
private:
	void OnMouseDown(WPARAM btnState, int x, int y);
	void OnMouseUp(WPARAM btnState, int x, int y);
	void OnMouseMove(WPARAM btnState, int x, int y);
private:
	ComPtr<ID3D12DescriptorHeap>									mCbvHeap = nullptr;//常量缓冲区 描述符堆

	ComPtr<ID3D12RootSignature>										mRootSignature = nullptr;//根签名

	ComPtr<ID3DBlob>												mvsByteCode = nullptr;//顶点着色器
	ComPtr<ID3DBlob>												mpsByteCode = nullptr;//像素着色器
	std::vector<D3D12_INPUT_ELEMENT_DESC>							mInputLayout;//输入布局

	ComPtr<ID3D12PipelineState>										mPSO_SOLID = nullptr;//流水线对象,实体
	ComPtr<ID3D12PipelineState>										mPSO_WIREFRAME = nullptr;//流水线对象,线框

	std::vector<std::unique_ptr<RenderItem>>						mAllRitems;//暂时不能理解为什么需要两个vector来创建渲染项
	std::vector<RenderItem*>										mOpaqueRitems;

	std::vector<std::unique_ptr<FrameResource>>						mFrameResources;//帧资源
	FrameResource* mCurrFrameResource = nullptr;
	int																mCurrFrameResourceIndex = 0;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>>	mGeometries;//所有物体的顶点与索引缓冲

	std::unique_ptr<Waves>											mWaves;
	RenderItem*														mWavesRitem = nullptr;
private:
	XMFLOAT4X4	mWorld = MathHelper::Identity4x4();//世界矩阵
	XMFLOAT4X4	mView = MathHelper::Identity4x4();//观察矩阵
	XMFLOAT4X4	mProj = MathHelper::Identity4x4();//投影矩阵

	float		mTheta = 1.5f * XM_PI; //1.5pai
	float		mPhi = XM_PIDIV4;//四分之pai
	float		mRadius = 5.0f;//弧度
private:
	bool mEnableWireframe = false;
private:
	POINT	mLastMousePos;
};

LandAndWavesApp::LandAndWavesApp(HINSTANCE hInstance)
	:D3DApp(hInstance)
{
}

LandAndWavesApp::~LandAndWavesApp()
{
}

//入口
bool LandAndWavesApp::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	//重置命令列表与命令分配器
	ThrowIfFailed(mDirectCmdListAlloc->Reset());
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));
	//初始化
	BuildGeometry();
	BuildRenderItems();
	BuildFrameResources();
	BuildDescriptorHeaps();
	BuildConstantBuffers();
	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildPSO_SOLID();
	BuildPSO_WIREFRAME();
	//关闭命令列表
	ThrowIfFailed(mCommandList->Close());
	//执行命令
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	//围栏
	FlushCommandQueue();
	return true;
}

void LandAndWavesApp::Update(const GameTimer& gt)
{
	float x = mRadius * sinf(mPhi) * cosf(mTheta);
	float z = mRadius * sinf(mPhi) * sinf(mTheta);
	float y = mRadius * cosf(mPhi);

	XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);

	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for (auto& e : mAllRitems)
	{
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->world);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));

			currObjectCB->CopyData(e->objCBIndex, objConstants);

			e->NumFramesDirty--;
		}
	}

	PassConstants passConstants;
	XMMATRIX proj = XMLoadFloat4x4(&mProj);
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMStoreFloat4x4(&passConstants.ViewProj, XMMatrixTranspose(viewProj));
	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, passConstants);

	UpdateWaves(gt);
}

void LandAndWavesApp::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	ThrowIfFailed(cmdListAlloc->Reset());

	if (mEnableWireframe)
	{
		ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSO_WIREFRAME.Get()));
	}
	else
	{
		ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSO_SOLID.Get()));
	}

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(mSwapChainBuffer[mCurrBackBuffer].Get(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET)
	);

	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	//设置根签名
	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(1,
		passCB->GetGPUVirtualAddress());

	DrawRenderItems();

	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(mSwapChainBuffer[mCurrBackBuffer].Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(mCommandList->Close());

	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	//围栏定点
	mCurrFrameResource->Fence = ++mCurrentFence;
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void LandAndWavesApp::OnResize()
{
	D3DApp::OnResize();

	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);
}

void LandAndWavesApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;
	SetCapture(mhMainWnd);
}

void LandAndWavesApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void LandAndWavesApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	//如果在左键按下状态
	if ((btnState & MK_LBUTTON) != 0)
	{
		//将鼠标的移动距离换算成弧度，0.25为调节阈值
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));
		//计算鼠标没有松开前的累计弧度
		mTheta += dx * 2.f;
		mPhi += dy * 2.f;
		//限制角度phi的范围在（0.1， Pi-0.1）
		//mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
	}
	//如果在右键按下状态
	else if ((btnState & MK_RBUTTON) != 0)
	{
		//将鼠标的移动距离换算成缩放大小，0.005为调节阈值
		float dx = 0.005f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.005f * static_cast<float>(y - mLastMousePos.y);
		//根据鼠标输入更新摄像机可视范围半径
		mRadius += (dx - dy) * 15.f;
		//限制可视范围半径
		//mRadius = MathHelper::Clamp(mRadius, 3.0f, 15.0f);
	}
	//将当前鼠标坐标赋值给“上一次鼠标坐标”，为下一次鼠标操作提供先前值
	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

LRESULT LandAndWavesApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			OutputDebugString(L"取消激活窗口\n");
			mAppPaused = true;
			mTimer.Stop();
		}
		else
		{
			OutputDebugString(L"激活窗口\n");
			mAppPaused = false;
			mTimer.Start();
		}
		return 0;
	case WM_SIZE:
		OutputDebugString(L"改变窗口大小 ");
		mClientWidth = LOWORD(lParam);
		mClientHeight = HIWORD(lParam);
		if (md3dDevice)
		{
			if (wParam == SIZE_MINIMIZED)
			{
				mAppPaused = true;
				mMinimized = true;
				mMaximized = false;
			}
			else if (wParam == SIZE_MAXIMIZED)
			{
				mAppPaused = false;
				mMinimized = false;
				mMaximized = true;
				OnResize();
			}
			else if (wParam == SIZE_RESTORED)
			{
				if (mMinimized)
				{
					mAppPaused = false;
					mMinimized = false;
					OnResize();
				}
				else if (mMaximized)
				{
					mAppPaused = false;
					mMaximized = false;
					OnResize();
				}
				else if (mResizing)
				{
					/*
						我们可以一直调整窗口大小，但如果同时也一直重新绘制，那是很没必要的，我们只会在调整完之后，进行一次重新绘制
						下面还有两个case，一个是当我们开始调整窗口大小时，另一个是当我们结束调整窗口大小时
						所以当我们开始调整窗口大小时，设置mResizing为true，这样，再WM_SIZE这个消息中，就会一直进入现在这个空分支，啥也不会干
						等绘制结束时，我们再设置mResizing为false，进行一次OnResize()
					*/
				}
				else
				{
					OnResize();
				}
			}
		}
	case WM_ENTERSIZEMOVE:
		mAppPaused = true;
		mResizing = true;
		mTimer.Stop();
		return 0;
	case WM_EXITSIZEMOVE:
		mAppPaused = false;
		mResizing = false;
		mTimer.Start();
		OnResize();
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_MENUCHAR:
		return MAKELRESULT(0, MNC_CLOSE);
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		return 0;
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_KEYUP:
		if (wParam == VK_ESCAPE)
		{
			PostQuitMessage(0);
		}
		else if (wParam == VK_F1)
		{
			mEnableWireframe = true;
		}
		else if (wParam == VK_F2)
		{
			mEnableWireframe = false;
		}
		return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		LandAndWavesApp theApp(hInstance);
		if (!theApp.Initialize())
			return 0;
		return theApp.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}

/*
	缓冲区想要绑定到流水线，需要借助缓冲区视图，而部分视图，需要储存在描述符堆中，如常量缓冲视图，DSV,RTV等
	当然也有一部分视图是不需要描述符堆的，如顶点缓冲视图，索引缓冲视图
	这里我们创建的描述符堆，主要是用于储存常量缓冲区视图的
*/
void LandAndWavesApp::BuildDescriptorHeaps()
{
	
}

void LandAndWavesApp::BuildConstantBuffers()
{
	
}

void LandAndWavesApp::BuildRootSignature()
{
	//根参数可以是描述符表、根描述符、根常量
	CD3DX12_ROOT_PARAMETER slotRootParameter[2];
	slotRootParameter[0].InitAsConstantBufferView(0);
	slotRootParameter[1].InitAsConstantBufferView(1);


	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, //根参数数量修改为2
		slotRootParameter, 0, nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr =
		D3D12SerializeRootSignature(
			&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
			serializedRootSig.GetAddressOf(),
			errorBlob.GetAddressOf()
		);

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(
		md3dDevice->CreateRootSignature(
			0,
			serializedRootSig->GetBufferPointer(),
			serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(&mRootSignature))
	);
}

void LandAndWavesApp::BuildShadersAndInputLayout()
{
	HRESULT hr = S_OK;

	mvsByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "VS", "vs_5_0");
	mpsByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "PS", "ps_5_0");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void LandAndWavesApp::BuildGeometry()
{
	//这一步是创建顶点缓冲与索引缓冲
	GeometryGenerator proceGeo;
	GeometryGenerator::MeshData grid = proceGeo.CreateGrid(160.0f, 160.0f, 50, 50);//创建平面

	SubmeshGeometry LandSubmesh;
	LandSubmesh.IndexCount = (UINT)grid.Indices32.size();
	LandSubmesh.BaseVertexLocation = 0;
	LandSubmesh.StartIndexLocation = 0;

	//创建顶点缓存
	size_t VertexCount = grid.Vertices.size();
	std::vector<Vertex> vertices(VertexCount);	//给定顶点数组大小
	for (int i = 0; i < grid.Vertices.size(); i++)
	{
		vertices[i].Pos = grid.Vertices[i].Position;
		vertices[i].Pos.y = GetHillsHeight(vertices[i].Pos.x, vertices[i].Pos.z);

		//根据顶点不同的y值，给予不同的顶点色(不同海拔对应的颜色)
		if (vertices[i].Pos.y < -10.0f)
		{
			vertices[i].Color = XMFLOAT4(1.0f, 0.96f, 0.62f, 1.0f);
		}
		else if (vertices[i].Pos.y < 5.0f)
		{
			vertices[i].Color = XMFLOAT4(0.48f, 0.77f, 0.46f, 1.0f);
		}
		else if (vertices[i].Pos.y < 12.0f)
		{
			vertices[i].Color = XMFLOAT4(0.1f, 0.48f, 0.19f, 1.0f);
		}
		else if (vertices[i].Pos.y < 20.0f)
		{
			vertices[i].Color = XMFLOAT4(0.45f, 0.39f, 0.34f, 1.0f);
		}
		else
		{
			vertices[i].Color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		}
	}
	//创建索引缓存
	std::vector<std::uint16_t> indices = grid.GetIndices16();

	//计算出顶点缓冲与索引缓冲的大小
	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	//创建缓冲区
	auto mGeo = std::make_unique<MeshGeometry>();
	mGeo->Name = "landGeo";

	mGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, mGeo->VertexBufferUploader);

	mGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, mGeo->IndexBufferUploader);

	mGeo->VertexByteStride = sizeof(Vertex);
	mGeo->VertexBufferByteSize = vbByteSize;
	mGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
	mGeo->IndexBufferByteSize = ibByteSize;

	mGeo->DrawArgs["grid"] = LandSubmesh;

	mGeometries["landGeo"] = std::move(mGeo);

	BuildWavesGeometryBuffers();
}

void LandAndWavesApp::BuildPSO_SOLID()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	psoDesc.pRootSignature = mRootSignature.Get();
	psoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()),
		mvsByteCode->GetBufferSize()
	};
	psoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()),
		mpsByteCode->GetBufferSize()
	};
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = mBackBufferFormat;
	psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	psoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO_SOLID)));
}

void LandAndWavesApp::BuildPSO_WIREFRAME()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	psoDesc.pRootSignature = mRootSignature.Get();
	psoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()),
		mvsByteCode->GetBufferSize()
	};
	psoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()),
		mpsByteCode->GetBufferSize()
	};
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = mBackBufferFormat;
	psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	psoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO_WIREFRAME)));
}

void LandAndWavesApp::BuildRenderItems()
{
	auto wavesRitem = std::make_unique<RenderItem>();
	wavesRitem->world = MathHelper::Identity4x4();
	wavesRitem->objCBIndex = 0;
	wavesRitem->geo = mGeometries["waterGeo"].get();
	wavesRitem->primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wavesRitem->IndexCount = wavesRitem->geo->DrawArgs["grid"].IndexCount;
	wavesRitem->StartIndexLocation = wavesRitem->geo->DrawArgs["grid"].StartIndexLocation;
	wavesRitem->BaseVertexLocation = wavesRitem->geo->DrawArgs["grid"].BaseVertexLocation;

	mWavesRitem = wavesRitem.get();


	auto landRitem = std::make_unique<RenderItem>();
	landRitem->world = MathHelper::Identity4x4();
	landRitem->objCBIndex = 1;
	landRitem->geo = mGeometries["landGeo"].get();
	landRitem->primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	landRitem->IndexCount = landRitem->geo->DrawArgs["grid"].IndexCount;
	landRitem->StartIndexLocation = landRitem->geo->DrawArgs["grid"].StartIndexLocation;
	landRitem->BaseVertexLocation = landRitem->geo->DrawArgs["grid"].BaseVertexLocation;


	mAllRitems.push_back(std::move(wavesRitem));
	mAllRitems.push_back(std::move(landRitem));

	for (auto& e : mAllRitems)
		mOpaqueRitems.push_back(e.get());
}

void LandAndWavesApp::DrawRenderItems()
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	//遍历渲染项数组
	for (size_t i = 0; i < mOpaqueRitems.size(); i++)
	{
		auto ritem = mOpaqueRitems[i];

		mCommandList->IASetVertexBuffers(0, 1, &ritem->geo->VertexBufferView());
		mCommandList->IASetIndexBuffer(&ritem->geo->IndexBufferView());
		mCommandList->IASetPrimitiveTopology(ritem->primitiveType);

		//设置根描述符,将根描述符与资源绑定
		auto objCB = mCurrFrameResource->ObjectCB->Resource();
		auto objCBAddress = objCB->GetGPUVirtualAddress();
		objCBAddress += ritem->objCBIndex * objCBByteSize;
		mCommandList->SetGraphicsRootConstantBufferView(0,//寄存器槽号
		objCBAddress);//子资源地址


		//绘制顶点（通过索引缓冲区绘制）
		mCommandList->DrawIndexedInstanced(ritem->IndexCount, //每个实例要绘制的索引数
			1,	//实例化个数
			ritem->StartIndexLocation,	//起始索引位置
			ritem->BaseVertexLocation,	//子物体起始索引在全局索引中的位置
			0);	//实例化的高级技术，暂时设置为0
	}
}

void LandAndWavesApp::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(
			md3dDevice.Get(),
			1, 
			(UINT)mAllRitems.size(),
			mWaves->VertexCount()
		));
	}
}

float LandAndWavesApp::GetHillsHeight(float x, float z)
{
	return 0.3f * (z * sinf(0.1f * x) + x * cosf(0.1f * z));
}

void LandAndWavesApp::BuildWavesGeometryBuffers()
{
	mWaves = std::make_unique<Waves>(128, 128, 1.0f, 0.03f, 4.0f, 0.2f);

	//初始化索引列表（每个三角形3个索引）
	std::vector<std::uint16_t> indices(3 * mWaves->TriangleCount());
	assert(mWaves->VertexCount() < 0x0000ffff);//顶点索引数大于65536则中止程序

	//填充索引列表
	int m = mWaves->RowCount();
	int n = mWaves->ColumnCount();
	int k = 0;
	for (int i = 0; i < m - 1; i++)
	{
		for (int j = 0; j < n - 1; j++)
		{
			indices[k] = i * n + j;
			indices[k + 1] = i * n + j + 1;
			indices[k + 2] = (i + 1) * n + j;

			indices[k + 3] = (i + 1) * n + j;
			indices[k + 4] = i * n + j + 1;
			indices[k + 5] = (i + 1) * n + j + 1;

			k += 6;
		}
	}
	//计算顶点和索引缓存大小
	UINT vbByteSize = mWaves->VertexCount() * sizeof(Vertex);
	UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "waterGeo";

	geo->VertexBufferGPU = nullptr;
	//创建索引的CPU系统内存
	
	//将索引数据通过上传堆传至默认堆
	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(
		md3dDevice.Get(),
		mCommandList.Get(),
		indices.data(),
		ibByteSize,
		geo->IndexBufferUploader);

	//赋值MeshGeomety中相关属性
	geo->VertexBufferByteSize = vbByteSize;
	geo->VertexByteStride = sizeof(Vertex);
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry LakeSubmesh;
	LakeSubmesh.BaseVertexLocation = 0;
	LakeSubmesh.StartIndexLocation = 0;
	LakeSubmesh.IndexCount = (UINT)indices.size();

	//使用grid几何体
	geo->DrawArgs["grid"] = LakeSubmesh;
	//湖泊的MeshGeometry
	mGeometries["waterGeo"] = std::move(geo);
}

void LandAndWavesApp::UpdateWaves(const GameTimer& gt)
{
	static float t_base = 0.0f;
	if ((gt.TotalTime() - t_base) >= 0.25f)
	{
		t_base += 0.25f;	//0.25秒生成一个波浪
		//随机生成横坐标
		int i = MathHelper::Rand(4, mWaves->RowCount() - 5);
		//随机生成纵坐标
		int j = MathHelper::Rand(4, mWaves->ColumnCount() - 5);
		//随机生成波的半径
		float r = MathHelper::RandF(0.2f, 0.5f);//float用RandF函数
		//使用波动方程函数生成波纹
		mWaves->Disturb(i, j, r);
	}

	//每帧更新波浪模拟（即更新顶点坐标）
	mWaves->Update(gt.DeltaTime());

	//将更新的顶点坐标存入GPU上传堆中
	auto currWavesVB = mCurrFrameResource->WavesVB.get();
	for (int i = 0; i < mWaves->VertexCount(); i++)
	{
		Vertex v;
		v.Pos = mWaves->Position(i);
		v.Color = XMFLOAT4(DirectX::Colors::Blue);

		currWavesVB->CopyData(i, v);
	}
	//赋值湖泊的GPU上的顶点缓存
	mWavesRitem->geo->VertexBufferGPU = currWavesVB->Resource();
}
