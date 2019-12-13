
#include "GraphicEngine.h"
#include "Util.h"
#include "ObjectBuilder.h"
#include "CameraDynamic.h"


using namespace Microsoft::WRL;
using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace std;

const int gNumFrameResources = 3;

// Lightweight structure stores parameters to draw a shape. This will vary from app-to-app.
struct RenderItem
{
	// World matrix of the shape that describes the object's local space
	// relative to the world space, which defines the position, orientation,
	// and scale of the object in the world.
	XMFLOAT4X4 World = UtilMath::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	MeshGeometry* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};

class MyEngine : public GraphicEngine
{
public:
	MyEngine(HINSTANCE hInstance);
	MyEngine(const MyEngine& rhs) = delete;
	MyEngine& operator=(const MyEngine& rhs) = delete;
	~MyEngine();

	virtual bool Initialize()override;

private:
	virtual void OnResize()override;
	virtual void Update(const Timer& m_timer)override;
	virtual void Draw(const Timer& m_timer)override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

	void OnKeyboardInput(const Timer& m_timer);
	void UpdateObjectCBs();
	void UpdateMainPassCB(const Timer& m_timer);

	void BuildDescriptorHeaps();
	void BuildConstantBufferViews();
	void BuildRootSignature();
	void BuildShaders();
	void BuildInputLayout();
	void BuildShapeGeometry();
	void BuildPSO();
	void BuildFrameResources();
	void BuildRenderItems();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const vector<RenderItem*>& ritems);

private:

	vector<unique_ptr<Resource>> m_resources;
	Resource* m_currentResource = nullptr;
	int m_currentResourceIndex = 0;

	ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
	ComPtr<ID3D12DescriptorHeap> m_cbvHeap = nullptr;

	ComPtr<ID3D12DescriptorHeap> m_srvDescriptorHeap = nullptr;

	unordered_map<string, unique_ptr<MeshGeometry>> m_geometries;
	unordered_map<string, ComPtr<ID3DBlob>> m_shaders;
	ComPtr<ID3D12PipelineState> m_PSO;

	vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;

	// List of all the render items.
	vector<unique_ptr<RenderItem>> m_renderItems;

	// Render items divided by PSO.
	vector<RenderItem*> m_opaqueRenderItems;

	PassConstants m_mainPassCB;

	UINT m_passCbvOffset = 0;

	Camera m_Camera;

	POINT m_mousePosition;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
	try
	{
		MyEngine theApp(hInstance);
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

MyEngine::MyEngine(HINSTANCE hInstance) : GraphicEngine(hInstance)
{
}

MyEngine::~MyEngine()
{
	if (md3dDevice != nullptr)
		FlushCommandQueue();
}

bool MyEngine::Initialize()
{
	if (!GraphicEngine::Initialize())
		return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	m_Camera.SetPosition(0.0f, 5.0f, -30.0f);

	BuildRootSignature();
	BuildShaders();
	BuildInputLayout();
	BuildShapeGeometry();
	BuildRenderItems();
	BuildFrameResources();
	BuildDescriptorHeaps();
	BuildConstantBufferViews();
	BuildPSO();

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	return true;
}

void MyEngine::OnResize()
{
	GraphicEngine::OnResize();

	// The window resized, so update the aspect ratio and recompute the projection matrix.
	m_Camera.SetFrustum(0.25f*UtilMath::Pi, AspectRatio(), 1.0f, 1000.0f);
}

void MyEngine::Update(const Timer& m_timer)
{
	OnKeyboardInput(m_timer);

	// Cycle through the circular frame resource array.
	m_currentResourceIndex = (m_currentResourceIndex + 1) % gNumFrameResources;
	m_currentResource = m_resources[m_currentResourceIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	if (m_currentResource->Fence != 0 && mFence->GetCompletedValue() < m_currentResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(m_currentResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	UpdateObjectCBs();
	UpdateMainPassCB(m_timer);
}

void MyEngine::Draw(const Timer& m_timer)
{
	auto cmdListAlloc = m_currentResource->CmdListAlloc;

	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(cmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), m_PSO.Get()));


	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Clear the back buffer and depth buffer.
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightGray, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { m_cbvHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(m_rootSignature.Get());

	int passCbvIndex = m_passCbvOffset + m_currentResourceIndex;
	auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_cbvHeap->GetGPUDescriptorHandleForHeapStart());
	passCbvHandle.Offset(passCbvIndex, mCbvSrvUavDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(1, passCbvHandle);

	DrawRenderItems(mCommandList.Get(), m_opaqueRenderItems);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Swap the back and front buffers
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Advance the fence value to mark commands up to this fence point.
	m_currentResource->Fence = ++mCurrentFence;

	// Add an instruction to the command queue to set a new fence point. 
	// Because we are on the GPU timeline, the new fence point won't be 
	// set until the GPU finishes processing all the commands prior to this Signal().
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void MyEngine::UpdateObjectCBs()
{

	//std::wstring text = L"/////// NON ANCORA CAMBIATO\n";
	//::OutputDebugString(text.c_str());

	auto currObjectCB = m_currentResource->ObjectCB.get();
	for (auto& e : m_renderItems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, DirectX::XMMatrixTranspose(world));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;

			//std::wstring text2 = L"/////// CAMBIATO\n";
			//::OutputDebugString(text2.c_str());
		}
	}
}

void MyEngine::UpdateMainPassCB(const Timer& m_timer)
{
	XMMATRIX view = m_Camera.GetView();
	XMMATRIX proj = m_Camera.GetProj();

	XMMATRIX viewProj = DirectX::XMMatrixMultiply(view, proj);
	XMMATRIX invView = DirectX::XMMatrixInverse(&DirectX::XMMatrixDeterminant(view), view);
	XMMATRIX invProj = DirectX::XMMatrixInverse(&DirectX::XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = DirectX::XMMatrixInverse(&DirectX::XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&m_mainPassCB.View, DirectX::XMMatrixTranspose(view));
	XMStoreFloat4x4(&m_mainPassCB.InvView, DirectX::XMMatrixTranspose(invView));
	XMStoreFloat4x4(&m_mainPassCB.Proj, DirectX::XMMatrixTranspose(proj));
	XMStoreFloat4x4(&m_mainPassCB.InvProj, DirectX::XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&m_mainPassCB.ViewProj, DirectX::XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&m_mainPassCB.InvViewProj, DirectX::XMMatrixTranspose(invViewProj));

	m_mainPassCB.EyePosW = m_Camera.GetPosition();
	m_mainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	m_mainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	m_mainPassCB.NearZ = 1.0f;
	m_mainPassCB.FarZ = 1000.0f;
	m_mainPassCB.TotalTime = m_timer.TotTime();
	m_mainPassCB.DeltaTime = m_timer.DTime();
	m_mainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	m_mainPassCB.FresnelR0 = { 0.02f, 0.02f, 0.02f };
	m_mainPassCB.Roughness = 0.1f;
	m_mainPassCB.Strength = { 2.0f, 2.0f, 2.0f };
	m_mainPassCB.FalloffStart = 0.3f;
	m_mainPassCB.Direction = { -1.0f, -1.0f, 0.0f };
	m_mainPassCB.FalloffEnd = 15.0f;
	m_mainPassCB.Position = { 0.0f, 2.5f, 0.0f };

	auto currPassCB = m_currentResource->PassCB.get();
	currPassCB->CopyData(0, m_mainPassCB);
}

void MyEngine::BuildDescriptorHeaps()
{
	UINT objCount = (UINT)m_opaqueRenderItems.size();

	// Need a CBV descriptor for each object for each frame resource,
	// +1 for the perPass CBV for each frame resource.
	UINT numDescriptors = (objCount + 1) * gNumFrameResources;

	// Save an offset to the start of the pass CBVs.  These are the last 3 descriptors.
	m_passCbvOffset = objCount * gNumFrameResources;

	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NumDescriptors = numDescriptors;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_cbvHeap)));
}

void MyEngine::BuildConstantBufferViews()
{
	UINT objCBByteSize = Util::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	UINT objCount = (UINT)m_opaqueRenderItems.size();

	// Need a CBV descriptor for each object for each frame resource.
	for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
	{
		auto objectCB = m_resources[frameIndex]->ObjectCB->GetUploadBuffer();
		for (UINT i = 0; i < objCount; ++i)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = objectCB->GetGPUVirtualAddress();

			// Offset to the ith object constant buffer in the buffer.
			cbAddress += i*objCBByteSize;

			// Offset to the object cbv in the descriptor heap.
			int heapIndex = frameIndex*objCount + i;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_cbvHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = objCBByteSize;

			md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
		}
	}

	UINT passCBByteSize = Util::CalcConstantBufferByteSize(sizeof(PassConstants));

	// Last three descriptors are the pass CBVs for each frame resource.
	for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
	{
		auto passCB = m_resources[frameIndex]->PassCB->GetUploadBuffer();
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress();

		// Offset to the pass cbv in the descriptor heap.
		int heapIndex = m_passCbvOffset + frameIndex;
		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_cbvHeap->GetCPUDescriptorHandleForHeapStart());
		handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = cbAddress;
		cbvDesc.SizeInBytes = passCBByteSize;

		md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
	}
}

void MyEngine::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE cbv0;
	cbv0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE cbv1;
	cbv1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER rootParameter[2];

	// Create root CBVs.
	rootParameter[0].InitAsDescriptorTable(1, &cbv0);
	rootParameter[1].InitAsDescriptorTable(1, &cbv1);

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, rootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSignature = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSignature.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSignature->GetBufferPointer(),
		serializedRootSignature->GetBufferSize(),
		IID_PPV_ARGS(m_rootSignature.GetAddressOf())));
}

void MyEngine::BuildShaders()
{
	m_shaders["standardVS"] = Util::CompileShader(L"Shaders\\color.hlsl", nullptr, "VS", "vs_5_1");
	m_shaders["opaquePS"] = Util::CompileShader(L"Shaders\\color.hlsl", nullptr, "PS", "ps_5_1");
}

void MyEngine::BuildInputLayout()
{
	m_inputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

void MyEngine::BuildShapeGeometry()
{
	ObjectBuilder geoGen;
	ObjectBuilder::MeshData box = geoGen.CreateBox(1.5f, 1.5f, 1.5f);
	ObjectBuilder::MeshData grid = geoGen.CreateGrid(50.0f, 50.0f, 10, 10);
	ObjectBuilder::MeshData pyr = geoGen.CreatePyramid(2.0f, 2.0f, 4.0f);

	// We are concatenating all the geometry into one big vertex/index buffer. So we define the regions in the buffer each submesh covers.

	// Cache the vertex offsets to each object in the concatenated vertex buffer.
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
	UINT pyrVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();

	// Cache the starting index for each object in the concatenated index buffer.
	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT pyrIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();

	// Define the SubmeshGeometry that cover different regions of the vertex/index buffers.

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;

	SubmeshGeometry pyrSubMesh;
	pyrSubMesh.IndexCount = (UINT)pyr.Indices32.size();
	pyrSubMesh.StartIndexLocation = pyrIndexOffset;
	pyrSubMesh.BaseVertexLocation = pyrVertexOffset;


	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.

	auto totalVertexCount = box.Vertices.size() + grid.Vertices.size() + pyr.Vertices.size();

	// Resources Vertex struct
	vector<Vertex> vertices(totalVertexCount);

	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::DarkGreen);
	}

	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Normal = grid.Vertices[i].Normal;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Aqua);
	}

	for (size_t i = 0; i < pyr.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = pyr.Vertices[i].Position;
		vertices[k].Normal = pyr.Vertices[i].Normal;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Coral);
	}

	vector<uint32_t> indices;
	indices.insert(indices.end(), begin(box.Indices32), end(box.Indices32));
	indices.insert(indices.end(), begin(grid.Indices32), end(grid.Indices32));
	indices.insert(indices.end(), begin(pyr.Indices32), end(pyr.Indices32));


	const UINT vertexBuff_size = (UINT)vertices.size() * sizeof(Vertex);
	const UINT indexBuff_size = (UINT)indices.size() * sizeof(uint32_t);

	auto geo = make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";

	ThrowIfFailed(D3DCreateBlob(vertexBuff_size, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vertexBuff_size);

	ThrowIfFailed(D3DCreateBlob(indexBuff_size, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), indexBuff_size);

	geo->VertexBufferGPU = Util::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), vertices.data(), vertexBuff_size, geo->VertexBufferUploader);

	geo->IndexBufferGPU = Util::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), indices.data(), indexBuff_size, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vertexBuff_size;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = indexBuff_size;

	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["pyr"] = pyrSubMesh;

	m_geometries[geo->Name] = move(geo);
}

void MyEngine::BuildPSO()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size() };
	opaquePsoDesc.pRootSignature = m_rootSignature.Get();
	opaquePsoDesc.VS =
	{
		static_cast<BYTE*>(m_shaders["standardVS"]->GetBufferPointer()), m_shaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS =
	{
		static_cast<BYTE*>(m_shaders["opaquePS"]->GetBufferPointer()), m_shaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&m_PSO)));
}

void MyEngine::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		m_resources.push_back(make_unique<Resource>(md3dDevice.Get(), 1, (UINT)m_renderItems.size()));
	}
}

void MyEngine::BuildRenderItems()
{
	auto boxRitem1 = make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem1->World, DirectX::XMMatrixScaling(2.0f, 2.0f, 2.0f)*DirectX::XMMatrixTranslation(-5.0f, 1.5f, -6.0f));
	boxRitem1->ObjCBIndex = 0;
	boxRitem1->Geo = m_geometries["shapeGeo"].get();
	boxRitem1->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem1->IndexCount = boxRitem1->Geo->DrawArgs["box"].IndexCount;
	boxRitem1->StartIndexLocation = boxRitem1->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem1->BaseVertexLocation = boxRitem1->Geo->DrawArgs["box"].BaseVertexLocation;
	m_renderItems.push_back(move(boxRitem1));

	auto boxRitem2 = make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem2->World, DirectX::XMMatrixScaling(3.0f, 3.0f, 3.0f)*DirectX::XMMatrixTranslation(5.0f, 2.0f, 6.0f));
	boxRitem2->ObjCBIndex = 1;
	boxRitem2->Geo = m_geometries["shapeGeo"].get();
	boxRitem2->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem2->IndexCount = boxRitem2->Geo->DrawArgs["box"].IndexCount;
	boxRitem2->StartIndexLocation = boxRitem2->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem2->BaseVertexLocation = boxRitem2->Geo->DrawArgs["box"].BaseVertexLocation;
	m_renderItems.push_back(move(boxRitem2));

	auto gridRitem = make_unique<RenderItem>();
	gridRitem->World = UtilMath::Identity4x4();
	gridRitem->ObjCBIndex = 2;
	gridRitem->Geo = m_geometries["shapeGeo"].get();
	gridRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	m_renderItems.push_back(move(gridRitem));

	auto pyrRitem = make_unique<RenderItem>();
	XMStoreFloat4x4(&pyrRitem->World, DirectX::XMMatrixTranslation(-4.0f, 0.0f, 6.0f));
	pyrRitem->ObjCBIndex = 3;
	pyrRitem->Geo = m_geometries["shapeGeo"].get();
	pyrRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	pyrRitem->IndexCount = pyrRitem->Geo->DrawArgs["pyr"].IndexCount;
	pyrRitem->StartIndexLocation = pyrRitem->Geo->DrawArgs["pyr"].StartIndexLocation;
	pyrRitem->BaseVertexLocation = pyrRitem->Geo->DrawArgs["pyr"].BaseVertexLocation;
	m_renderItems.push_back(move(pyrRitem));


	// All render items
	for (auto& e : m_renderItems)
		m_opaqueRenderItems.push_back(e.get());
}

void MyEngine::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const vector<RenderItem*>& ritems)
{
	UINT objCBByteSize = Util::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	auto objectCB = m_currentResource->ObjectCB->GetUploadBuffer();

	// For each render item
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		// Offset to the CBV in the descriptor heap for this object and for this frame resource.
		UINT cbvIndex = m_currentResourceIndex*(UINT)m_opaqueRenderItems.size() + ri->ObjCBIndex;
		auto cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_cbvHeap->GetGPUDescriptorHandleForHeapStart());
		cbvHandle.Offset(cbvIndex, mCbvSrvUavDescriptorSize);

		cmdList->SetGraphicsRootDescriptorTable(0, cbvHandle);

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

void MyEngine::OnMouseDown(WPARAM btnState, int x, int y)
{
	m_mousePosition.x = x;
	m_mousePosition.y = y;

	SetCapture(mhMainWnd);
}

void MyEngine::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void MyEngine::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25*static_cast<float>(x - m_mousePosition.x));
		float dy = XMConvertToRadians(0.25*static_cast<float>(y - m_mousePosition.y));

		m_Camera.Pitch(dy);
		m_Camera.Yaw(dx);
	}

	m_mousePosition.x = x;
	m_mousePosition.y = y;

	ReleaseCapture();
}

void MyEngine::OnKeyboardInput(const Timer& m_timer)
{
	const float delta_time = m_timer.DTime();


	// 0x8000 - using this will return 0 in all cases except for when the high bit is set. A good way to see this is representing the numbers in binary.
	if (GetAsyncKeyState('W') & 0x8000)
		m_Camera.ForwardAndBackward(0.1f);

	if (GetAsyncKeyState('S') & 0x8000)
		m_Camera.ForwardAndBackward(-0.1f);

	if (GetAsyncKeyState('A') & 0x8000)
		m_Camera.LeftAndRight(-0.1f);

	if (GetAsyncKeyState('D') & 0x8000)
		m_Camera.LeftAndRight(0.1f);

	m_Camera.UpdateViewMatrix();
}
