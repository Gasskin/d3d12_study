#include "d3dApp.h"
#include "UploadBuffer.h"
#include "MathHelper.h"
#include <WindowsX.h>

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

//====================================================
//������Ϣ
//====================================================

//���泣����Ϣ��mvp����
struct ObjectConstants
{
	XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
};

//����
struct VPosData
{
	XMFLOAT3 Pos;
};
struct VColorData
{
	XMFLOAT4 Color;
};

//====================================================
//����
//====================================================
class BoxApp :public D3DApp
{
public:
	BoxApp(HINSTANCE hInstance);
	BoxApp(const BoxApp& rhs) = delete;
	BoxApp& operator=(const BoxApp& rhs) = delete;
	~BoxApp();

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
	void BuildBoxGeometry();//���������壨������������
	void BuildPSO();//������ˮ�߶���
private:
	void OnMouseDown(WPARAM btnState, int x, int y);
	void OnMouseUp(WPARAM btnState, int x, int y);
	void OnMouseMove(WPARAM btnState, int x, int y);
private:
	ComPtr<ID3D12DescriptorHeap>					mCbvHeap = nullptr;//���������� ��������

	std::unique_ptr<UploadBuffer<ObjectConstants>>	mObjectCB = nullptr;//����������

	ComPtr<ID3D12RootSignature>						mRootSignature = nullptr;//��ǩ��

	ComPtr<ID3DBlob>								mvsByteCode = nullptr;//������ɫ��
	ComPtr<ID3DBlob>								mpsByteCode = nullptr;//������ɫ��
	std::vector<D3D12_INPUT_ELEMENT_DESC>			mInputLayout;//���벼��

	std::unique_ptr<MeshGeometry>					mBoxGeo = nullptr;//���ӵĶ�������������

	ComPtr<ID3D12PipelineState>						mPSO = nullptr;//��ˮ�߶���
private:
	XMFLOAT4X4	mWorld = MathHelper::Identity4x4();//�������
	XMFLOAT4X4	mView = MathHelper::Identity4x4();//�۲����
	XMFLOAT4X4	mProj = MathHelper::Identity4x4();//ͶӰ����

	float		mTheta = 1.5f * XM_PI; //1.5pai
	float		mPhi = XM_PIDIV4;//�ķ�֮pai
	float		mRadius = 5.0f;//����
private:
	POINT	mLastMousePos;
};

BoxApp::BoxApp(HINSTANCE hInstance)
	:D3DApp(hInstance)
{
}

BoxApp::~BoxApp()
{
}

//���
bool BoxApp::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	//���������б������������
	ThrowIfFailed(mDirectCmdListAlloc->Reset());
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));
	//��ʼ��
	BuildDescriptorHeaps();
	BuildConstantBuffers();
	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildBoxGeometry();
	BuildPSO();
	//�ر������б�
	ThrowIfFailed(mCommandList->Close());
	//ִ������
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	//Χ��
	FlushCommandQueue();

	return true;
}

void BoxApp::Update(const GameTimer& gt)
{
	float x = mRadius * sinf(mPhi) * cosf(mTheta);
	float z = mRadius * sinf(mPhi) * sinf(mTheta);
	float y = mRadius * cosf(mPhi);

	XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);

	XMMATRIX world = XMLoadFloat4x4(&mWorld);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);
	XMMATRIX worldViewProj = world * view * proj;

	ObjectConstants objConstants;
	XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(worldViewProj));
	mObjectCB->CopyData(0, objConstants);
}

void BoxApp::Draw(const GameTimer& gt)
{
	ThrowIfFailed(mDirectCmdListAlloc->Reset());

	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSO.Get()));

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

	//���ö��㻺��
	mCommandList->IASetVertexBuffers(0, 1, &mBoxGeo->vPosBufferView());
	mCommandList->IASetVertexBuffers(1, 1, &mBoxGeo->vColorBufferView());
	//������������
	mCommandList->IASetIndexBuffer(&mBoxGeo->IndexBufferView());
	//����ͼԪ����
	mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	//���ø���������
	mCommandList->SetGraphicsRootDescriptorTable(0, mCbvHeap->GetGPUDescriptorHandleForHeapStart());

	mCommandList->DrawIndexedInstanced(
		mBoxGeo->DrawArgs["box"].IndexCount,
		1, 0, 0, 0);

	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(mSwapChainBuffer[mCurrBackBuffer].Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(mCommandList->Close());

	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	FlushCommandQueue();
}

void BoxApp::OnResize()
{
	D3DApp::OnResize();

	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);
}

void BoxApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;
	SetCapture(mhMainWnd);
}

void BoxApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void BoxApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	//������������״̬
	if ((btnState & MK_LBUTTON) != 0)
	{
		//�������ƶ����뻻��ɻ��ȣ�0.25Ϊ������ֵ
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));
		//�������û���ɿ�ǰ���ۼƻ���
		mTheta += dx;
		mPhi += dy;
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
		mRadius += dx - dy;
		//���ƿ��ӷ�Χ�뾶
		mRadius = MathHelper::Clamp(mRadius, 3.0f, 15.0f);
	}
	//����ǰ������긳ֵ������һ��������ꡱ��Ϊ��һ���������ṩ��ǰֵ
	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

LRESULT BoxApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
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
		BoxApp theApp(hInstance);
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
void BoxApp::BuildDescriptorHeaps()
{
	//���� ��������
	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NumDescriptors = 1;
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

void BoxApp::BuildConstantBuffers()
{
	//������������������UploadBuffer���ж����˴����ѵĹ���
	mObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(md3dDevice.Get(), 1, true);

	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	D3D12_GPU_VIRTUAL_ADDRESS cbAddress = mObjectCB->Resource()->GetGPUVirtualAddress();

	int boxCBufIndex = 0;
	cbAddress += boxCBufIndex * objCBByteSize;
	//���ﴴ������CBV
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
	cbvDesc.BufferLocation = cbAddress;
	cbvDesc.SizeInBytes = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	md3dDevice->CreateConstantBufferView(
		&cbvDesc,
		mCbvHeap->GetCPUDescriptorHandleForHeapStart());
}

void BoxApp::BuildRootSignature()
{
	CD3DX12_ROOT_PARAMETER slotRootParameter[1];

	CD3DX12_DESCRIPTOR_RANGE cbvTable;
	cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr,
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

void BoxApp::BuildShadersAndInputLayout()
{
	HRESULT hr = S_OK;

	mvsByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "VS", "vs_5_0");
	mpsByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "PS", "ps_5_0");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void BoxApp::BuildBoxGeometry()
{
	std::array<VPosData, 5> verticesPos =
	{
		VPosData({ XMFLOAT3(+0.0f, +1.0f, +0.0f) }),
		VPosData({ XMFLOAT3(-1.0f, -1.0f, -1.0f) }),
		VPosData({ XMFLOAT3(+1.0f, -1.0f, -1.0f) }),
		VPosData({ XMFLOAT3(+1.0f, -1.0f, +1.0f) }),
		VPosData({ XMFLOAT3(-1.0f, -1.0f, +1.0f) })
	};
	std::array<VColorData, 5> verticesColor =
	{
		VColorData({ XMFLOAT4(Colors::Red) }),
		VColorData({ XMFLOAT4(Colors::Green) }),
		VColorData({ XMFLOAT4(Colors::Green) }),
		VColorData({ XMFLOAT4(Colors::Green) }),
		VColorData({ XMFLOAT4(Colors::Green) })
	};

	std::array<std::uint16_t, 36> indices =
	{
		//ǰ
		0, 2, 1,

		//��
		0, 4, 3,

		//��
		0, 1, 4,

		//��
		0, 3, 2,

		//��
		2, 4, 1,
		2, 3, 4
	};

	const UINT vPosByteSize = (UINT)verticesPos.size() * sizeof(VPosData);
	const UINT vColorByteSize = (UINT)verticesColor.size() * sizeof(VColorData);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	mBoxGeo = std::make_unique<MeshGeometry>();
	mBoxGeo->Name = "boxGeo";

	//�����ݸ��Ƶ�CPU����ʱ��������ã�ע�͵�Ҳ����������
	/*ThrowIfFailed(D3DCreateBlob(vbByteSize, &mBoxGeo->VertexBufferCPU));
	CopyMemory(mBoxGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &mBoxGeo->IndexBufferCPU));
	CopyMemory(mBoxGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);*/

	mBoxGeo->vPosBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), verticesPos.data(), vPosByteSize, mBoxGeo->vPosBufferUploader);
	mBoxGeo->vColorBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), verticesColor.data(), vColorByteSize, mBoxGeo->vColorBufferUploader);

	mBoxGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, mBoxGeo->IndexBufferUploader);

	mBoxGeo->vPosByteStride = sizeof(VPosData);
	mBoxGeo->vPosBufferByteSize = vPosByteSize;
	mBoxGeo->vColorByteStride = sizeof(VColorData);
	mBoxGeo->vColorBufferByteSize = vColorByteSize;

	mBoxGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
	mBoxGeo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	mBoxGeo->DrawArgs["box"] = submesh;

}

void BoxApp::BuildPSO()
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
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));
}
