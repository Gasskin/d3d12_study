#include "d3dApp.h"
#include <WindowsX.h>

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;

//ע�⣬��̬��Ա���������ⶨ�壬�������ڴ���Ԥ�����ÿռ䣬��Ϊ�ڱ���׶ξ�̬�����ͻ��ʼ��
D3DApp* D3DApp::mApp = nullptr;

D3DApp::D3DApp(HINSTANCE hInstance)
	: mhAppInst(hInstance)
{
	//���Ի�����������Ϊ��ʱ����ֹ����
	//��������£����ǵ�һ�γ�ʼ����ʱ�򣬾�̬����Ӧ���Ѿ��ڱ���ʱ�ͷ�����ڴ��ˣ�������nullptr
	//�����nullptr���Ǿ�˵�����������ˣ���Ҫ��ֹ
	assert(mApp == nullptr);
	mApp = this;
}

D3DApp::~D3DApp()
{
}

//��ʼ�����
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

//����Ϣѭ��������ά�ֳ�������
int D3DApp::Run()
{
	MSG msg = { 0 };
	mTimer.Reset();
	//�����յ�����Ϣ�����˳��ǣ�ά��ѭ��
	while (msg.message != WM_QUIT)
	{
		//�������Ϣ������Ϣ
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		//û��Ϣ������Ϸ
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

//��������Ϣ������
LRESULT D3DApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

float D3DApp::AspectRatio()const
{
	return static_cast<float>(mClientWidth) / mClientHeight;
}

//�����ص�������������ֻ��������ʼ�����ڣ����ڴ˺����У��������յ��õĻ���D3DApp��MsgPro()����ص�����
//�����Ļ�����Ϳ���ʵ���麯�����������Լ����¼�����������Ҫ�޸Ŀ�ܴ���
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

	//�ü�����
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
	//����D3D12���Բ�
#if defined(DEBUG) || defined(_DEBUG) 
	{
		ComPtr<ID3D12Debug> debugController;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
		debugController->EnableDebugLayer();
	}
#endif
	//1.��������
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mdxgiFactory)));

	//2.���Դ���Ӳ���豸
	//��Ϊ��һ����GPU���������ǲ���ThrowIfFailedֱ���׳����󣬶�����HRESULT�ռ��������Ļ����Խ��ж����ж�
	HRESULT hardwareResult =
		D3D12CreateDevice(
			nullptr,                    //ָ���������õ�GPU�������nullptr������Ĭ��GPU
			D3D_FEATURE_LEVEL_11_0,     //Ӧ�ó�������ġ���͡����ܼ��������֧�֣��Ǿʹ���ʧ��
			IID_PPV_ARGS(&md3dDevice)
		);
	//�������ʧ��,���˻ش���WARP�豸
	if (FAILED(hardwareResult))
	{
		ComPtr<IDXGIAdapter>pWarpAdapter;
		//ö��WARP�豸������win�Դ��ģ�7���µĹ��ܼ�����ൽ10��ע�⣬��mdxgiFactory����ΪIDXGIFactory4������û��ö�ٹ���
		ThrowIfFailed(mdxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter)));
		//����WARP�豸
		ThrowIfFailed(
			D3D12CreateDevice(
				pWarpAdapter.Get(),
				D3D_FEATURE_LEVEL_11_0,
				IID_PPV_ARGS(&md3dDevice))
		);
	}

	//3.����Χ�����������������С
	ThrowIfFailed(
		md3dDevice->CreateFence(
			0,
			D3D12_FENCE_FLAG_NONE,
			IID_PPV_ARGS(&mFence))
	);
	mRtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mDsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	//4.������4X MSAA���������֧��
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
	msQualityLevels.Format = mDepthStencilFormat;
	msQualityLevels.SampleCount = 4;
	msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	msQualityLevels.NumQualityLevels = 0;
	//�����֮�󣬺������������дmsQualityLevels�ṹ�壬�������룬�������
	ThrowIfFailed(md3dDevice->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&msQualityLevels,
		sizeof(msQualityLevels)));
	//ΪʲôҪ����������� ��Ϊ���������������г�����˵��һ���ģ�1�ξ���1�Σ�4�ξ���4��
	//��������������ڲ�ͬ������˵����������ر���������Ҫ��⣬�������Լ���д
	//��������������Ϊ��⵽����������
	m4xMsaaQuality = msQualityLevels.NumQualityLevels;
	//������ȷ�Ļ���m4XMsaaQuality>0�����Ի���������Ϊ��ʱ��ֹ����
	assert(m4xMsaaQuality > 0 && "Unexpected MSAA quality level.");

	//5.�����������
	CreateCommandObjects();

	//6.����������
	CreateSwapChain();

	//7.������ȾĿ����ͼ����Ȼ�����ͼ
	CreateRtvAndDsvDescriptorHeaps();

	return true;
}

void D3DApp::CreateCommandObjects()
{
	//�������ΪGPU�������������������������б������������У��÷��������ύ��GPU�Ļ�������
	//��������
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	//��������
	ThrowIfFailed(
		md3dDevice->CreateCommandQueue(
			&queueDesc,
			IID_PPV_ARGS(&mCommandQueue))
	);
	//�������������
	ThrowIfFailed(
		md3dDevice->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(mDirectCmdListAlloc.GetAddressOf()))
	);
	//���������б�
	ThrowIfFailed(
		md3dDevice->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			mDirectCmdListAlloc.Get(), //�б�����һ��������
			nullptr,
			IID_PPV_ARGS(mCommandList.GetAddressOf()))
	);
	//�����б�����֮����봦�ڹر�״̬
	mCommandList->Close();
}

void D3DApp::CreateSwapChain()
{
	mSwapChain.Reset();

	DXGI_SWAP_CHAIN_DESC sd;
	//�������൱����Ļ
	sd.BufferDesc.Width = mClientWidth;//�������߶�
	sd.BufferDesc.Height = mClientHeight;//���������
	sd.BufferDesc.RefreshRate.Numerator = 60;//ˢ���ʷ���
	sd.BufferDesc.RefreshRate.Denominator = 1;//ˢ���ʷ�ĸ
	sd.BufferDesc.Format = mBackBufferFormat;//��ʾ��ʽ
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;//����ɨ��VS����ɨ��(δָ����)
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;//ͼ�������Ļ�����죨δָ���ģ�

	sd.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	sd.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;

	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // ��������Ⱦ����̨������������Ϊ��ȾĿ�꣩
	sd.BufferCount = SwapChainBufferCount; //�������л�������������������2������ȻҲ�������ػ���
	sd.OutputWindow = mhMainWnd; //��Ⱦ���ھ��
	sd.Windowed = true; //�Ƿ񴰿ڻ�
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;//�̶�д��
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;//����Ӧ����ģʽ���Զ�ѡ�������ڵ�ǰ���ڳߴ����ʾģʽ��
	//�������������������еľ��Ǻ�̨��������Դ����
	ThrowIfFailed(
		mdxgiFactory->CreateSwapChain(
			mCommandQueue.Get(),
			&sd,
			mSwapChain.GetAddressOf())
	);
}

void D3DApp::CreateRtvAndDsvDescriptorHeaps()
{
	//�����������ѣ�RTV��ͼ��DSV��ͼ��Ҫ���������������ϣ�����Щ��ͼ����Ҫ
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
	mCurrentFence++;	//CPU��������رպ󣬽���ǰΧ��ֵ+1
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);	//��GPU������CPU���������󣬽�fence�ӿ��е�Χ��ֵ+1����fence->GetCompletedValue()+1
	if (mFence->GetCompletedValue() < mCurrentFence)	//���С�ڣ�˵��GPUû�д�������������
	{
		HANDLE eventHandle = CreateEvent(nullptr, false, false, L"FenceSetDone");	//�����¼�
		mFence->SetEventOnCompletion(mCurrentFence, eventHandle);//��Χ���ﵽmCurrentFenceֵ����ִ�е�Signal����ָ���޸���Χ��ֵ��ʱ������eventHandle�¼�
		WaitForSingleObject(eventHandle, INFINITE);//�ȴ�GPU����Χ���������¼���������ǰ�߳�ֱ���¼�������ע���Enent���������ٵȴ���
													//���û��Set��Wait���������ˣ�Set��Զ������ã�����Ҳ��û�߳̿��Ի�������̣߳�
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

	//�������������
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));
	//������Դ
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

	//��Դ��Ҫ�󶨵���ˮ�ߣ������Ҫ����ͼ��������ͼ�ֱ��봴���ڶ��ϣ������Ѿ������˶ѣ�����һ���Ǵ�����ͼ
	//��ȡ���е�һ���������ľ�������ô˾�������ǿ��Խ���ͼ��������Ӧλ��
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (int i = 0; i < 2; i++)
	{
		//��ô��ڽ������еĺ�̨��������Դ
		mSwapChain->GetBuffer(i, IID_PPV_ARGS(mSwapChainBuffer[i].GetAddressOf()));
		//����RTV
		md3dDevice->CreateRenderTargetView(
			mSwapChainBuffer[i].Get(),
			nullptr,	    //�ڽ������������Ѿ������˸���Դ�����ݸ�ʽ����������ָ��Ϊ��ָ��
			rtvHeapHandle);	//����������ṹ�壨�����Ǳ��壬�̳���CD3DX12_CPU_DESCRIPTOR_HANDLE��

		//��rtvHeapHandleƫ�Ƶ����������е���һ������������λ��
		rtvHeapHandle.Offset(1, mRtvDescriptorSize);
	}

	//��ʱ�������Ѿ�������DSV�������ѣ�����û�д���DSV����������DSV��ͼ
	//ע�⣬����Ҳû����ʾ�Ĵ���RTV����������������ʵ���ǽ��������������������2��RTV������

	//����DSV������
	//����DSV������
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
	//������һ�����������Դ���Ż�ֵ����ʱ��̫���������
	D3D12_CLEAR_VALUE optClear;
	optClear.Format = mDepthStencilFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;
	//����������
	ThrowIfFailed(
		md3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&depthStencilDesc,
			D3D12_RESOURCE_STATE_COMMON,
			&optClear,
			IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf()))
	);
	//DSV��������ͼ
	md3dDevice->CreateDepthStencilView(
		mDepthStencilBuffer.Get(),//D3D12_DEPTH_STENCIL_VIEW_DESC����ָ�룬����&dsvDesc������ע�ʹ��룩��
		nullptr,	   //�����ڴ������ģ����Դʱ�Ѿ��������ģ���������ԣ������������ָ��Ϊ��ָ��
		mDsvHeap->GetCPUDescriptorHandleForHeapStart()//DSV���
	);
	//����������Դ�ɳ�ʼ״̬��Ϊ��Ȼ�����״̬
	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE)
	);
	//�ύ����
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	FlushCommandQueue();

	//�����ӿ���ü����Σ��ھ���Draw��ʱ�򣬲Ż��������
	mScreenViewport.TopLeftX = -320;
	mScreenViewport.TopLeftY = 0;
	mScreenViewport.Width = static_cast<float>(mClientWidth);
	mScreenViewport.Height = static_cast<float>(mClientHeight);
	mScreenViewport.MinDepth = 0.0f;
	mScreenViewport.MaxDepth = 1.0f;

	mScissorRect = { 0, 0, mClientWidth, mClientHeight };
}
