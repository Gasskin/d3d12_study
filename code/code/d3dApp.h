#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "d3dUtil.h"

#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

/*
	1.初始化入口为Initialize()->InitMainWindow()

	2.在InitMainWindow()中，我们注册了一个窗口实例，并创建它

	3.创建窗口时绑定MainWndProc()作为辅助回调函数，在其中又调用虚函数MsgProc()进行真正的消息处理
	  子类可以重写这个虚函数实现自己的消息处理机制

	4.但我们不能创建完窗口后又立刻关闭它！所以用Run()维持一个while循环，在出现退出消息之前，不断的循环，防止窗口关闭
*/

//主框架类
class D3DApp
{
protected:
	D3DApp(HINSTANCE hInstance);
	D3DApp(const D3DApp& rhs) = delete;
	D3DApp& operator=(const D3DApp& rhs) = delete;
	virtual ~D3DApp();
public:
	//初始化入口
	virtual bool Initialize();

	//获取静态D3DApp对象
	static D3DApp* GetApp();

	//获取APP的HINSTANCE和窗口HWND
	HINSTANCE AppInst()const;
	HWND      MainWnd()const;

	//维持消息循环
	int Run();
	
	//真正的窗口回调函数，我们将他设为virtual，这样继承于D3DApp的子类就能够定义自己的事件处理函数
	virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
protected:
	//初始化窗口
	bool InitMainWindow();	
protected:
	//维持一个静态的D3DApp类
	static D3DApp* mApp;
	
	//HINSTANCE是APP句柄，HWND是窗口句柄，不同点在于，程序只能维持一个APP，而一个APP可以拥有多个窗口
	//所以在程序运行之初，就会有一个且只有这一个HINSTANCE，而每创建一个窗口，都会有一个HWND
	HINSTANCE mhAppInst = nullptr; 
	HWND      mhMainWnd = nullptr;
	//窗口的名字
	std::wstring mMainWndCaption = L"d3d App";
	//窗口的宽高
	int mClientWidth = 800;
	int mClientHeight = 600;
};