#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "d3dUtil.h"

#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")


//�������
class D3DApp
{
protected:
	D3DApp(HINSTANCE hInstance);
	D3DApp(const D3DApp& rhs) = delete;
	D3DApp& operator=(const D3DApp& rhs) = delete;
	virtual ~D3DApp();
public:
	virtual bool	Initialize();//��ʼ�����

	static D3DApp*	GetApp();//��ȡ��̬D3DApp����
	
	HINSTANCE		AppInst()const;//��ȡAPP��HINSTANCE�ʹ���HWND
	HWND			MainWnd()const;

	int				Run();//ά����Ϣѭ��
	
	//�����Ĵ��ڻص����������ǽ�����Ϊvirtual�������̳���D3DApp��������ܹ������Լ����¼�������
	virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
protected:
	bool						InitMainWindow();//��ʼ������	
	bool						InitDirect3D();//��ʼ��d3d

	void						CreateCommandObjects();//�����������
	void						CreateSwapChain();//����������
	virtual void				CreateRtvAndDsvDescriptorHeaps();//����RTV��DSV��������

	void						FlushCommandQueue();//Χ���㷨

	ID3D12Resource*				CurrentBackBuffer()const;//��ȡ��ǰ��̨������
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView()const;//��ǰ��̨��������ͼ
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView()const;//���ģ�建������ͼ
protected:
	virtual void OnResize();//���»��ƴ��ڣ�����ı䴰�ڴ�С��Ҫ���»���
	virtual void Update() = 0;//���²������ݣ���MVP����
	virtual void Draw() = 0;//��������
protected:
	//ά��һ����̬��D3DApp��
	static				D3DApp* mApp;
	bool				m4xMsaaState = false; //�Ƿ���msaa
	UINT				m4xMsaaQuality = 0;	//msaa����������
	static const int	SwapChainBufferCount = 2;//�������к�̨��������������һ�����2
	bool				mAppPaused = false;//�����Ƿ���ͣ

	//HINSTANCE��APP�����HWND�Ǵ��ھ������ͬ�����ڣ�����ֻ��ά��һ��APP����һ��APP����ӵ�ж������
	//�����ڳ�������֮�����ͻ���һ����ֻ����һ��HINSTANCE����ÿ����һ�����ڣ�������һ��HWND
	HINSTANCE		mhAppInst = nullptr; 
	HWND			mhMainWnd = nullptr;
	std::wstring	mMainWndCaption = L"d3d App";//���ڵ�����
	int				mClientWidth = 800;//���ڵĿ��
	int				mClientHeight = 600;

	Microsoft::WRL::ComPtr<IDXGIFactory4>				mdxgiFactory;//�������豸��������
	Microsoft::WRL::ComPtr<ID3D12Device>				md3dDevice;
	Microsoft::WRL::ComPtr<IDXGISwapChain>				mSwapChain;
	Microsoft::WRL::ComPtr<ID3D12Fence>					mFence;//Χ��
	UINT64												mCurrentFence = 0;//���ڼ�¼��ǰΧ��λ��
	Microsoft::WRL::ComPtr<ID3D12CommandQueue>			mCommandQueue;//�������
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator>		mDirectCmdListAlloc;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	mCommandList;

	//���ڴ���RTV DSV CBV����������С
	//Ϊʲô��Ҫ�أ���Ϊ���ǻ���������������offset()�����Ե�ǰ������λ�ý���ƫ�ƣ��ǵ�ȻҪ֪����Ҫƫ�ƵĴ�С
	UINT mRtvDescriptorSize = 0;
	UINT mDsvDescriptorSize = 0;
	UINT mCbvSrvUavDescriptorSize = 0;

	int												mCurrBackBuffer = 0;//��ǰ��̨��������������Ϊ��2���������Ҫһ��������ȷ�����ĸ�
	Microsoft::WRL::ComPtr<ID3D12Resource>			mSwapChainBuffer[SwapChainBufferCount];//��ǰ��̨��������Դ
	Microsoft::WRL::ComPtr<ID3D12Resource>			mDepthStencilBuffer;//��ǰ���ģ�建������Դ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>	mRtvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>	mDsvHeap;

	D3D12_VIEWPORT	mScreenViewport;//�ӿ�
	D3D12_RECT		mScissorRect;//�ü�����

	D3D_DRIVER_TYPE md3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
	DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
};