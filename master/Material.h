#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <memory>
#include <unordered_map>

#include "Camera.h"
#include "Transform.h"

class Material
{

public: 

	Material(Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState,
		DirectX::XMFLOAT3 tint,
		DirectX::XMFLOAT2 uvScale = DirectX::XMFLOAT2(1, 1),
		DirectX::XMFLOAT2 uvOffset = DirectX::XMFLOAT2(0, 0));
	~Material();

	Microsoft::WRL::ComPtr<ID3D12PipelineState> GetPipelineState();
	DirectX::XMFLOAT2 GetUVScale();
	DirectX::XMFLOAT2 GetUVOffset();
	DirectX::XMFLOAT3 GetColorTint();
	D3D12_GPU_DESCRIPTOR_HANDLE GetFinalGPUHandleForTextures();

	void SetPipelineState(Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState);
	void SetUVScale(DirectX::XMFLOAT2 scale);
	void SetUVOffset(DirectX::XMFLOAT2 offset);
	void SetColorTint(DirectX::XMFLOAT3 tint);

	void AddTexture(D3D12_CPU_DESCRIPTOR_HANDLE srvDescriptorHandle, int slot);
	void FinalizeTextures();

private:

	//Shared among materials, includes shaders.
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;

	//Properties of our material
	DirectX::XMFLOAT3 colorTint;
	DirectX::XMFLOAT2 uvOffset;
	DirectX::XMFLOAT2 uvScale;

	//Tracking for textures on GPU
	bool materialTexturesFinalized;
	int highestSRVSlot;
	D3D12_CPU_DESCRIPTOR_HANDLE textureSRVsBySlot[128]; //Up to 128 textures can be bound here, we won't reach that amount
	D3D12_GPU_DESCRIPTOR_HANDLE finalGPUHandleForSRVs;
};

