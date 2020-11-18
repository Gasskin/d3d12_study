#include "d3dApp.h"

using namespace DirectX;

//我们实际的绘制类需要继承自我们的框架类
class InitDirect3DApp : public D3DApp
{
public:
	InitDirect3DApp(HINSTANCE hInstance);
	~InitDirect3DApp();
	virtual bool Initialize()override;
private:
	virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)override;
	virtual void Update()override;
	virtual void Draw()override;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		InitDirect3DApp theApp(hInstance);
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

InitDirect3DApp::InitDirect3DApp(HINSTANCE hInstance)
	:D3DApp(hInstance)
{
}

InitDirect3DApp::~InitDirect3DApp()
{
}

bool InitDirect3DApp::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	return true;
}

LRESULT InitDirect3DApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void InitDirect3DApp::Update()
{
}

void InitDirect3DApp::Draw()
{
	//重置命令列表与命令分配器
	ThrowIfFailed(mDirectCmdListAlloc->Reset());

	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	//将缓冲区资源从初始状态转为后台缓冲区资源
	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(mSwapChainBuffer[mCurrBackBuffer].Get(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	//设置视口和矩形
	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	//清除后台缓冲区和深度缓冲区，并赋值
	mCommandList->ClearRenderTargetView(
		CurrentBackBufferView(),
		DirectX::Colors::LightBlue, //清除RT背景色为淡蓝色，并且不设置裁剪矩形
		0,
		nullptr);

	mCommandList->ClearDepthStencilView(
		DepthStencilView(),                                //DSV描述符句柄
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, //FLAG
		1.0f,                                              //默认深度值
		0,                                                 //默认模板值
		0,                                                 //裁剪矩形数量
		nullptr);                                          //裁剪矩形指针

	//指定将要渲染的缓冲区，即指定RTV和DSV
	mCommandList->OMSetRenderTargets(
		1,                        //待绑定的RTV数量
		&CurrentBackBufferView(), //指向RTV数组的指针
		true,                     //RTV对象在堆内存中是连续存放的
		&DepthStencilView());       //指向DSV的指针

	//将后台缓冲区的状态改成呈现状态
	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(mSwapChainBuffer[mCurrBackBuffer].Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT)); //从渲染目标到呈现

	//完成命令的记录关闭命令列表
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* commandLists[] = { mCommandList.Get() };                 //声明并定义命令列表数组
	mCommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists); //将命令从命令列表传至命令队列

	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	FlushCommandQueue();
}
