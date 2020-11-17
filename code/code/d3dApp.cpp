#include "d3dApp.h"
#include <WindowsX.h>

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

		}
	}

	return (int)msg.wParam;
}

//真正的消息处理函数
LRESULT D3DApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProc(hwnd, msg, wParam, lParam);
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
