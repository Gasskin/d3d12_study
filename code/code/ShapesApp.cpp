#include "d3dApp.h"
#include "UploadBuffer.h"
#include "MathHelper.h"
#include <WindowsX.h>
#include "GeometryGenerator.h"
#include "FrameResource.h"

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
};

//====================================================
//主类
//====================================================
class ShapesApp :public D3DApp
{
public:
	ShapesApp(HINSTANCE hInstance);
	ShapesApp(const ShapesApp& rhs) = delete;
	ShapesApp& operator=(const ShapesApp& rhs) = delete;
	~ShapesApp();

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
	void BuildPSO();//创建流水线对象

	void BuildRenderItems();//渲染项
	void DrawRenderItems();

	void BuildFrameResources();//帧资源
private:
	void OnMouseDown(WPARAM btnState, int x, int y);
	void OnMouseUp(WPARAM btnState, int x, int y);
	void OnMouseMove(WPARAM btnState, int x, int y);
private:
	ComPtr<ID3D12DescriptorHeap>					mCbvHeap = nullptr;//常量缓冲区 描述符堆

	std::unique_ptr<UploadBuffer<ObjectConstants>>	objCB = nullptr;//M矩阵常量
	std::unique_ptr<UploadBuffer<PassConstants>>	passCB = nullptr;//VP矩阵常量

	ComPtr<ID3D12RootSignature>						mRootSignature = nullptr;//根签名

	ComPtr<ID3DBlob>								mvsByteCode = nullptr;//顶点着色器
	ComPtr<ID3DBlob>								mpsByteCode = nullptr;//像素着色器
	std::vector<D3D12_INPUT_ELEMENT_DESC>			mInputLayout;//输入布局

	std::unique_ptr<MeshGeometry>					mGeos = nullptr;//盒子的顶点与索引缓冲

	ComPtr<ID3D12PipelineState>						mPSO = nullptr;//流水线对象

	std::vector<std::unique_ptr<RenderItem>>		mAllRitems;//暂时不能理解为什么需要两个vector来创建渲染项
	std::vector<RenderItem*>						mOpaqueRitems;

	std::vector<std::unique_ptr<FrameResource>>		mFrameResources;//帧资源
	FrameResource*									mCurrFrameResource = nullptr;
	int												mCurrFrameResourceIndex = 0;
private:
	XMFLOAT4X4	mWorld = MathHelper::Identity4x4();//世界矩阵
	XMFLOAT4X4	mView = MathHelper::Identity4x4();//观察矩阵
	XMFLOAT4X4	mProj = MathHelper::Identity4x4();//投影矩阵

	float		mTheta = 1.5f * XM_PI; //1.5pai
	float		mPhi = XM_PIDIV4;//四分之pai
	float		mRadius = 5.0f;//弧度
private:
	POINT	mLastMousePos;
};

ShapesApp::ShapesApp(HINSTANCE hInstance)
	:D3DApp(hInstance)
{
}

ShapesApp::~ShapesApp()
{
}

//入口
bool ShapesApp::Initialize()
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
	BuildPSO();
	//关闭命令列表
	ThrowIfFailed(mCommandList->Close());
	//执行命令
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	//围栏
	FlushCommandQueue();

	return true;
}

void ShapesApp::Update(const GameTimer& gt)
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
}

void ShapesApp::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	ThrowIfFailed(cmdListAlloc->Reset());

	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSO.Get()));
	
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

	//设置CBV描述符堆
	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	//设置根签名
	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	int passCbvIndex = mAllRitems.size()*gNumFrameResources + mCurrFrameResourceIndex;
	auto handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
	handle.Offset(passCbvIndex, mCbvSrvUavDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(
		1, //根参数的起始索引
		handle);

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

void ShapesApp::OnResize()
{
	D3DApp::OnResize();

	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);
}

void ShapesApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;
	SetCapture(mhMainWnd);
}

void ShapesApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void ShapesApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	//如果在左键按下状态
	if ((btnState & MK_LBUTTON) != 0)
	{
		//将鼠标的移动距离换算成弧度，0.25为调节阈值
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));
		//计算鼠标没有松开前的累计弧度
		mTheta += dx*2.f;
		mPhi += dy*2.f;
		//限制角度phi的范围在（0.1， Pi-0.1）
		mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
	}
	//如果在右键按下状态
	else if ((btnState & MK_RBUTTON) != 0)
	{
		//将鼠标的移动距离换算成缩放大小，0.005为调节阈值
		float dx = 0.005f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.005f * static_cast<float>(y - mLastMousePos.y);
		//根据鼠标输入更新摄像机可视范围半径
		mRadius += (dx - dy)*4.f;
		//限制可视范围半径
		mRadius = MathHelper::Clamp(mRadius, 3.0f, 15.0f);
	}
	//将当前鼠标坐标赋值给“上一次鼠标坐标”，为下一次鼠标操作提供先前值
	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

LRESULT ShapesApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
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
		ShapesApp theApp(hInstance);
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
void ShapesApp::BuildDescriptorHeaps()
{
	UINT objectCount = (UINT)mOpaqueRitems.size();
	//描述 描述符堆
	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NumDescriptors = (objectCount + 1) * gNumFrameResources;//每一个堆里有n个W矩阵+1个VP矩阵，3个帧资源，3个堆
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;

	//创建描述符堆
	ThrowIfFailed(
		md3dDevice->CreateDescriptorHeap(
			&cbvHeapDesc,
			IID_PPV_ARGS(&mCbvHeap))
	);
}

void ShapesApp::BuildConstantBuffers()
{
	//创建M矩阵常量缓冲区
	UINT objectCount = (UINT)mOpaqueRitems.size();
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
	{
		auto objectCB = mFrameResources[frameIndex]->ObjectCB->Resource();
		for (UINT i = 0; i < objectCount; ++i)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = objectCB->GetGPUVirtualAddress();

			//视图在常量缓冲区的位置，第几个物体就偏移几个常量大小
			cbAddress += i * objCBByteSize;

			//视图在描述符堆的位置，注意现在有6个常量缓冲区（3个obj3个pass），但只有1个描述符堆
			int heapIndex = frameIndex * objectCount + i;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = objCBByteSize;

			md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
		}
	}
	

	//创建VP矩阵常量缓冲区

	UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

	for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
	{
		auto passCB = mFrameResources[frameIndex]->PassCB->Resource();
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress();

		//描述符堆中最后3个位置储存3个pass
		int heapIndex = objectCount * gNumFrameResources + frameIndex;
		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
		handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = cbAddress;//pass视图在缓冲区中的位置不偏移，因为每个pass缓冲区只有1个数据
		cbvDesc.SizeInBytes = passCBByteSize;

		md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
	}
}

void ShapesApp::BuildRootSignature()
{
	CD3DX12_ROOT_PARAMETER slotRootParameter[2];

	//创建由单个CBV所组成的描述符表
	CD3DX12_DESCRIPTOR_RANGE cbvTable0;
	cbvTable0.Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_CBV, //描述符类型
		1, //描述符数量
		0);//描述符所绑定的寄存器槽号
	CD3DX12_DESCRIPTOR_RANGE cbvTable1;
	cbvTable1.Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_CBV, //描述符类型
		1, //描述符数量
		1);//描述符所绑定的寄存器槽号
	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);
	slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable1);

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

void ShapesApp::BuildShadersAndInputLayout()
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

void ShapesApp::BuildGeometry()
{
	//这一步是创建顶点缓冲与索引缓冲
	GeometryGenerator proceGeo;
	GeometryGenerator::MeshData box = proceGeo.CreateBox(1.5f, 0.5f, 1.5f, 3);//创建立方体
	GeometryGenerator::MeshData grid = proceGeo.CreateGrid(20.0f, 30.0f, 60, 40);//创建平面
	GeometryGenerator::MeshData sphere = proceGeo.CreateSphere(0.5f, 20, 20);//创建球体
	GeometryGenerator::MeshData cylinder = proceGeo.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);//创建立方体

	//计算单个几何体顶点在总顶点数组中的偏移量,顺序为：box、grid、sphere、cylinder
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
	UINT sphereVertexOffset = (UINT)grid.Vertices.size() + gridVertexOffset;
	UINT cylinderVertexOffset = (UINT)sphere.Vertices.size() + sphereVertexOffset;

	//计算单个几何体索引在总索引数组中的偏移量,顺序为：box、grid、sphere、cylinder
	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT sphereIndexOffset = (UINT)grid.Indices32.size() + gridIndexOffset;
	UINT cylinderIndexOffset = (UINT)sphere.Indices32.size() + sphereIndexOffset;

	//用SubmeshGeometry结构体记录上面获得的偏移数据
	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.BaseVertexLocation = boxVertexOffset;
	boxSubmesh.StartIndexLocation = boxIndexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.BaseVertexLocation = gridVertexOffset;
	gridSubmesh.StartIndexLocation = gridIndexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;

	//然后我们创建一个总的顶点缓存vertices，并将4个子物体的顶点数据存入其中
	size_t totalVertexCount = box.Vertices.size() + grid.Vertices.size() + sphere.Vertices.size() + cylinder.Vertices.size();
	std::vector<Vertex> vertices(totalVertexCount);	//给定顶点数组大小
	int k = 0;
	for (int i = 0; i < box.Vertices.size(); i++, k++)
	{
		//此处容易混淆，vertices[k]是我们创建的顶点结构体，它有Pos和Color两个属性
		//而box.Vertices[i]是用辅助结构体GeometryGenerator创建的，它有是一个顶点结构体，里面有很多属性，我们用它的Position来初始化我们定义的顶点
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Yellow);//不过box.Vertices[i]里没有颜色属性，所以我们自定义了
	}
	for (int i = 0; i < grid.Vertices.size(); i++, k++)
	{
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Brown);
	}
	for (int i = 0; i < sphere.Vertices.size(); i++, k++)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Green);
	}
	for (int i = 0; i < cylinder.Vertices.size(); i++, k++)
	{
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Blue);
	}
	//同理我们创建一个总的索引缓存indices
	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), box.GetIndices16().begin(), box.GetIndices16().end());
	indices.insert(indices.end(), grid.GetIndices16().begin(), grid.GetIndices16().end());
	indices.insert(indices.end(), sphere.GetIndices16().begin(), sphere.GetIndices16().end());
	indices.insert(indices.end(), cylinder.GetIndices16().begin(), cylinder.GetIndices16().end());

	//计算出顶点缓冲与索引缓冲的大小
	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	//创建缓冲区
	mGeos = std::make_unique<MeshGeometry>();
	mGeos->Name = "shapeGeo";

	mGeos->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, mGeos->VertexBufferUploader);

	mGeos->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, mGeos->IndexBufferUploader);

	mGeos->VertexByteStride = sizeof(Vertex);
	mGeos->VertexBufferByteSize = vbByteSize;
	mGeos->IndexFormat = DXGI_FORMAT_R16_UINT;
	mGeos->IndexBufferByteSize = ibByteSize;

	mGeos->DrawArgs["box"] = boxSubmesh;
	mGeos->DrawArgs["grid"] = gridSubmesh;
	mGeos->DrawArgs["sphere"] = sphereSubmesh;
	mGeos->DrawArgs["cylinder"] = cylinderSubmesh;
}

void ShapesApp::BuildPSO()
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
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));
}

void ShapesApp::BuildRenderItems()
{
	auto boxRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&(boxRitem->world), XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(0.0f, 0.5f, 0.0f));
	boxRitem->objCBIndex = 0;//BOX常量数据（world矩阵）在objConstantBuffer索引0上
	boxRitem->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = mGeos->DrawArgs["box"].IndexCount;
	boxRitem->BaseVertexLocation = mGeos->DrawArgs["box"].BaseVertexLocation;
	boxRitem->StartIndexLocation = mGeos->DrawArgs["box"].StartIndexLocation;
	mAllRitems.push_back(std::move(boxRitem));

	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->world = MathHelper::Identity4x4();
	gridRitem->objCBIndex = 1;//BOX常量数据（world矩阵）在objConstantBuffer索引1上
	gridRitem->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->IndexCount = mGeos->DrawArgs["grid"].IndexCount;
	gridRitem->BaseVertexLocation = mGeos->DrawArgs["grid"].BaseVertexLocation;
	gridRitem->StartIndexLocation = mGeos->DrawArgs["grid"].StartIndexLocation;
	mAllRitems.push_back(std::move(gridRitem));

	UINT fllowObjCBIndex = 2;//接下去的几何体常量数据在CB中的索引从2开始
	//将圆柱和圆的实例模型存入渲染项中
	for (int i = 0; i < 5; i++)
	{
		auto leftCylinderRitem = std::make_unique<RenderItem>();
		auto rightCylinderRitem = std::make_unique<RenderItem>();
		auto leftSphereRitem = std::make_unique<RenderItem>();
		auto rightSphereRitem = std::make_unique<RenderItem>();

		XMMATRIX leftCylWorld = XMMatrixTranslation(-5.0f, 1.5f, -10.0f + i * 5.0f);
		XMMATRIX rightCylWorld = XMMatrixTranslation(+5.0f, 1.5f, -10.0f + i * 5.0f);
		XMMATRIX leftSphereWorld = XMMatrixTranslation(-5.0f, 3.5f, -10.0f + i * 5.0f);
		XMMATRIX rightSphereWorld = XMMatrixTranslation(+5.0f, 3.5f, -10.0f + i * 5.0f);
		//左边5个圆柱
		XMStoreFloat4x4(&(leftCylinderRitem->world), leftCylWorld);
		//此处的索引随着循环不断加1（注意：这里是先赋值再++）
		leftCylinderRitem->objCBIndex = fllowObjCBIndex++;
		leftCylinderRitem->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftCylinderRitem->IndexCount = mGeos->DrawArgs["cylinder"].IndexCount;
		leftCylinderRitem->BaseVertexLocation = mGeos->DrawArgs["cylinder"].BaseVertexLocation;
		leftCylinderRitem->StartIndexLocation = mGeos->DrawArgs["cylinder"].StartIndexLocation;
		//右边5个圆柱
		XMStoreFloat4x4(&(rightCylinderRitem->world), rightCylWorld);
		rightCylinderRitem->objCBIndex = fllowObjCBIndex++;
		rightCylinderRitem->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightCylinderRitem->IndexCount = mGeos->DrawArgs["cylinder"].IndexCount;
		rightCylinderRitem->BaseVertexLocation = mGeos->DrawArgs["cylinder"].BaseVertexLocation;
		rightCylinderRitem->StartIndexLocation = mGeos->DrawArgs["cylinder"].StartIndexLocation;
		//左边5个球
		XMStoreFloat4x4(&(leftSphereRitem->world), leftSphereWorld);
		leftSphereRitem->objCBIndex = fllowObjCBIndex++;
		leftSphereRitem->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftSphereRitem->IndexCount = mGeos->DrawArgs["sphere"].IndexCount;
		leftSphereRitem->BaseVertexLocation = mGeos->DrawArgs["sphere"].BaseVertexLocation;
		leftSphereRitem->StartIndexLocation = mGeos->DrawArgs["sphere"].StartIndexLocation;
		//右边5个球
		XMStoreFloat4x4(&(rightSphereRitem->world), rightSphereWorld);
		rightSphereRitem->objCBIndex = fllowObjCBIndex++;
		rightSphereRitem->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightSphereRitem->IndexCount = mGeos->DrawArgs["sphere"].IndexCount;
		rightSphereRitem->BaseVertexLocation = mGeos->DrawArgs["sphere"].BaseVertexLocation;
		rightSphereRitem->StartIndexLocation = mGeos->DrawArgs["sphere"].StartIndexLocation;

		mAllRitems.push_back(std::move(leftCylinderRitem));
		mAllRitems.push_back(std::move(rightCylinderRitem));
		mAllRitems.push_back(std::move(leftSphereRitem));
		mAllRitems.push_back(std::move(rightSphereRitem));
	}
	for (auto& e : mAllRitems)
		mOpaqueRitems.push_back(e.get());
}

void ShapesApp::DrawRenderItems()
{
	//遍历渲染项数组
	for (size_t i = 0; i < mOpaqueRitems.size(); i++)
	{
		auto ritem = mOpaqueRitems[i];

		mCommandList->IASetVertexBuffers(0, 1, &mGeos->VertexBufferView());
		mCommandList->IASetIndexBuffer(&mGeos->IndexBufferView());
		mCommandList->IASetPrimitiveTopology(ritem->primitiveType);

		//设置根描述符表1
		UINT objCbvIndex = mCurrFrameResourceIndex * (UINT)mOpaqueRitems.size() + ritem->objCBIndex;;
		auto handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		handle.Offset(objCbvIndex, mCbvSrvUavDescriptorSize);
		mCommandList->SetGraphicsRootDescriptorTable(0, //根参数的起始索引
			handle);

		//绘制顶点（通过索引缓冲区绘制）
		mCommandList->DrawIndexedInstanced(ritem->IndexCount, //每个实例要绘制的索引数
			1,	//实例化个数
			ritem->StartIndexLocation,	//起始索引位置
			ritem->BaseVertexLocation,	//子物体起始索引在全局索引中的位置
			0);	//实例化的高级技术，暂时设置为0
	}
}

void ShapesApp::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
			1, (UINT)mAllRitems.size()));
	}
}