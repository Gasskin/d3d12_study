#include "d3dApp.h"
#include <WindowsX.h>

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

		}
	}

	return (int)msg.wParam;
}

//��������Ϣ������
LRESULT D3DApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProc(hwnd, msg, wParam, lParam);
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
