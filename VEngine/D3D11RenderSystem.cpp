#include "RenderSystem.h"
#include "CoreSystem.h"
#include "Actor.h"
#include "Camera.h"
#include "UISystem.h"
#include "Input.h"
#include "Console.h"
#include "DebugMenu.h"
#include "ShaderFactory.h"
#include "MathHelpers.h"
#include "World.h"
#include "Debug.h"
#include "WICTextureLoaderD3D11.h"
#include <string>
#include "WorldEditor.h"
#include "D3D11RenderSystem.h"
#include "ISampler.h"
#include "IBuffer.h";
#include "ITexture.h"
#include "IShaderView.h"

UINT strides = sizeof(Vertex);
UINT offsets = 0;

ActorSystem debugBox;
ActorSystem debugSphere;

Vertex debugLineData[2];

D3D11RenderSystem::D3D11RenderSystem()
{
	
}

void D3D11RenderSystem::Tick()
{
	//Set wireframe on/off
	if (gInputSystem.GetKeyUpState(VK_F1))
	{
		activeRastState = rastStateWireframe;
	}
	if (gInputSystem.GetKeyUpState(VK_F2))
	{
		activeRastState = rastStateSolid;
	}

	if (gInputSystem.GetKeyUpState('B')) 
	{
		bDrawBoundingBoxes = !bDrawBoundingBoxes;
	}
	if (gInputSystem.GetKeyUpState('V'))
	{
		bDrawBoundingSpheres = !bDrawBoundingSpheres;
	}
}

void D3D11RenderSystem::Init(HWND window)
{
	viewport = { 0.f, 0.f, (float)gCoreSystem.windowWidth, (float)gCoreSystem.windowHeight, 0.f, 1.f };

	CreateDevice();
	CreateSwapchain(window);
	CreateRTVAndDSV();
	CreateInputLayout();
	CreateRasterizerStates();
	CreateConstantBuffer();

	//Check feature support (just for breakpoint checking for now)
	//TODO: Think about putting this into the debug menu
	D3D11_FEATURE_DATA_THREADING threadFeature = {};
	device->CheckFeatureSupport(D3D11_FEATURE_THREADING, &threadFeature, sizeof(threadFeature));
	return;
}

void D3D11RenderSystem::CreateDefaultBuffer()
{
}

void D3D11RenderSystem::CreateVertexShader()
{
}

void D3D11RenderSystem::CreatePixelShader()
{
}

void D3D11RenderSystem::CreateAllShaders()
{
	for (int i = 0; i < gShaderFactory.shaders.size(); i++)
	{
		HR(device->CreateVertexShader(
			gShaderFactory.shaders[i].vertexCode->GetBufferPointer(), 
			gShaderFactory.shaders[i].vertexCode->GetBufferSize(), 
			nullptr, 
			&gShaderFactory.shaders[i].vertexShader));

		HR(device->CreatePixelShader(
			gShaderFactory.shaders[i].pixelCode->GetBufferPointer(),
			gShaderFactory.shaders[i].pixelCode->GetBufferSize(),
			nullptr, 
			&gShaderFactory.shaders[i].pixelShader));
	}
}

void D3D11RenderSystem::CreateDevice()
{
	D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };

	IDXGIFactory* tmpFactory;
	HR(CreateDXGIFactory(IID_PPV_ARGS(&tmpFactory)));
	HR(tmpFactory->QueryInterface(&dxgiFactory));
	tmpFactory->Release();

	//Reference for EnumAdapterByGpuPerformance
	//https://github.com/walbourn/directx-vs-templates/blob/master/d3d11game_win32_dr/DeviceResources.cpp

	IDXGIAdapter1* adapter;
	for (int i = 0; dxgiFactory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_MINIMUM_POWER, IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND; i++)
	{
		adapters.push_back(adapter);
		DXGI_ADAPTER_DESC1 desc = {};
		adapter->GetDesc1(&desc);
		adaptersDesc.push_back(desc);
	}

	UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	//BGRA support needed for DirectWrite and Direct2D
	HR(D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_HARDWARE, 0, createDeviceFlags,
		featureLevels, _countof(featureLevels), D3D11_SDK_VERSION, &device, &featureLevel, &context));


	debugLineData[0].pos = XMFLOAT3(0.f, 0.f, 0.f);
	debugLineData[1].pos = XMFLOAT3(0.f, 0.f, 100.f);

	gShaderFactory.CompileAllShadersFromFile();
	gRenderSystem->CreateAllShaders();
	gShaderFactory.InitHotLoading();

	D3D11_QUERY_DESC qd = {};
	qd.Query = D3D11_QUERY_TIMESTAMP;
	HR(device->CreateQuery(&qd, &startTimeQuery));
	HR(device->CreateQuery(&qd, &endTimeQuery));

	qd.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
	HR(device->CreateQuery(&qd, &disjointQuery));

	debugBox.modelName = "cube.fbx";
	debugBox.shaderName = L"debugDraw.hlsl";
	debugBox.CreateActors<Actor>(this, 1);

	debugSphere.shaderName = L"debugDraw.hlsl";
	debugSphere.modelName = "ico_sphere.fbx";
	debugSphere.CreateActors<Actor>(this, 1);
}

void D3D11RenderSystem::CreateSwapchain(HWND windowHandle)
{
	DXGI_SWAP_CHAIN_DESC sd = {};
	sd.BufferDesc = { (UINT)gCoreSystem.windowWidth, (UINT)gCoreSystem.windowHeight, {60, 1}, DXGI_FORMAT_R8G8B8A8_UNORM };
	sd.Windowed = TRUE;
	sd.SampleDesc.Count = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = windowHandle;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.BufferCount = frameCount;

	IDXGISwapChain* tmpSwapchain;
	HR(dxgiFactory->CreateSwapChain(device, &sd, &tmpSwapchain));
	HR(tmpSwapchain->QueryInterface(&swapchain));
	tmpSwapchain->Release();
}

void D3D11RenderSystem::CreateRTVAndDSV()
{
	for (int i = 0; i < frameCount; i++)
	{
		ID3D11Texture2D* backBuffer;
		swapchain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
		HR(device->CreateRenderTargetView(backBuffer, nullptr, &rtvs[i]));
		backBuffer->Release();
	}

	D3D11_TEXTURE2D_DESC dsDesc = {};
	dsDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsDesc.ArraySize = 1;
	dsDesc.MipLevels = 1;
	dsDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	dsDesc.SampleDesc.Count = 1;
	dsDesc.Width = gCoreSystem.windowWidth;
	dsDesc.Height = gCoreSystem.windowHeight;

	ID3D11Texture2D* depthStencilBuffer;
	HR(device->CreateTexture2D(&dsDesc, nullptr, &depthStencilBuffer));
	assert(depthStencilBuffer);
	HR(device->CreateDepthStencilView(depthStencilBuffer, nullptr, &dsv));
}

void D3D11RenderSystem::CreateShaders()
{
	//TODO: fix this for later. for now, all shaders are using the same Inputlayout so its fine
	vertexCode = CreateShaderFromFile(L"Shaders/shaders.hlsl", "VSMain", "vs_5_0");
	pixelCode = CreateShaderFromFile(L"Shaders/shaders.hlsl", "PSMain", "ps_5_0");

	HR(device->CreateVertexShader(vertexCode->GetBufferPointer(), vertexCode->GetBufferSize(), nullptr, &vertexShader));
	HR(device->CreatePixelShader(pixelCode->GetBufferPointer(), pixelCode->GetBufferSize(), nullptr, &pixelShader));
}

void D3D11RenderSystem::CreateInputLayout()
{
	D3D11_INPUT_ELEMENT_DESC inputDesc[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, pos), D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, uv), D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, normal), D3D11_INPUT_PER_VERTEX_DATA, 0}
	};

	CreateShaders();

	HR(device->CreateInputLayout(inputDesc, _countof(inputDesc), vertexCode->GetBufferPointer(), vertexCode->GetBufferSize(), &inputLayout));
	context->IASetInputLayout(inputLayout);
}

void D3D11RenderSystem::CreateRasterizerStates()
{
	D3D11_RASTERIZER_DESC rastDesc = {};
	rastDesc.FillMode = D3D11_FILL_SOLID;
	rastDesc.CullMode = D3D11_CULL_BACK;
	rastDesc.DepthClipEnable = TRUE;
	rastDesc.FrontCounterClockwise = FALSE;

	//SOLID
	HR(device->CreateRasterizerState(&rastDesc, &rastStateSolid));
	activeRastState = rastStateSolid;
	context->RSSetState(activeRastState);

	//WIREFRAME
	rastDesc.FillMode = D3D11_FILL_WIREFRAME;
	rastDesc.CullMode = D3D11_CULL_NONE;
	HR(device->CreateRasterizerState(&rastDesc, &rastStateWireframe));

	//SOLID, NO BACK CULL
	rastDesc.CullMode = D3D11_CULL_NONE;
	rastDesc.FillMode = D3D11_FILL_SOLID;
	HR(device->CreateRasterizerState(&rastDesc, &rastStateNoBackCull));
}

//One vertex buffer per actor system
void D3D11RenderSystem::CreateVertexBuffer(UINT size, const void* data, ActorSystem* system)
{
	IBuffer* buffer = new D3D11Buffer();
	buffer->data = CreateDefaultBuffer(size, D3D11_BIND_VERTEX_BUFFER, data);
	system->SetVertexBuffer(buffer);
}

IDXGISwapChain3* D3D11RenderSystem::GetSwapchain()
{
	return swapchain;
}

void D3D11RenderSystem::Present()
{
	HR(swapchain->Present(1, 0));
}

void D3D11RenderSystem::Flush()
{
	//Empty
}

void D3D11RenderSystem::CreateConstantBuffer()
{
	//Matrix constant buffer
	matrices.model = XMMatrixIdentity();
	matrices.view = XMMatrixIdentity();
	matrices.proj = XMMatrixPerspectiveFovLH(XM_PI / 3, gCoreSystem.GetAspectRatio(), 0.01f, 1000.f);

	editorCamera.proj = matrices.proj;
	matrices.mvp = matrices.model * matrices.view * matrices.proj;

	cbMatrices = CreateDefaultBuffer(sizeof(Matrices), D3D11_BIND_CONSTANT_BUFFER, &matrices);

	//Material constant buffer	
	material.baseColour = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	cbMaterial = CreateDefaultBuffer(sizeof(Material), D3D11_BIND_CONSTANT_BUFFER, &material);
}

void D3D11RenderSystem::CreateSamplerState(ActorSystem* actorSystem)
{
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;

	ISampler* sampler = new D3D11Sampler();

	ID3D11SamplerState* samplerState;
	HR(device->CreateSamplerState(&sampDesc, &samplerState));

	sampler->data = samplerState;
	actorSystem->SetSamplerState(sampler);
}

void D3D11RenderSystem::CreateTexture(ActorSystem* actorSystem)
{
	const wchar_t* textureFilename = L"Textures/";
	actorSystem->textureName = textureFilename;

	ID3D11Resource* texture;
	ID3D11ShaderResourceView* srv;

	HR(CreateWICTextureFromFile(device, textureFilename, &texture, &srv));
	if (texture && srv)
	{
		ITexture* iTexture = new D3D11Texture(texture);
		IShaderView* iShaderView = new D3D11ShaderView(srv);

		actorSystem->SetTexture(iTexture);
		actorSystem->SetShaderView(iShaderView);
	}
}

void D3D11RenderSystem::RenderActorSystem(World* world)
{
	for (int i = 0; i < world->actorSystems.size(); i++)
	{
		ActorSystem* actorSystem = world->actorSystems[i];

		//Set rastState
		if (actorSystem->GetRasterizerState())
		{
			//TODO: eye on this
			//context->RSSetState(actorSystem->rastState);
		}
		else
		{
			context->RSSetState(activeRastState);
		}

		auto shader = gShaderFactory.shaderMap.find(actorSystem->shaderName);

		if (shader == gShaderFactory.shaderMap.end())
		{
			DebugPrint("vertex shader file name %ls not found\n", actorSystem->shaderName);
		}

		assert(shader->second->vertexShader);
		assert(shader->second->pixelShader);
		context->VSSetShader(shader->second->vertexShader, nullptr, 0);
		context->PSSetShader(shader->second->pixelShader, nullptr, 0);

		context->PSSetSamplers(0, 1, (ID3D11SamplerState**)actorSystem->GetSamplerState());
		context->PSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)actorSystem->GetShaderView());

		context->IASetVertexBuffers(0, 1, (ID3D11Buffer**)actorSystem->GetVertexBuffer(), &strides, &offsets);
		//context->IASetIndexBuffer(actorSystem->indexBuffer, DXGI_FORMAT_R16_UINT, 0);
		
		//Constant buffer register values
		const int cbMatrixRegister = 0;
		const int cbMaterialRegister = 1;

		for (int i = 0; i < actorSystem->actors.size(); i++)
		{
			if (actorSystem->actors[i]->bRender)
			{
				//Set Matrix constant buffer
				matrices.view = GetActiveCamera()->view;
				matrices.model = actorSystem->actors[i]->transform;
				matrices.mvp = matrices.model * matrices.view * matrices.proj;
				context->UpdateSubresource(cbMatrices, 0, nullptr, &matrices, 0, 0);
				context->VSSetConstantBuffers(cbMatrixRegister, 1, &cbMatrices);

				//Set material constant buffer
				//TODO: set material per actor/system
				//material.colour = actor.color etc.
				context->UpdateSubresource(cbMaterial, 0, nullptr, &material, 0, 0);
				context->PSSetConstantBuffers(cbMaterialRegister, 1, &cbMaterial);

				//Draw indexed/straight
				context->Draw(actorSystem->modelData.verts.size(), 0);
				//context->DrawIndexed(actorSystem->modelData.indices.size(), 0, 0);
			}
		}
	}
}

void D3D11RenderSystem::RenderBounds()
{
	World* world = GetWorld();
	Camera* camera = GetActiveCamera();

	if (bDrawBoundingBoxes || bDrawBoundingSpheres)
	{
		context->RSSetState(rastStateWireframe);
	}

	if (bDrawBoundingBoxes)
	{
		auto boxIt = gShaderFactory.shaderMap.find(debugBox.shaderName);
		context->VSSetShader(boxIt->second->vertexShader, nullptr, 0);
		context->PSSetShader(boxIt->second->pixelShader, nullptr, 0);

		context->IASetVertexBuffers(0, 1, (ID3D11Buffer**)debugBox.GetVertexBuffer(), &strides, &offsets);

		for (int systemIndex = 0; systemIndex < world->actorSystems.size(); systemIndex++)
		{
			for (int actorIndex = 0; actorIndex < world->actorSystems[systemIndex]->actors.size(); actorIndex++)
			{
				matrices.view = camera->view;

				XMMATRIX boxBoundsMatrix = XMMatrixIdentity();
				XMVECTOR offset = XMVectorSet(
					world->actorSystems[systemIndex]->actors[actorIndex]->GetPositionFloat3().x + world->actorSystems[systemIndex]->boundingBox.Center.x,
					world->actorSystems[systemIndex]->actors[actorIndex]->GetPositionFloat3().y + world->actorSystems[systemIndex]->boundingBox.Center.y,
					world->actorSystems[systemIndex]->actors[actorIndex]->GetPositionFloat3().z + world->actorSystems[systemIndex]->boundingBox.Center.z,
					1.0f);

				boxBoundsMatrix = XMMatrixScalingFromVector(XMLoadFloat3(&world->actorSystems[systemIndex]->boundingBox.Extents));
				boxBoundsMatrix *= world->actorSystems[systemIndex]->actors[actorIndex]->GetRotation();
				boxBoundsMatrix.r[3] = offset;

				matrices.model = boxBoundsMatrix;
				matrices.mvp = matrices.model * matrices.view * matrices.proj;
				context->UpdateSubresource(cbMatrices, 0, nullptr, &matrices, 0, 0);
				context->VSSetConstantBuffers(0, 1, &cbMatrices);

				context->Draw((UINT)debugBox.modelData.verts.size(), 0);
			}
		}
	}

	if (bDrawBoundingSpheres)
	{
		auto sphereIt = gShaderFactory.shaderMap.find(debugSphere.shaderName);
		context->VSSetShader(sphereIt->second->vertexShader, nullptr, 0);
		context->PSSetShader(sphereIt->second->pixelShader, nullptr, 0);

		context->IASetVertexBuffers(0, 1, (ID3D11Buffer**)debugSphere.GetVertexBuffer(), &strides, &offsets);

		for (int systemIndex = 0; systemIndex < world->actorSystems.size(); systemIndex++)
		{
			for (int actorIndex = 0; actorIndex < world->actorSystems[systemIndex]->actors.size(); actorIndex++)
			{
				matrices.view = camera->view;

				XMMATRIX sphereBoundsMatrix = XMMatrixIdentity();

				XMVECTOR offset = XMVectorSet(
					world->actorSystems[systemIndex]->actors[actorIndex]->GetPositionFloat3().x + world->actorSystems[systemIndex]->boundingBox.Center.x,
					world->actorSystems[systemIndex]->actors[actorIndex]->GetPositionFloat3().y + world->actorSystems[systemIndex]->boundingBox.Center.y,
					world->actorSystems[systemIndex]->actors[actorIndex]->GetPositionFloat3().z + world->actorSystems[systemIndex]->boundingBox.Center.z,
					1.0f);

				XMVECTOR boundingSphereScaleFromRadius = XMVectorReplicate(world->actorSystems[systemIndex]->boundingSphere.Radius);
				sphereBoundsMatrix = XMMatrixScalingFromVector(boundingSphereScaleFromRadius);

				sphereBoundsMatrix.r[3] = offset;

				matrices.model = sphereBoundsMatrix;
				matrices.mvp = matrices.model * matrices.view * matrices.proj;
				context->UpdateSubresource(cbMatrices, 0, nullptr, &matrices, 0, 0);
				context->VSSetConstantBuffers(0, 1, &cbMatrices);

				context->Draw((UINT)debugSphere.modelData.verts.size(), 0);
			}
		}
	}
}

void D3D11RenderSystem::Render(float deltaTime)
{
	RenderActorSystem(GetWorld());
	RenderBounds();
}

void D3D11RenderSystem::RenderEnd(float deltaTime)
{
	//TODO: put debug line buffers into rendersystem
	//DRAW DEBUG LINES
	/*if (debugLineBuffer != nullptr)
	{
		context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
		context->IASetVertexBuffers(0, 1, &debugLineBuffer, &strides, &offsets);

		for (int i = 0; i < debugLines.size(); i++)
		{
			matrices.view = GetPlayerCamera()->view;
			matrices.model = XMMatrixIdentity();
			matrices.mvp = matrices.model * matrices.view * matrices.proj;
			context->UpdateSubresource(cbMatrices, 0, nullptr, &matrices, 0, 0);
			context->VSSetConstantBuffers(0, 1, &cbMatrices);

			context->Draw((UINT)debugLines.size(), 0);
		}
	}*/

	//END QUERY
	if (bQueryGPUInner)
	{
		context->End(endTimeQuery);
		context->End(disjointQuery);

		//POLL QUERY
		while (context->GetData(disjointQuery, nullptr, 0, 0) == S_FALSE)
		{
			Sleep(1);
		}


		D3D11_QUERY_DATA_TIMESTAMP_DISJOINT freq = {};
		HR(context->GetData(disjointQuery, &freq, sizeof(freq), 0));

		//Is the thread polling necessary?
		while (context->GetData(startTimeQuery, nullptr, 0, 0) == S_FALSE)
		{
			Sleep(1);
		}

		while (context->GetData(endTimeQuery, nullptr, 0, 0) == S_FALSE)
		{
			Sleep(1);
		}

		UINT64 endTime = 0, startTime = 0;
		HR(context->GetData(startTimeQuery, &startTime, sizeof(UINT64), 0));
		HR(context->GetData(endTimeQuery, &endTime, sizeof(UINT64), 0));

		UINT64 realTime = endTime - startTime;
		double tick = 1.0 / freq.Frequency;
		double time = tick * (realTime);

		renderTime = time;
	}
}

void D3D11RenderSystem::RenderSetup(float deltaTime)
{
	if (bQueryGPU)
	{
		context->Begin(disjointQuery);
		context->End(startTimeQuery);
		bQueryGPUInner = true;
	}
	else
	{
		bQueryGPUInner = false;
	}

	context->RSSetViewports(1, &viewport);

	const float clearColour[4] = { 0.2f, 0.2f, 0.2f, 1.f };
	UINT frameIndex = swapchain->GetCurrentBackBufferIndex();

	context->ClearRenderTargetView(rtvs[frameIndex], clearColour);
	context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);

	context->OMSetRenderTargets(1, &rtvs[frameIndex], dsv);
	context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	context->RSSetState(activeRastState);

	if (gInputSystem.GetKeyUpState(VK_F3))
	{
		gShaderFactory.HotReloadShaders();
		debugMenu.notifications.push_back(DebugNotification(L"Shaders reloaded."));
	}
}

ID3DBlob* D3D11RenderSystem::CreateShaderFromFile(const wchar_t* filename, const char* entry, const char* target)
{
	UINT compileFlags = 0;
#ifdef _DEBUG
	compileFlags = D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_DEBUG;
#endif
	ID3DBlob* code;
	ID3DBlob* error;
	
	D3DCompileFromFile(filename, nullptr, nullptr, entry, target, compileFlags, 0, &code, &error);
	if (error)
	{
		const wchar_t* errMsg = (wchar_t*)error->GetBufferPointer();
		//OutputDebugString(errMsg);
		//MessageBox(0, errMsg, entry, 0);
	}

	return code;
}

ID3D11Buffer* D3D11RenderSystem::CreateDefaultBuffer(UINT byteWidth, UINT bindFlags, const void* initData)
{
	ID3D11Buffer* buffer;

	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = byteWidth;
	desc.BindFlags = bindFlags;

	D3D11_SUBRESOURCE_DATA data = {};
	data.pSysMem = initData;

	HR(device->CreateBuffer(&desc, &data, &buffer));

	return buffer;
}

ID3D11Buffer* D3D11RenderSystem::CreateDyamicBuffer(UINT byteWidth, UINT bindFlags, const void* initData)
{
	ID3D11Buffer* buffer;

	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = byteWidth;
	desc.BindFlags = bindFlags;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	D3D11_SUBRESOURCE_DATA data = {};
	data.pSysMem = initData;

	HR(device->CreateBuffer(&desc, &data, &buffer));

	return buffer;
}


