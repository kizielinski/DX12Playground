#include "Material.h"
#include "DX12Helper.h"

Material::Material(Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState,
    DirectX::XMFLOAT3 tint,
    DirectX::XMFLOAT2 uvScale,
    DirectX::XMFLOAT2 uvOffset) :
    pipelineState(pipelineState),
    colorTint(tint),
    uvScale(uvScale),
    uvOffset(uvOffset),
    materialTexturesFinalized(false),
    highestSRVSlot(-1)
{
    //Init remaining pices of data
    finalGPUHandleForSRVs = {}; //Empty Value for now
    ZeroMemory(textureSRVsBySlot, sizeof(D3D12_CPU_DESCRIPTOR_HANDLE) * 128);
}

Material::~Material()
{
}

Microsoft::WRL::ComPtr<ID3D12PipelineState> Material::GetPipelineState()
{ return pipelineState; }

DirectX::XMFLOAT2 Material::GetUVScale()
{ return uvScale; }

DirectX::XMFLOAT2 Material::GetUVOffset()
{ return uvOffset; }

DirectX::XMFLOAT3 Material::GetColorTint()
{ return colorTint; }

D3D12_GPU_DESCRIPTOR_HANDLE Material::GetFinalGPUHandleForTextures()
{ return finalGPUHandleForSRVs; }

void Material::SetPipelineState(Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState)
{
    this->pipelineState = pipelineState;
}

void Material::SetUVScale(DirectX::XMFLOAT2 scale)
{
    this->uvScale = scale;
}

void Material::SetUVOffset(DirectX::XMFLOAT2 offset)
{
    this->uvOffset = offset;
}

void Material::SetColorTint(DirectX::XMFLOAT3 tint)
{
    this->colorTint = tint;
}

void Material::AddTexture(D3D12_CPU_DESCRIPTOR_HANDLE srvDescriptorHandle, int slot)
{
    //Return out if there is no valid slot to save texture
    if(materialTexturesFinalized || slot < 0 || slot >= 128)
    { return; }

    //Otherwise save texture nad check for highest slot
    textureSRVsBySlot[slot] = srvDescriptorHandle;
    highestSRVSlot = max(highestSRVSlot, slot);
}

void Material::FinalizeTextures()
{
    //If done already then return
    if(materialTexturesFinalized)
    { return; }

    DX12Helper& dx12Helper = DX12Helper::GetInstance();

    //All SRVs are coped into one singular CBV/SRV heap.
    //Need to keep the first texture's GPU handle so we
    //have a pointer to the beginning of SRV range for this (specific?) material
    for (int i = 0; i <= highestSRVSlot; i++)
    {
        //Copy one SRV at a time
        //Separate heaps now combine into one
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle =
            dx12Helper.CopySRVsToDescriptorHeapAndGetGPUDescriptorHandle(textureSRVsBySlot[i], 1);

            //Track first handle
        if (i == 0) 
        { finalGPUHandleForSRVs = gpuHandle; }
    }

    //Asset texture setup has been finished
    materialTexturesFinalized = true;
}
