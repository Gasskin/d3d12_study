#include "d3dApp.h"
#include <WindowsX.h>

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;

//注意，静态成员必须在类外定义，这是在内存中预先留好空间，因为在编译阶段静态变量就会初始化
D3DApp* D3DApp::mApp = nullptr;

D3DApp::D3DApp(HINSTANCE hInstance)
	: mhAppInst(hInstance)
{
	//断言会在其中内容为假时，终止程序
	//正常情况下，我们第一次初始化的时候，静态类型应该已经在编译时就分配好内存了，不会是nullptr
	//如果是nullptr，那就说明发生错误了，需要终止
	assert(mApp == nullptr);
	mApp = this;
}

D3DApp::~D3DApp()
{
}

//初始化入口
bool D3DApp::Initialize()
{
	if (!InitMainWindow())
		return false;

	if (!InitDirect3D())
		return false;

	OnResize();

	return true;
}

D3DApp* D3DApp::GetApp()
{
	return mApp;
}

HINSTANCE D3DApp::AppInst() const
{
	return mhAppInst;
}

HWND D3DApp::MainWnd() const
{
	return mhMainWnd;
}

//主消息循环，用于维持程序运行
int D3DApp::Run()
{
	MSG msg = { 0 };
	mTimer.Reset();
	//当接收到的消息不是退出是，维持循环
	while (msg.message != WM_QUIT)
	{
		//如果有消息则处理消息
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		//没消息则处理游戏
		else
		{
			mTimer.Tick();
			if (!mAppPaused)
			{
				CalculateFrameStats();
				Update(mTimer);
				Draw(mTimer);
			}
			else
			{
				Sleep(100);
			}
		}
	}

	return (int)msg.wParam;
}

//真正的消息处理函数
LRESULT D3DApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

float D3DApp::AspectRatio()const
{
	return static_cast<float>(mClientWidth) / mClientHeight;
}

//辅助回调函数，其作用只是用来初始化窗口，而在此函数中，我们最终调用的还是D3DApp中MsgPro()这个回调函数
//这样的话子类就可以实现虚函数，来定义自己的事件处理，而不需要修改框架代码
LRESULT CALLBACK
MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return D3DApp::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}

bool D3DApp::InitMainWindow()
{
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = mhAppInst;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = L"MainWnd";

	if (!RegisterClass(&wc))
	{
		MessageBox(0, L"RegisterClass Failed.", 0, 0);
		return false;
	}

	//裁剪矩形
	RECT R = { 0, 0, mClientWidth, mClientHeight };
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;

	mhMainWnd = CreateWindow(
		L"MainWnd",
		mMainWndCaption.c_str(),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		width, height, 0, 0, mhAppInst, 0);

	if (!mhMainWnd)
	{
		MessageBox(0, L"CreateWindow Failed.", 0, 0);
		return false;
	}

	ShowWindow(mhMainWnd, SW_SHOW);
	UpdateWindow(mhMainWnd);

	return true;
}

bool D3DApp::InitDirect3D()
{
	//开启D3D12调试层
#if defined(DEBUG) || defined(_DEBUG) 
	{
		ComPtr<ID3D12Debug> debugController;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
		debugController->EnableDebugLayer();
	}
#endif
	//1.创建工厂
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mdxgiFactory)));

	//2.尝试创建硬件设备
	//因为不一定有GPU，所以我们不用ThrowIfFailed直接抛出错误，而是用HRESULT收集，这样的话可以进行二次判断
	HRESULT hardwareResult =
		D3D12CreateDevice(
			nullptr,                    //指向我们想用的GPU，如果是nullptr，则是默认GPU
			D3D_FEATURE_LEVEL_11_0,     //应用程序所需的“最低”功能级别，如果不支持，那就创建失败
			IID_PPV_ARGS(&md3dDevice)
		);
	//如果创建失败,就退回创建WARP设备
	if (FAILED(hardwareResult))
	{
		ComPtr<IDXGIAdapter>pWarpAdapter;
		//枚举WARP设备，这是win自带的，7以下的功能级别最多到10，注意，此mdxgiFactory必须为IDXGIFactory4，否则没有枚举功能
		ThrowIfFailed(mdxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter)));
		//创建WARP设备
		ThrowIfFailed(
			D3D12CreateDevice(
				pWarpAdapter.Get(),
				D3D_FEATURE_LEVEL_11_0,
				IID_PPV_ARGS(&md3dDevice))
		);
	}

	//3.创建围栏，并获得描述符大小
	ThrowIfFailed(
		md3dDevice->CreateFence(
			0,
			D3D12_FENCE_FLAG_NONE,
			IID_PPV_ARGS(&mFence))
	);
	mRtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mDsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	//4.检测对于4X MSAA质量级别的支持
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
	msQualityLevels.Format = mDepthStencilFormat;
	msQualityLevels.SampleCount = 4;
	msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	msQualityLevels.NumQualityLevels = 0;
	//检测完之后，函数会帮我们填写msQualityLevels结构体，即是输入，又是输出
	ThrowIfFailed(md3dDevice->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&msQualityLevels,
		sizeof(msQualityLevels)));
	//为什么要检测质量级别， 因为采样次数对于所有厂商来说是一样的，1次就是1次，4次就是4次
	//但是质量级别对于不同厂商来说，其概念天差地别，所以我们要检测，而不能自己填写
	//将质量级别设置为检测到的质量级别
	m4xMsaaQuality = msQualityLevels.NumQualityLevels;
	//运行正确的话，m4XMsaaQuality>0，断言会在其内容为假时终止程序
	assert(m4xMsaaQuality > 0 && "Unexpected MSAA quality level.");

	//5.创建命令相关
	CreateCommandObjects();

	//6.创建交换链
	CreateSwapChain();

	//7.创建渲染目标视图与深度缓冲视图
	CreateRtvAndDsvDescriptorHeaps();

	return true;
}

void D3DApp::CreateCommandObjects()
{
	//命令队列为GPU的命令缓冲区，而命令储存在命令列表的命令分配器中，用方法可以提交到GPU的缓冲区内
	//描述队列
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	//创建队列
	ThrowIfFailed(
		md3dDevice->CreateCommandQueue(
			&queueDesc,
			IID_PPV_ARGS(&mCommandQueue))
	);
	//创建命令分配器
	ThrowIfFailed(
		md3dDevice->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(mDirectCmdListAlloc.GetAddressOf()))
	);
	//创建命令列表
	ThrowIfFailed(
		md3dDevice->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			mDirectCmdListAlloc.Get(), //列表必须绑定一个分配器
			nullptr,
			IID_PPV_ARGS(mCommandList.GetAddressOf()))
	);
	//命令列表创建完之后必须处于关闭状态
	mCommandList->Close();
}

void D3DApp::CreateSwapChain()
{
	mSwapChain.Reset();

	DXGI_SWAP_CHAIN_DESC sd;
	//缓冲区相当于屏幕
	sd.BufferDesc.Width = mClientWidth;//缓冲区高度
	sd.BufferDesc.Height = mClientHeight;//缓冲区宽度
	sd.BufferDesc.RefreshRate.Numerator = 60;//刷新率分子
	sd.BufferDesc.RefreshRate.Denominator = 1;//刷新率分母
	sd.BufferDesc.Format = mBackBufferFormat;//显示格式
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;//逐行扫描VS隔行扫描(未指定的)
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;//图像相对屏幕的拉伸（未指定的）

	sd.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	sd.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;

	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 将数据渲染至后台缓冲区（即作为渲染目标）
	sd.BufferCount = SwapChainBufferCount; //交换链中缓冲区的数量，现在是2个，当然也可以三重缓冲
	sd.OutputWindow = mhMainWnd; //渲染窗口句柄
	sd.Windowed = true; //是否窗口化
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;//固定写法
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;//自适应窗口模式（自动选择最适于当前窗口尺寸的显示模式）
	//创建交换链，交换链中的就是后台缓冲区资源！！
	ThrowIfFailed(
		mdxgiFactory->CreateSwapChain(
			mCommandQueue.Get(),
			&sd,
			mSwapChain.GetAddressOf())
	);
}

void D3DApp::CreateRtvAndDsvDescriptorHeaps()
{
	//创建描述符堆，RTV视图和DSV视图需要创建在描述符堆上，但有些视图不需要
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;

	ThrowIfFailed(
		md3dDevice->CreateDescriptorHeap(
			&rtvHeapDesc,
			IID_PPV_ARGS(mRtvHeap.GetAddressOf()))
	);


	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;

	ThrowIfFailed(
		md3dDevice->CreateDescriptorHeap(
			&dsvHeapDesc,
			IID_PPV_ARGS(mDsvHeap.GetAddressOf()))
	);
}

void D3DApp::FlushCommandQueue()
{
	mCurrentFence++;	//CPU传完命令并关闭后，将当前围栏值+1
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);	//当GPU处理完CPU传入的命令后，将fence接口中的围栏值+1，即fence->GetCompletedValue()+1
	if (mFence->GetCompletedValue() < mCurrentFence)	//如果小于，说明GPU没有处理完所有命令
	{
		HANDLE eventHandle = CreateEvent(nullptr, false, false, L"FenceSetDone");	//创建事件
		mFence->SetEventOnCompletion(mCurrentFence, eventHandle);//当围栏达到mCurrentFence值（即执行到Signal（）指令修改了围栏值）时触发的eventHandle事件
		WaitForSingleObject(eventHandle, INFINITE);//等待GPU命中围栏，激发事件（阻塞当前线程直到事件触发，注意此Enent需先设置再等待，
													//如果没有Set就Wait，就死锁了，Set永远不会调用，所以也就没线程可以唤醒这个线程）
		CloseHandle(eventHandle);
	}
}

ID3D12Resource* D3DApp::CurrentBackBuffer() const
{
	return mSwapChainBuffer[mCurrBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::CurrentBackBufferView() const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
		mCurrBackBuffer,
		mRtvDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::DepthStencilView() const
{
	return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}

void D3DApp::CalculateFrameStats()
{
	static int frameCnt = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	if ((mTimer.TotalTime() - timeElapsed) >= 1.0f)
	{
		float fps = (float)frameCnt; // fps = frameCnt / 1
		float mspf = 1000.0f / fps;

		wstring fpsStr = to_wstring(fps);
		wstring mspfStr = to_wstring(mspf);

		wstring windowText = mMainWndCaption +
			L"    fps: " + fpsStr +
			L"   mspf: " + mspfStr;

		SetWindowText(mhMainWnd, windowText.c_str());

		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}

void D3DApp::OnResize()
{
	assert(md3dDevice);
	assert(mSwapChain);
	assert(mDirectCmdListAlloc);

	FlushCommandQueue();

	//重置命令分配器
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));
	//重置资源
	for (int i = 0; i < SwapChainBufferCount; ++i)
	{
		mSwapChainBuffer[i].Reset();
		mDepthStencilBuffer.Reset();
	}
	ThrowIfFailed(
		mSwapChain->ResizeBuffers(
			SwapChainBufferCount,
			mClientWidth, mClientHeight,
			mBackBufferFormat,
			DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH)
	);
	mCurrBackBuffer = 0;

	//资源想要绑定到流水线，则必须要有视图，部分视图又必须创建在堆上，上面已经创建了堆，则这一步是创建视图
	//获取堆中第一个描述符的句柄，利用此句柄，我们可以将视图创建到对应位置
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (int i = 0; i < 2; i++)
	{
		//获得存于交换链中的后台缓冲区资源
		mSwapChain->GetBuffer(i, IID_PPV_ARGS(mSwapChainBuffer[i].GetAddressOf()));
		//创建RTV
		md3dDevice->CreateRenderTargetView(
			mSwapChainBuffer[i].Get(),
			nullptr,	    //在交换链创建中已经定义了该资源的数据格式，所以这里指定为空指针
			rtvHeapHandle);	//描述符句柄结构体（这里是变体，继承自CD3DX12_CPU_DESCRIPTOR_HANDLE）

		//将rtvHeapHandle偏移到描述符堆中的下一个描述符所在位置
		rtvHeapHandle.Offset(1, mRtvDescriptorSize);
	}

	//此时，我们已经创建了DSV描述符堆，但还没有创建DSV缓冲区，和DSV视图
	//注意，我们也没有显示的创建RTV缓冲区，但是这其实就是交换链，交换链本身就是2个RTV缓冲区

	//创建DSV缓冲区
	//描述DSV缓冲区
	D3D12_RESOURCE_DESC depthStencilDesc;
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = mClientWidth;
	depthStencilDesc.Height = mClientHeight;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.Format = mDepthStencilFormat;
	depthStencilDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	depthStencilDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	//描述了一个用于清除资源的优化值，暂时不太能理解作用
	D3D12_CLEAR_VALUE optClear;
	optClear.Format = mDepthStencilFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;
	//创建缓冲区
	ThrowIfFailed(
		md3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&depthStencilDesc,
			D3D12_RESOURCE_STATE_COMMON,
			&optClear,
			IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf()))
	);
	//DSV缓冲区视图
	md3dDevice->CreateDepthStencilView(
		mDepthStencilBuffer.Get(),//D3D12_DEPTH_STENCIL_VIEW_DESC类型指针，可填&dsvDesc（见上注释代码），
		nullptr,	   //由于在创建深度模板资源时已经定义深度模板数据属性，所以这里可以指定为空指针
		mDsvHeap->GetCPUDescriptorHandleForHeapStart()//DSV句柄
	);
	//将缓冲区资源由初始状态变为深度缓冲区状态
	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE)
	);
	//提交命令
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	FlushCommandQueue();

	//设置视口与裁剪矩形，在具体Draw的时候，才会绑定这两个
	mScreenViewport.TopLeftX = -320;
	mScreenViewport.TopLeftY = 0;
	mScreenViewport.Width = static_cast<float>(mClientWidth);
	mScreenViewport.Height = static_cast<float>(mClientHeight);
	mScreenViewport.MinDepth = 0.0f;
	mScreenViewport.MaxDepth = 1.0f;

	mScissorRect = { 0, 0, mClientWidth, mClientHeight };
}
