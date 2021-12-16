#pragma once

#include "DXCore.h"
#include "Mesh.h"
#include "Entity.h"
#include "Transform.h"
#include "Camera.h"
#include "Lights.h"

#include <DirectXMath.h>
#include <wrl/client.h> // Used for ComPtr - a smart pointer for COM objects
#include <vector>
#include <memory>

class Game 
	: public DXCore
{

public:
	Game(HINSTANCE hInstance);
	~Game();

	// Overridden setup and game loop methods, which
	// will be called automatically
	void Init();
	void OnResize();
	void Update(float deltaTime, float totalTime);
	void Draw(float deltaTime, float totalTime);

private:
	
	// Initialization helper methods - feel free to customize, combine, etc.
	// Should we use vsync to limit the frame rate?
	bool vsync;
	void CreateRootSigAndPipelineState();
	void CreateBasicGeometry();
	void GenerateLights();
	//void LoadShaders(); <--Depricated from DX11
	

	// Note the usage of ComPtr below
	//  - This is a smart pointer for objects that abide by the
	//    Component Object Model, which DirectX objects do
	//  - More info here: https://github.com/Microsoft/DirectXTK/wiki/ComPtr

	//// Buffers to hold actual geometry data
	//Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
	//Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;

	//D3D12_VERTEX_BUFFER_VIEW vbView;
	//D3D12_INDEX_BUFFER_VIEW ibView;
	
	// Shaders and shader-related constructs now located in a PipelineState
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	
	// Scene
	int lightCount;
	std::vector<Light> lights;
	std::shared_ptr<Camera> camera;
	std::vector<std::shared_ptr<Entity>> entities;
};

