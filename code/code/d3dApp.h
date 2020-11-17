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
	1.��ʼ�����ΪInitialize()->InitMainWindow()

	2.��InitMainWindow()�У�����ע����һ������ʵ������������

	3.��������ʱ��MainWndProc()��Ϊ�����ص��������������ֵ����麯��MsgProc()������������Ϣ����
	  ���������д����麯��ʵ���Լ�����Ϣ�������

	4.�����ǲ��ܴ����괰�ں������̹ر�����������Run()ά��һ��whileѭ�����ڳ����˳���Ϣ֮ǰ�����ϵ�ѭ������ֹ���ڹر�
*/

//�������
class D3DApp
{
protected:
	D3DApp(HINSTANCE hInstance);
	D3DApp(const D3DApp& rhs) = delete;
	D3DApp& operator=(const D3DApp& rhs) = delete;
	virtual ~D3DApp();
public:
	//��ʼ�����
	virtual bool Initialize();

	//��ȡ��̬D3DApp����
	static D3DApp* GetApp();

	//��ȡAPP��HINSTANCE�ʹ���HWND
	HINSTANCE AppInst()const;
	HWND      MainWnd()const;

	//ά����Ϣѭ��
	int Run();
	
	//�����Ĵ��ڻص����������ǽ�����Ϊvirtual�������̳���D3DApp��������ܹ������Լ����¼�������
	virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
protected:
	//��ʼ������
	bool InitMainWindow();	
protected:
	//ά��һ����̬��D3DApp��
	static D3DApp* mApp;
	
	//HINSTANCE��APP�����HWND�Ǵ��ھ������ͬ�����ڣ�����ֻ��ά��һ��APP����һ��APP����ӵ�ж������
	//�����ڳ�������֮�����ͻ���һ����ֻ����һ��HINSTANCE����ÿ����һ�����ڣ�������һ��HWND
	HINSTANCE mhAppInst = nullptr; 
	HWND      mhMainWnd = nullptr;
	//���ڵ�����
	std::wstring mMainWndCaption = L"d3d App";
	//���ڵĿ��
	int mClientWidth = 800;
	int mClientHeight = 600;
};