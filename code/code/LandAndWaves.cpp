#include "d3dApp.h"
#include "UploadBuffer.h"
#include "MathHelper.h"
#include <WindowsX.h>
#include "GeometryGenerator.h"
#include "FrameResource.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

//====================================================
//������Ϣ
//====================================================

//֡��Դ����
const int gNumFrameResources = 3;

//��Ⱦ��
struct RenderItem
{
	RenderItem() = default;

	//�ü�������������
	XMFLOAT4X4 world = MathHelper::Identity4x4();

	//�ü�����ĳ���������objConstantBuffer�е�����
	UINT objCBIndex = -1;

	//֡��Դ����
	int NumFramesDirty = gNumFrameResources;

	//�ü������ͼԪ��������
	D3D12_PRIMITIVE_TOPOLOGY primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	//�ü�����Ļ���������
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	UINT BaseVertexLocation = 0;
};

//====================================================
//����
//====================================================
class LandAndWaves :public D3DApp
{
public:
	LandAndWaves(HINSTANCE hInstance);
	LandAndWaves(const LandAndWaves& rhs) = delete;
	LandAndWaves& operator=(const LandAndWaves& rhs) = delete;
	~LandAndWaves();

	virtual bool Initialize()override;

private:
	virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)override;
	virtual void	Update(const GameTimer& gt)override;
	virtual void	Draw(const GameTimer& gt)override;
	virtual void	OnResize()override;
private:
	void BuildDescriptorHeaps();//������������
	void BuildConstantBuffers();//������������������ͼ
	void BuildRootSignature();//������ǩ��
	void BuildShadersAndInputLayout();//�����������벼��
	void BuildGeometry();//���������壨������������

	void BuildPSO_SOLID();//������ˮ�߶���
	void BuildPSO_WIREFRAME();//������ˮ�߶���

	void BuildRenderItems();//��Ⱦ��
	void DrawRenderItems();

	void BuildFrameResources();//֡��Դ
private:
	void OnMouseDown(WPARAM btnState, int x, int y);
	void OnMouseUp(WPARAM btnState, int x, int y);
	void OnMouseMove(WPARAM btnState, int x, int y);
private:
	ComPtr<ID3D12DescriptorHeap>					mCbvHeap = nullptr;//���������� ��������

	std::unique_ptr<UploadBuffer<ObjectConstants>>	objCB = nullptr;//M������
	std::unique_ptr<UploadBuffer<PassConstants>>	passCB = nullptr;//VP������

	ComPtr<ID3D12RootSignature>						mRootSignature = nullptr;//��ǩ��

	ComPtr<ID3DBlob>								mvsByteCode = nullptr;//������ɫ��
	ComPtr<ID3DBlob>								mpsByteCode = nullptr;//������ɫ��
	std::vector<D3D12_INPUT_ELEMENT_DESC>			mInputLayout;//���벼��

	std::unique_ptr<MeshGeometry>					mGeos = nullptr;//���ӵĶ�������������

	ComPtr<ID3D12PipelineState>						mPSO_SOLID = nullptr;//��ˮ�߶���,ʵ��
	ComPtr<ID3D12PipelineState>						mPSO_WIREFRAME = nullptr;//��ˮ�߶���,�߿�

	std::vector<std::unique_ptr<RenderItem>>		mAllRitems;//��ʱ�������Ϊʲô��Ҫ����vector��������Ⱦ��
	std::vector<RenderItem*>						mOpaqueRitems;

	std::vector<std::unique_ptr<FrameResource>>		mFrameResources;//֡��Դ
	FrameResource* mCurrFrameResource = nullptr;
	int												mCurrFrameResourceIndex = 0;
private:
	XMFLOAT4X4	mWorld = MathHelper::Identity4x4();//�������
	XMFLOAT4X4	mView = MathHelper::Identity4x4();//�۲����
	XMFLOAT4X4	mProj = MathHelper::Identity4x4();//ͶӰ����

	float		mTheta = 1.5f * XM_PI; //1.5pai
	float		mPhi = XM_PIDIV4;//�ķ�֮pai
	float		mRadius = 5.0f;//����
private:
	bool mEnableWireframe = false;
private:
	POINT	mLastMousePos;
};

LandAndWaves::LandAndWaves(HINSTANCE hInstance)
	:D3DApp(hInstance)
{
}

LandAndWaves::~LandAndWaves()
{
}

//���
bool LandAndWaves::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	//���������б������������
	ThrowIfFailed(mDirectCmdListAlloc->Reset());
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));
	//��ʼ��
	BuildGeometry();
	BuildRenderItems();
	BuildFrameResources();
	BuildDescriptorHeaps();
	BuildConstantBuffers();
	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildPSO_SOLID();
	BuildPSO_WIREFRAME();
	//�ر������б�
	ThrowIfFailed(mCommandList->Close());
	//ִ������
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	//Χ��
	FlushCommandQueue();
	return true;
}

void LandAndWaves::Update(const GameTimer& gt)
{
	float x = mRadius * sinf(mPhi) * cosf(mTheta);
	float z = mRadius * sinf(mPhi) * sinf(mTheta);
	float y = mRadius * cosf(mPhi);

	XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);

	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for (auto& e : mAllRitems)
	{
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->world);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));

			currObjectCB->CopyData(e->objCBIndex, objConstants);

			e->NumFramesDirty--;
		}
	}

	PassConstants passConstants;
	XMMATRIX proj = XMLoadFloat4x4(&mProj);
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMStoreFloat4x4(&passConstants.ViewProj, XMMatrixTranspose(viewProj));
	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, passConstants);
}

void LandAndWaves::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	ThrowIfFailed(cmdListAlloc->Reset());

	if (mEnableWireframe)
	{
		ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSO_WIREFRAME.Get()));
	}
	else
	{
		ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSO_SOLID.Get()));
	}

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(mSwapChainBuffer[mCurrBackBuffer].Get(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET)
	);

	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	//����CBV��������
	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	//���ø�ǩ��
	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	int passCbvIndex = mAllRitems.size() * gNumFrameResources + mCurrFrameResourceIndex;
	auto handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
	handle.Offset(passCbvIndex, mCbvSrvUavDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(
		1, //����������ʼ����
		handle);

	DrawRenderItems();

	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(mSwapChainBuffer[mCurrBackBuffer].Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(mCommandList->Close());

	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	//Χ������
	mCurrFrameResource->Fence = ++mCurrentFence;
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void LandAndWaves::OnResize()
{
	D3DApp::OnResize();

	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);
}

void LandAndWaves::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;
	SetCapture(mhMainWnd);
}

void LandAndWaves::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void LandAndWaves::OnMouseMove(WPARAM btnState, int x, int y)
{
	//������������״̬
	if ((btnState & MK_LBUTTON) != 0)
	{
		//�������ƶ����뻻��ɻ��ȣ�0.25Ϊ������ֵ
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));
		//�������û���ɿ�ǰ���ۼƻ���
		mTheta += dx * 2.f;
		mPhi += dy * 2.f;
		//���ƽǶ�phi�ķ�Χ�ڣ�0.1�� Pi-0.1��
		mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
	}
	//������Ҽ�����״̬
	else if ((btnState & MK_RBUTTON) != 0)
	{
		//�������ƶ����뻻������Ŵ�С��0.005Ϊ������ֵ
		float dx = 0.005f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.005f * static_cast<float>(y - mLastMousePos.y);
		//����������������������ӷ�Χ�뾶
		mRadius += (dx - dy) * 4.f;
		//���ƿ��ӷ�Χ�뾶
		mRadius = MathHelper::Clamp(mRadius, 3.0f, 15.0f);
	}
	//����ǰ������긳ֵ������һ��������ꡱ��Ϊ��һ���������ṩ��ǰֵ
	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

LRESULT LandAndWaves::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			OutputDebugString(L"ȡ�������\n");
			mAppPaused = true;
			mTimer.Stop();
		}
		else
		{
			OutputDebugString(L"�����\n");
			mAppPaused = false;
			mTimer.Start();
		}
		return 0;
	case WM_SIZE:
		OutputDebugString(L"�ı䴰�ڴ�С ");
		mClientWidth = LOWORD(lParam);
		mClientHeight = HIWORD(lParam);
		if (md3dDevice)
		{
			if (wParam == SIZE_MINIMIZED)
			{
				mAppPaused = true;
				mMinimized = true;
				mMaximized = false;
			}
			else if (wParam == SIZE_MAXIMIZED)
			{
				mAppPaused = false;
				mMinimized = false;
				mMaximized = true;
				OnResize();
			}
			else if (wParam == SIZE_RESTORED)
			{
				if (mMinimized)
				{
					mAppPaused = false;
					mMinimized = false;
					OnResize();
				}
				else if (mMaximized)
				{
					mAppPaused = false;
					mMaximized = false;
					OnResize();
				}
				else if (mResizing)
				{
					/*
						���ǿ���һֱ�������ڴ�С�������ͬʱҲһֱ���»��ƣ����Ǻ�û��Ҫ�ģ�����ֻ���ڵ�����֮�󣬽���һ�����»���
						���滹������case��һ���ǵ����ǿ�ʼ�������ڴ�Сʱ����һ���ǵ����ǽ����������ڴ�Сʱ
						���Ե����ǿ�ʼ�������ڴ�Сʱ������mResizingΪtrue����������WM_SIZE�����Ϣ�У��ͻ�һֱ������������շ�֧��ɶҲ�����
						�Ȼ��ƽ���ʱ������������mResizingΪfalse������һ��OnResize()
					*/
				}
				else
				{
					OnResize();
				}
			}
		}
	case WM_ENTERSIZEMOVE:
		mAppPaused = true;
		mResizing = true;
		mTimer.Stop();
		return 0;
	case WM_EXITSIZEMOVE:
		mAppPaused = false;
		mResizing = false;
		mTimer.Start();
		OnResize();
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_MENUCHAR:
		return MAKELRESULT(0, MNC_CLOSE);
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		return 0;
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_KEYUP:
		if (wParam == VK_ESCAPE)
		{
			PostQuitMessage(0);
		}
		else if (wParam == VK_F1)
		{
			mEnableWireframe = true;
		}
		else if (wParam == VK_F2)
		{
			mEnableWireframe = false;
		}
		return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		LandAndWaves theApp(hInstance);
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

/*
	��������Ҫ�󶨵���ˮ�ߣ���Ҫ������������ͼ����������ͼ����Ҫ���������������У��糣��������ͼ��DSV,RTV��
	��ȻҲ��һ������ͼ�ǲ���Ҫ�������ѵģ��綥�㻺����ͼ������������ͼ
	�������Ǵ������������ѣ���Ҫ�����ڴ��泣����������ͼ��
*/
void LandAndWaves::BuildDescriptorHeaps()
{
	UINT objectCount = (UINT)mOpaqueRitems.size();
	//���� ��������
	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NumDescriptors = (objectCount + 1) * gNumFrameResources;//ÿһ��������n��W����+1��VP����3��֡��Դ��3����
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;

	//������������
	ThrowIfFailed(
		md3dDevice->CreateDescriptorHeap(
			&cbvHeapDesc,
			IID_PPV_ARGS(&mCbvHeap))
	);
}

void LandAndWaves::BuildConstantBuffers()
{
	//����M������������
	UINT objectCount = (UINT)mOpaqueRitems.size();
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
	{
		auto objectCB = mFrameResources[frameIndex]->ObjectCB->Resource();
		for (UINT i = 0; i < objectCount; ++i)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = objectCB->GetGPUVirtualAddress();

			//��ͼ�ڳ�����������λ�ã��ڼ��������ƫ�Ƽ���������С
			cbAddress += i * objCBByteSize;

			//��ͼ���������ѵ�λ�ã�ע��������6��������������3��obj3��pass������ֻ��1����������
			int heapIndex = frameIndex * objectCount + i;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = objCBByteSize;

			md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
		}
	}


	//����VP������������

	UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

	for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
	{
		auto passCB = mFrameResources[frameIndex]->PassCB->Resource();
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress();

		//�������������3��λ�ô���3��pass
		int heapIndex = objectCount * gNumFrameResources + frameIndex;
		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
		handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = cbAddress;//pass��ͼ�ڻ������е�λ�ò�ƫ�ƣ���Ϊÿ��pass������ֻ��1������
		cbvDesc.SizeInBytes = passCBByteSize;

		md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
	}
}

void LandAndWaves::BuildRootSignature()
{
	CD3DX12_ROOT_PARAMETER slotRootParameter[2];

	//�����ɵ���CBV����ɵ���������
	CD3DX12_DESCRIPTOR_RANGE cbvTable0;
	cbvTable0.Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_CBV, //����������
		1, //����������
		0);//���������󶨵ļĴ����ۺ�
	CD3DX12_DESCRIPTOR_RANGE cbvTable1;
	cbvTable1.Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_CBV, //����������
		1, //����������
		1);//���������󶨵ļĴ����ۺ�
	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);
	slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable1);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, //�����������޸�Ϊ2
		slotRootParameter, 0, nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr =
		D3D12SerializeRootSignature(
			&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
			serializedRootSig.GetAddressOf(),
			errorBlob.GetAddressOf()
		);

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(
		md3dDevice->CreateRootSignature(
			0,
			serializedRootSig->GetBufferPointer(),
			serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(&mRootSignature))
	);
}

void LandAndWaves::BuildShadersAndInputLayout()
{
	HRESULT hr = S_OK;

	mvsByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "VS", "vs_5_0");
	mpsByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "PS", "ps_5_0");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void LandAndWaves::BuildGeometry()
{
	//��һ���Ǵ������㻺������������
	GeometryGenerator proceGeo;
	GeometryGenerator::MeshData box = proceGeo.CreateBox(1.5f, 0.5f, 1.5f, 3);//����������
	GeometryGenerator::MeshData grid = proceGeo.CreateGrid(20.0f, 30.0f, 60, 40);//����ƽ��
	GeometryGenerator::MeshData sphere = proceGeo.CreateSphere(0.5f, 20, 20);//��������
	GeometryGenerator::MeshData cylinder = proceGeo.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);//����������

	//���㵥�������嶥�����ܶ��������е�ƫ����,˳��Ϊ��box��grid��sphere��cylinder
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
	UINT sphereVertexOffset = (UINT)grid.Vertices.size() + gridVertexOffset;
	UINT cylinderVertexOffset = (UINT)sphere.Vertices.size() + sphereVertexOffset;

	//���㵥�������������������������е�ƫ����,˳��Ϊ��box��grid��sphere��cylinder
	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT sphereIndexOffset = (UINT)grid.Indices32.size() + gridIndexOffset;
	UINT cylinderIndexOffset = (UINT)sphere.Indices32.size() + sphereIndexOffset;

	//��SubmeshGeometry�ṹ���¼�����õ�ƫ������
	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.BaseVertexLocation = boxVertexOffset;
	boxSubmesh.StartIndexLocation = boxIndexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.BaseVertexLocation = gridVertexOffset;
	gridSubmesh.StartIndexLocation = gridIndexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;

	//Ȼ�����Ǵ���һ���ܵĶ��㻺��vertices������4��������Ķ������ݴ�������
	size_t totalVertexCount = box.Vertices.size() + grid.Vertices.size() + sphere.Vertices.size() + cylinder.Vertices.size();
	std::vector<Vertex> vertices(totalVertexCount);	//�������������С
	int k = 0;
	for (int i = 0; i < box.Vertices.size(); i++, k++)
	{
		//�˴����׻�����vertices[k]�����Ǵ����Ķ���ṹ�壬����Pos��Color��������
		//��box.Vertices[i]���ø����ṹ��GeometryGenerator�����ģ�������һ������ṹ�壬�����кܶ����ԣ�����������Position����ʼ�����Ƕ���Ķ���
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Yellow);//����box.Vertices[i]��û����ɫ���ԣ����������Զ�����
	}
	for (int i = 0; i < grid.Vertices.size(); i++, k++)
	{
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Brown);
	}
	for (int i = 0; i < sphere.Vertices.size(); i++, k++)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Green);
	}
	for (int i = 0; i < cylinder.Vertices.size(); i++, k++)
	{
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Blue);
	}
	//ͬ�����Ǵ���һ���ܵ���������indices
	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), box.GetIndices16().begin(), box.GetIndices16().end());
	indices.insert(indices.end(), grid.GetIndices16().begin(), grid.GetIndices16().end());
	indices.insert(indices.end(), sphere.GetIndices16().begin(), sphere.GetIndices16().end());
	indices.insert(indices.end(), cylinder.GetIndices16().begin(), cylinder.GetIndices16().end());

	//��������㻺������������Ĵ�С
	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	//����������
	mGeos = std::make_unique<MeshGeometry>();
	mGeos->Name = "shapeGeo";

	mGeos->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, mGeos->VertexBufferUploader);

	mGeos->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, mGeos->IndexBufferUploader);

	mGeos->VertexByteStride = sizeof(Vertex);
	mGeos->VertexBufferByteSize = vbByteSize;
	mGeos->IndexFormat = DXGI_FORMAT_R16_UINT;
	mGeos->IndexBufferByteSize = ibByteSize;

	mGeos->DrawArgs["box"] = boxSubmesh;
	mGeos->DrawArgs["grid"] = gridSubmesh;
	mGeos->DrawArgs["sphere"] = sphereSubmesh;
	mGeos->DrawArgs["cylinder"] = cylinderSubmesh;
}

void LandAndWaves::BuildPSO_SOLID()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	psoDesc.pRootSignature = mRootSignature.Get();
	psoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()),
		mvsByteCode->GetBufferSize()
	};
	psoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()),
		mpsByteCode->GetBufferSize()
	};
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = mBackBufferFormat;
	psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	psoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO_SOLID)));
}

void LandAndWaves::BuildPSO_WIREFRAME()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	psoDesc.pRootSignature = mRootSignature.Get();
	psoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()),
		mvsByteCode->GetBufferSize()
	};
	psoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()),
		mpsByteCode->GetBufferSize()
	};
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = mBackBufferFormat;
	psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	psoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO_WIREFRAME)));
}

void LandAndWaves::BuildRenderItems()
{
	auto boxRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&(boxRitem->world), XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(0.0f, 0.5f, 0.0f));
	boxRitem->objCBIndex = 0;//BOX�������ݣ�world������objConstantBuffer����0��
	boxRitem->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = mGeos->DrawArgs["box"].IndexCount;
	boxRitem->BaseVertexLocation = mGeos->DrawArgs["box"].BaseVertexLocation;
	boxRitem->StartIndexLocation = mGeos->DrawArgs["box"].StartIndexLocation;
	mAllRitems.push_back(std::move(boxRitem));

	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->world = MathHelper::Identity4x4();
	gridRitem->objCBIndex = 1;//BOX�������ݣ�world������objConstantBuffer����1��
	gridRitem->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->IndexCount = mGeos->DrawArgs["grid"].IndexCount;
	gridRitem->BaseVertexLocation = mGeos->DrawArgs["grid"].BaseVertexLocation;
	gridRitem->StartIndexLocation = mGeos->DrawArgs["grid"].StartIndexLocation;
	mAllRitems.push_back(std::move(gridRitem));

	UINT fllowObjCBIndex = 2;//����ȥ�ļ����峣��������CB�е�������2��ʼ
	//��Բ����Բ��ʵ��ģ�ʹ�����Ⱦ����
	for (int i = 0; i < 5; i++)
	{
		auto leftCylinderRitem = std::make_unique<RenderItem>();
		auto rightCylinderRitem = std::make_unique<RenderItem>();
		auto leftSphereRitem = std::make_unique<RenderItem>();
		auto rightSphereRitem = std::make_unique<RenderItem>();

		XMMATRIX leftCylWorld = XMMatrixTranslation(-5.0f, 1.5f, -10.0f + i * 5.0f);
		XMMATRIX rightCylWorld = XMMatrixTranslation(+5.0f, 1.5f, -10.0f + i * 5.0f);
		XMMATRIX leftSphereWorld = XMMatrixTranslation(-5.0f, 3.5f, -10.0f + i * 5.0f);
		XMMATRIX rightSphereWorld = XMMatrixTranslation(+5.0f, 3.5f, -10.0f + i * 5.0f);
		//���5��Բ��
		XMStoreFloat4x4(&(leftCylinderRitem->world), leftCylWorld);
		//�˴�����������ѭ�����ϼ�1��ע�⣺�������ȸ�ֵ��++��
		leftCylinderRitem->objCBIndex = fllowObjCBIndex++;
		leftCylinderRitem->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftCylinderRitem->IndexCount = mGeos->DrawArgs["cylinder"].IndexCount;
		leftCylinderRitem->BaseVertexLocation = mGeos->DrawArgs["cylinder"].BaseVertexLocation;
		leftCylinderRitem->StartIndexLocation = mGeos->DrawArgs["cylinder"].StartIndexLocation;
		//�ұ�5��Բ��
		XMStoreFloat4x4(&(rightCylinderRitem->world), rightCylWorld);
		rightCylinderRitem->objCBIndex = fllowObjCBIndex++;
		rightCylinderRitem->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightCylinderRitem->IndexCount = mGeos->DrawArgs["cylinder"].IndexCount;
		rightCylinderRitem->BaseVertexLocation = mGeos->DrawArgs["cylinder"].BaseVertexLocation;
		rightCylinderRitem->StartIndexLocation = mGeos->DrawArgs["cylinder"].StartIndexLocation;
		//���5����
		XMStoreFloat4x4(&(leftSphereRitem->world), leftSphereWorld);
		leftSphereRitem->objCBIndex = fllowObjCBIndex++;
		leftSphereRitem->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftSphereRitem->IndexCount = mGeos->DrawArgs["sphere"].IndexCount;
		leftSphereRitem->BaseVertexLocation = mGeos->DrawArgs["sphere"].BaseVertexLocation;
		leftSphereRitem->StartIndexLocation = mGeos->DrawArgs["sphere"].StartIndexLocation;
		//�ұ�5����
		XMStoreFloat4x4(&(rightSphereRitem->world), rightSphereWorld);
		rightSphereRitem->objCBIndex = fllowObjCBIndex++;
		rightSphereRitem->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightSphereRitem->IndexCount = mGeos->DrawArgs["sphere"].IndexCount;
		rightSphereRitem->BaseVertexLocation = mGeos->DrawArgs["sphere"].BaseVertexLocation;
		rightSphereRitem->StartIndexLocation = mGeos->DrawArgs["sphere"].StartIndexLocation;

		mAllRitems.push_back(std::move(leftCylinderRitem));
		mAllRitems.push_back(std::move(rightCylinderRitem));
		mAllRitems.push_back(std::move(leftSphereRitem));
		mAllRitems.push_back(std::move(rightSphereRitem));
	}
	for (auto& e : mAllRitems)
		mOpaqueRitems.push_back(e.get());
}

void LandAndWaves::DrawRenderItems()
{
	//������Ⱦ������
	for (size_t i = 0; i < mOpaqueRitems.size(); i++)
	{
		auto ritem = mOpaqueRitems[i];

		mCommandList->IASetVertexBuffers(0, 1, &mGeos->VertexBufferView());
		mCommandList->IASetIndexBuffer(&mGeos->IndexBufferView());
		mCommandList->IASetPrimitiveTopology(ritem->primitiveType);

		//���ø���������1
		UINT objCbvIndex = mCurrFrameResourceIndex * (UINT)mOpaqueRitems.size() + ritem->objCBIndex;;
		auto handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		handle.Offset(objCbvIndex, mCbvSrvUavDescriptorSize);
		mCommandList->SetGraphicsRootDescriptorTable(0, //����������ʼ����
			handle);

		//���ƶ��㣨ͨ���������������ƣ�
		mCommandList->DrawIndexedInstanced(ritem->IndexCount, //ÿ��ʵ��Ҫ���Ƶ�������
			1,	//ʵ��������
			ritem->StartIndexLocation,	//��ʼ����λ��
			ritem->BaseVertexLocation,	//��������ʼ������ȫ�������е�λ��
			0);	//ʵ�����ĸ߼���������ʱ����Ϊ0
	}
}

void LandAndWaves::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
			1, (UINT)mAllRitems.size()));
	}
}