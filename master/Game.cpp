#include "Game.h"
#include "Vertex.h"
#include "Input.h"
#include "BufferStructs.h"
#include "DX12Helper.h"
//#include "Material.h" ? already in Entity class

#include <stdlib.h> //Seeding random and rand()
#include <time.h>   //We will use current time to seed random.

// Needed for a helper function to read compiled shader files from the hard drive
#pragma comment(lib, "d3dcompiler.lib")
#include <d3dcompiler.h>

// For the DirectX Math library
using namespace DirectX;

#define RandomRange(min, max) (float)rand() / RAND_MAX * max - min + min

// --------------------------------------------------------
// Constructor
//
// DXCore (base class) constructor will set up underlying fields.
// DirectX itself, and our window, are not ready yet!
//
// hInstance - the application's OS-level handle (unique ID)
// --------------------------------------------------------
Game::Game(HINSTANCE hInstance)
	: DXCore(
		hInstance,		   // The application's handle
		"DirectX Game",	   // Text for the window's title bar
		1280,			   // Width of the window's client area
		720,			   // Height of the window's client area
		true),			   // Show extra stats (fps) in title bar?
	vsync(false)
{
#if defined(DEBUG) || defined(_DEBUG)
	// Do we want a console window?  Probably only in debug mode
	CreateConsoleWindow(500, 120, 32, 120);
	printf("Console window created successfully.  Feel free to printf() here.\n");
#endif
}

// --------------------------------------------------------
// Destructor - Clean up anything our game has created:
//  - Release all DirectX objects created here
//  - Delete any objects to prevent memory leaks
// --------------------------------------------------------
Game::~Game()
{
	//Need to wait until GPU is done with its work otherwise we will get errors
	DX12Helper::GetInstance().WaitForGPU();
}

// --------------------------------------------------------
// Called once per program, after DirectX and the window
// are initialized but before the game loop.
// --------------------------------------------------------
void Game::Init()
{
	//Random time!
	srand((unsigned int)time(0));

	// Helper methods for loading shaders, creating some basic
	// geometry to draw and some simple camera matrices.
	//  - You'll be expanding and/or replacing these later
	CreateRootSigAndPipelineState();
	CreateBasicGeometry();
	GenerateLights();
	
	//camera = std::make_shared<Camera>(0.0f, 0.0f, -5.0, 1.0f, XM_PIDIV4, width / (float)height);
	camera = std::make_shared<Camera>(0.0f, 0.0f, -5.0, width / (float)height);
}

// --------------------------------------------------------
// Loads the two basic shaders, then creates the root signature 
// and pipeline state object for our very basic demo.
// --------------------------------------------------------
void Game::CreateRootSigAndPipelineState()
{
	// Blobs to hold raw shader byte code used in several steps below
	Microsoft::WRL::ComPtr<ID3DBlob> vertexShaderByteCode;
	Microsoft::WRL::ComPtr<ID3DBlob> pixelShaderByteCode;
	// Load shaders
	{
		// Read our compiled vertex shader code into a blob
		// - Essentially just "open the file and plop its contents here"
		D3DReadFileToBlob(GetFullPathTo_Wide(L"VertexShader.cso").c_str(),
			vertexShaderByteCode.GetAddressOf());
		D3DReadFileToBlob(GetFullPathTo_Wide(L"PixelShader.cso").c_str(),
			pixelShaderByteCode.GetAddressOf());
	}

	// Input layout
	const unsigned int inputElementCount = 4;
	D3D12_INPUT_ELEMENT_DESC inputElements[inputElementCount] = {};
	{
		// Create an input layout that describes the vertex format
		// used by the vertex shader we're using
		//  - This is used by the pipeline to know how to interpret the raw data
		//     sitting inside a vertex buffer
		// Set up the first element - a position, which is 3 float values
		inputElements[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
		inputElements[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		inputElements[0].SemanticName = "POSITION";
		inputElements[0].SemanticIndex = 0;
		// Set up the second element - a UV, which is 2 more float values
		inputElements[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;	// After the previous element
		inputElements[1].Format = DXGI_FORMAT_R32G32_FLOAT;			// 2x 32-bit floats
		inputElements[1].SemanticName = "TEXCOORD";					// Match our vertex shader input!
		inputElements[1].SemanticIndex = 0;
		// Set up the third element - a normal, which is 3 more float values
		inputElements[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;	// After the previous element
		inputElements[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;		// 3x 32-bit floats
		inputElements[2].SemanticName = "NORMAL";					// Match our vertex shader input!
		inputElements[2].SemanticIndex = 0;							// This is the 0th normal (there could be more)

		// Set up the fourth element - a tangent, which is 2 more float values
		inputElements[3].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;	// After the previous element
		inputElements[3].Format = DXGI_FORMAT_R32G32B32_FLOAT;		// 3x 32-bit floats
		inputElements[3].SemanticName = "TANGENT";					// Match our vertex shader input!
		inputElements[3].SemanticIndex = 0;
	}

	// Root Signature
	{
		//// Create a table of CBV's (constant buffer views)
		//D3D12_DESCRIPTOR_RANGE cbvTable = {};
		//cbvTable.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		//cbvTable.NumDescriptors = 1;
		//cbvTable.BaseShaderRegister = 0;
		//cbvTable.RegisterSpace = 0;
		//cbvTable.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		//*New*

		// Describe the range of CBVs needed for the vertex shader
		D3D12_DESCRIPTOR_RANGE cbvRangeVS = {};
		cbvRangeVS.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		cbvRangeVS.NumDescriptors = 1;
		cbvRangeVS.BaseShaderRegister = 0;
		cbvRangeVS.RegisterSpace = 0;
		cbvRangeVS.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		//Describe the range of CBVs needed for pixel shader
		D3D12_DESCRIPTOR_RANGE cbvRangePS = {};
		cbvRangePS.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		cbvRangePS.NumDescriptors = 1;
		cbvRangePS.BaseShaderRegister = 0;
		cbvRangePS.RegisterSpace = 0;
		cbvRangePS.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		// Create a range of SRV's for textures
		D3D12_DESCRIPTOR_RANGE srvRange = {};
		srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange.NumDescriptors = 4;		// Set to max number of textures at once (match pixel shader!)
		srvRange.BaseShaderRegister = 0;	// Starts at s0 (match pixel shader!)
		srvRange.RegisterSpace = 0;
		srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		//// Create the root parameter <-- Depricated, we now use multiple rootParams.
		//D3D12_ROOT_PARAMETER rootParam = {};
		//rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		//rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		//rootParam.DescriptorTable.NumDescriptorRanges = 1;
		//rootParam.DescriptorTable.pDescriptorRanges = &cbvTable;

		D3D12_ROOT_PARAMETER rootParams[3] = {};

		//Definitions same as original one, just now for each piece (PS, VS, SRVs)

		rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[0].DescriptorTable.pDescriptorRanges = &cbvRangeVS;

		rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[1].DescriptorTable.pDescriptorRanges = &cbvRangePS;

		rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[2].DescriptorTable.pDescriptorRanges = &srvRange;

		//// Describe and serialize the root signature
		//D3D12_ROOT_SIGNATURE_DESC rootSig = {};
		//rootSig.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		//rootSig.NumParameters = 1;
		//rootSig.pParameters = &rootParam;
		//rootSig.NumStaticSamplers = 0;
		//rootSig.pStaticSamplers = 0;

		//Create static sampler, only works for this exercise
		//Eventually materials will have their own samplers.
		D3D12_STATIC_SAMPLER_DESC anisoWrap = {};
		anisoWrap.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		anisoWrap.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		anisoWrap.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		anisoWrap.Filter = D3D12_FILTER_ANISOTROPIC;
		anisoWrap.MaxAnisotropy = 16;
		anisoWrap.MaxLOD = D3D12_FLOAT32_MAX;
		anisoWrap.ShaderRegister = 0; //register (0)
		anisoWrap.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_STATIC_SAMPLER_DESC samplers[] = { anisoWrap };

		// Describe and serialize the root signature
		D3D12_ROOT_SIGNATURE_DESC rootSig = {};
		rootSig.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootSig.NumParameters = ARRAYSIZE(rootParams);
		rootSig.pParameters = rootParams;
		rootSig.NumStaticSamplers = ARRAYSIZE(samplers);
		rootSig.pStaticSamplers = samplers;

		ID3DBlob* serializedRootSig = 0;
		ID3DBlob* errors = 0;

		D3D12SerializeRootSignature(
			&rootSig,
			D3D_ROOT_SIGNATURE_VERSION_1,
			&serializedRootSig,
			&errors);

		// Check for errors during serialization
		if (errors != 0)
		{
			OutputDebugString((char*)errors->GetBufferPointer());
		}

		// Actually create the root sig
		device->CreateRootSignature(
			0,
			serializedRootSig->GetBufferPointer(),
			serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(rootSignature.GetAddressOf()));
	}

	// Pipeline state
	{
		// Describe the pipeline state
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

		// -- Input assembler related ---
		psoDesc.InputLayout.NumElements = inputElementCount;
		psoDesc.InputLayout.pInputElementDescs = inputElements;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		// Root sig
		psoDesc.pRootSignature = rootSignature.Get();

		// -- Shaders (VS/PS) --- 
		psoDesc.VS.pShaderBytecode = vertexShaderByteCode->GetBufferPointer();
		psoDesc.VS.BytecodeLength = vertexShaderByteCode->GetBufferSize();
		psoDesc.PS.pShaderBytecode = pixelShaderByteCode->GetBufferPointer();
		psoDesc.PS.BytecodeLength = pixelShaderByteCode->GetBufferSize();

		// -- Render targets ---
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;

		// -- States ---
		psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
		psoDesc.RasterizerState.DepthClipEnable = true;

		psoDesc.DepthStencilState.DepthEnable = true;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

		psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
		psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		// -- Misc ---
		psoDesc.SampleMask = 0xffffffff;

		// Create the pipe state object
		device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(pipelineState.GetAddressOf()));
	}
}


// --------------------------------------------------------
// Creates the geometry we're going to draw - a single triangle for now
// --------------------------------------------------------
void Game::CreateBasicGeometry()
{
	//Macro for texture loading
#define LoadTexture(x) DX12Helper::GetInstance().LoadTexture(GetFullPathTo_Wide(x).c_str())

	//Load Texture(s)
	D3D12_CPU_DESCRIPTOR_HANDLE bronzeAlbedo = LoadTexture(L"../../Assets/Textures/bronze_albedo.png");
	D3D12_CPU_DESCRIPTOR_HANDLE bronzeNormal = LoadTexture(L"../../Assets/Textures/bronze_normals.png");
	D3D12_CPU_DESCRIPTOR_HANDLE bronzeRoughness = LoadTexture(L"../../Assets/Textures/bronze_roughness.png");
	D3D12_CPU_DESCRIPTOR_HANDLE bronzeMetal = LoadTexture(L"../../Assets/Textures/bronze_metal.png");

	//Create material(s)
	//Samplers are a single static one in root sampler
	//Not per material yet.
	std::shared_ptr<Material> bronze = std::make_shared<Material>(pipelineState, XMFLOAT3(1, 1, 1));
	bronze->AddTexture(bronzeAlbedo, 0);
	bronze->AddTexture(bronzeNormal, 1);
	bronze->AddTexture(bronzeRoughness, 2);
	bronze->AddTexture(bronzeMetal, 3);
	bronze->FinalizeTextures();

	std::shared_ptr<Mesh> cube = std::make_shared<Mesh>(GetFullPathTo("../../Assets/Models/cube.obj").c_str());
	std::shared_ptr<Entity> entity = std::make_shared<Entity>(cube, bronze);
	entity.get()->GetTransform()->Scale(2, 2, 2);
	entity.get()->GetTransform()->SetPosition(0, 0, 5);
	entities.push_back(entity);
}

void Game::GenerateLights()
{
	// Reset
	lights.clear();

	// Setup directional lights
	Light dir1 = {};
	dir1.Type = LIGHT_TYPE_DIRECTIONAL;
	dir1.Direction = XMFLOAT3(1, -1, 1);
	dir1.Color = XMFLOAT3(0.8f, 0.8f, 0.8f);
	dir1.Intensity = 1.0f;

	// Add light to the list
	lights.push_back(dir1);

	// Create the rest of the lights
	while (lights.size() < MAX_LIGHTS)
	{
		Light point = {};
		point.Type = LIGHT_TYPE_POINT;
		point.Position = XMFLOAT3(RandomRange(-15.0f, 15.0f), RandomRange(-2.0f, 5.0f), RandomRange(-15.0f, 15.0f));
		point.Color = XMFLOAT3(RandomRange(0, 1), RandomRange(0, 1), RandomRange(0, 1));
		point.Range = RandomRange(5.0f, 10.0f);
		point.Intensity = RandomRange(0.1f, 3.0f);

		// Add to the list
		lights.push_back(point);
	}

	// Make sure we're exactly MAX_LIGHTS big
	lights.resize(MAX_LIGHTS);
}

// --------------------------------------------------------
// Handle resizing DirectX "stuff" to match the new window size.
// For instance, updating our projection matrix's aspect ratio.
// --------------------------------------------------------
void Game::OnResize()
{
	// Handle base-level DX resize stuff
	DXCore::OnResize();

	//// Update the camera's projection to match the new size
	//if (camera)
	//{
	//	camera->UpdateProjectionMatrix((float)width / height);
	//}
}

// --------------------------------------------------------
// Update your game here - user input, move objects, AI, etc.
// --------------------------------------------------------
void Game::Update(float deltaTime, float totalTime)
{
	// Example input checking: Quit if the escape key is pressed
	if (Input::GetInstance().KeyDown(VK_ESCAPE))
		Quit();

	// Spin entities
	for (auto& e : entities)
	{
		e->GetTransform()->Rotate(0, deltaTime * 0.5f, 0);
	}

	// Other updates
	camera->Update(deltaTime, hWnd);
}

// --------------------------------------------------------
// Clear the screen, redraw everything, present to the user
// --------------------------------------------------------
void Game::Draw(float deltaTime, float totalTime)
{
	DX12Helper& dx12Helper = DX12Helper::GetInstance();

	// Grab the current back buffer for this frame
	Microsoft::WRL::ComPtr<ID3D12Resource> currentBackBuffer = backBuffers[currentSwapBuffer];
	// Clearing the render target
	{
		// Transition the back buffer from present to render target
		D3D12_RESOURCE_BARRIER rb = {};
		rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		rb.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		rb.Transition.pResource = currentBackBuffer.Get();
		rb.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		rb.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList->ResourceBarrier(1, &rb);
		// Background color (Cornflower Blue in this case) for clearing
		float color[] = { 0.0f, 0.0f, 0.0f, 1.0f }; //Remember to set this to black once we have moved on past the first step.
		// Clear the RTV
		commandList->ClearRenderTargetView(
			rtvHandles[currentSwapBuffer],
			color,
			0, 0); // No scissor rectangles
			// Clear the depth buffer, too
		commandList->ClearDepthStencilView(
			dsvHandle,
			D3D12_CLEAR_FLAG_DEPTH,
			1.0f, // Max depth = 1.0f
			0, // Not clearing stencil, but need a value
			0, 0); // No scissor rects
	}

	// Rendering here!
	{

		// Set overall pipeline state
		//commandList->SetPipelineState(pipelineState.Get());

		// Root sig (must happen before root descriptor table)
		commandList->SetGraphicsRootSignature(rootSignature.Get());

		// Set constant buffer
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap = dx12Helper.GetConstantBufferDescriptorHeap();
		commandList->SetDescriptorHeaps(1, descriptorHeap.GetAddressOf());

		// Set up other commands for rendering
		commandList->OMSetRenderTargets(1, &rtvHandles[currentSwapBuffer], true, &dsvHandle);
		commandList->RSSetViewports(1, &viewport);
		commandList->RSSetScissorRects(1, &scissorRect);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Loop through the entities
		for (auto& e : entities)
		{
			//Grab material 
			//(should we consider sorting entities by marterial 
			//and then drawing groups of entities by material?)
			std::shared_ptr<Material> mat = e->GetMaterial();

			//Setup pipeline
			commandList->SetPipelineState(mat->GetPipelineState().Get());

			//Setup Vertexshader
			{
				// Set up the data we intend to use for drawing this entity
				VertexShaderExternalData vsData = {};
				vsData.world = e->GetTransform()->GetWorldMatrix();
				vsData.worldInverseTranspose = e->GetTransform()->GetWorldITMatrix();
				vsData.view = camera->GetViewMatrix();
				vsData.projection = camera->GetProjectionMatrix();

				// Send this to a chunk of the constant buffer heap
				// and grab the GPU handle for it so we can set it for this draw
				D3D12_GPU_DESCRIPTOR_HANDLE cbHandle = dx12Helper.FillNextConstantBufferAndGetGPUDescriptorHandle(
					(void*)(&vsData), sizeof(VertexShaderExternalData));

				// Set this constant buffer handle
				// Note: This assumes that descriptor table 0 is the
				//       place to put this particular descriptor.  This
				//       is based on how we set up our root signature.
				commandList->SetGraphicsRootDescriptorTable(0, cbHandle);
			}
			
			//Now Pixelshader and cbuffer
			{
				PixelShaderExternalData psData = {};
				psData.uvScale = mat->GetUVScale();
				psData.uvOffset = mat->GetUVOffset();
				psData.cameraPosition = camera->GetPosition();
				psData.lightCount = lightCount;
				memcpy(psData.lights, &lights[0], sizeof(Light) * MAX_LIGHTS);

				//Now send it to cbuffer memory (heap) and grab its handle
				D3D12_GPU_DESCRIPTOR_HANDLE cbHandlePS = dx12Helper.FillNextConstantBufferAndGetGPUDescriptorHandle(
					(void*)(&psData), sizeof(PixelShaderExternalData));

				//Set constant buffer handle
				//Assumes, based on our current root signature setup (the Textures/Materials exercise)
				//that descriptor table 1 is where this cbuffer goes, this value could change based on our setup.
				commandList->SetGraphicsRootDescriptorTable(2, mat->GetFinalGPUHandleForTextures());
			}

			//Set descriptor handle for this material's textures
			//Same thing as the constant buffer handle directly above.
			//This setup is based on our current implementation of rootSig
			commandList->SetGraphicsRootDescriptorTable(2, mat->GetFinalGPUHandleForTextures());

			// Grab the mesh and its buffer views
			std::shared_ptr<Mesh> mesh = e->GetMesh();
			D3D12_VERTEX_BUFFER_VIEW vbv = mesh->GetVB();
			D3D12_INDEX_BUFFER_VIEW  ibv = mesh->GetIB();

			// Set the geometry
			commandList->IASetVertexBuffers(0, 1, &vbv);
			commandList->IASetIndexBuffer(&ibv);

			// Draw
			commandList->DrawIndexedInstanced(mesh->GetIndexCount(), 1, 0, 0, 0);
		}
	}
	// Present
	{
		// Transition back to present
		D3D12_RESOURCE_BARRIER rb = {};
		rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		rb.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		rb.Transition.pResource = currentBackBuffer.Get();
		rb.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		rb.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList->ResourceBarrier(1, &rb);

		// Must occur BEFORE present

		dx12Helper.CloseExecuteAndResetCommandList();

		// Present the current back buffer
		swapChain->Present(vsync ? 1 : 0, 0); //Vsync on or off? Simple computation

		// Figure out which buffer is next
		currentSwapBuffer++;
		if (currentSwapBuffer >= numBackBuffers)
			currentSwapBuffer = 0;
	}
}