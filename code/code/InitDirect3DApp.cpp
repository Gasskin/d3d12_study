#include "d3dApp.h"

using namespace DirectX;

//����ʵ�ʵĻ�������Ҫ�̳������ǵĿ����
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
	//���������б������������
	ThrowIfFailed(mDirectCmdListAlloc->Reset());

	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	//����������Դ�ӳ�ʼ״̬תΪ��̨��������Դ
	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(mSwapChainBuffer[mCurrBackBuffer].Get(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	//�����ӿں;���
	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	//�����̨����������Ȼ�����������ֵ
	mCommandList->ClearRenderTargetView(
		CurrentBackBufferView(),
		DirectX::Colors::LightBlue, //���RT����ɫΪ����ɫ�����Ҳ����òü�����
		0,
		nullptr);

	mCommandList->ClearDepthStencilView(
		DepthStencilView(),                                //DSV���������
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, //FLAG
		1.0f,                                              //Ĭ�����ֵ
		0,                                                 //Ĭ��ģ��ֵ
		0,                                                 //�ü���������
		nullptr);                                          //�ü�����ָ��

	//ָ����Ҫ��Ⱦ�Ļ���������ָ��RTV��DSV
	mCommandList->OMSetRenderTargets(
		1,                        //���󶨵�RTV����
		&CurrentBackBufferView(), //ָ��RTV�����ָ��
		true,                     //RTV�����ڶ��ڴ�����������ŵ�
		&DepthStencilView());       //ָ��DSV��ָ��

	//����̨��������״̬�ĳɳ���״̬
	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(mSwapChainBuffer[mCurrBackBuffer].Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT)); //����ȾĿ�굽����

	//�������ļ�¼�ر������б�
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* commandLists[] = { mCommandList.Get() };                 //���������������б�����
	mCommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists); //������������б����������

	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	FlushCommandQueue();
}
