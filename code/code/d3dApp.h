#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "d3dUtil.h"

#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")


//主框架类
class D3DApp
{
protected:
	D3DApp(HINSTANCE hInstance);
	D3DApp(const D3DApp& rhs) = delete;
	D3DApp& operator=(const D3DApp& rhs) = delete;
	virtual ~D3DApp();
public:
	virtual bool	Initialize();//初始化入口

	static D3DApp*	GetApp();//获取静态D3DApp对象
	
	HINSTANCE		AppInst()const;//获取APP的HINSTANCE和窗口HWND
	HWND			MainWnd()const;

	int				Run();//维持消息循环
	
	//真正的窗口回调函数，我们将他设为virtual，这样继承于D3DApp的子类就能够定义自己的事件处理函数
	virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
protected:
	bool						InitMainWindow();//初始化窗口	
	bool						InitDirect3D();//初始化d3d

	void						CreateCommandObjects();//创建命令对象
	void						CreateSwapChain();//创建交换链
	virtual void				CreateRtvAndDsvDescriptorHeaps();//创建RTV和DSV描述符堆

	void						FlushCommandQueue();//围栏算法

	ID3D12Resource*				CurrentBackBuffer()const;//获取当前后台缓冲区
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView()const;//当前后台缓冲区视图
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView()const;//深度模板缓冲区视图
protected:
	virtual void OnResize();//重新绘制窗口，比如改变窗口大小后要重新绘制
	virtual void Update() = 0;//更新部分数据，如MVP矩阵
	virtual void Draw() = 0;//绘制命令
protected:
	//维持一个静态的D3DApp类
	static				D3DApp* mApp;
	bool				m4xMsaaState = false; //是否开启msaa
	UINT				m4xMsaaQuality = 0;	//msaa的质量级别
	static const int	SwapChainBufferCount = 2;//交换链中后台缓冲区的数量，一般就是2
	bool				mAppPaused = false;//程序是否暂停

	//HINSTANCE是APP句柄，HWND是窗口句柄，不同点在于，程序只能维持一个APP，而一个APP可以拥有多个窗口
	//所以在程序运行之初，就会有一个且只有这一个HINSTANCE，而每创建一个窗口，都会有一个HWND
	HINSTANCE		mhAppInst = nullptr; 
	HWND			mhMainWnd = nullptr;
	std::wstring	mMainWndCaption = L"d3d App";//窗口的名字
	int				mClientWidth = 800;//窗口的宽高
	int				mClientHeight = 600;

	Microsoft::WRL::ComPtr<IDXGIFactory4>				mdxgiFactory;//工厂、设备、交换链
	Microsoft::WRL::ComPtr<ID3D12Device>				md3dDevice;
	Microsoft::WRL::ComPtr<IDXGISwapChain>				mSwapChain;
	Microsoft::WRL::ComPtr<ID3D12Fence>					mFence;//围栏
	UINT64												mCurrentFence = 0;//用于记录当前围栏位置
	Microsoft::WRL::ComPtr<ID3D12CommandQueue>			mCommandQueue;//命令对象
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator>		mDirectCmdListAlloc;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	mCommandList;

	//用于储存RTV DSV CBV的描述符大小
	//为什么需要呢，因为我们会在描述符堆中用offset()函数对当前描述符位置进行偏移，那当然要知道需要偏移的大小
	UINT mRtvDescriptorSize = 0;
	UINT mDsvDescriptorSize = 0;
	UINT mCbvSrvUavDescriptorSize = 0;

	int												mCurrBackBuffer = 0;//当前后台缓冲区索引，因为有2个嘛，所以需要一个索引来确定是哪个
	Microsoft::WRL::ComPtr<ID3D12Resource>			mSwapChainBuffer[SwapChainBufferCount];//当前后台缓冲区资源
	Microsoft::WRL::ComPtr<ID3D12Resource>			mDepthStencilBuffer;//当前深度模板缓冲区资源
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>	mRtvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>	mDsvHeap;

	D3D12_VIEWPORT	mScreenViewport;//视口
	D3D12_RECT		mScissorRect;//裁剪矩形

	D3D_DRIVER_TYPE md3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
	DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
};