#include "LightingClean.hlsli"

cbuffer ExternalData : register(b0)
{
	matrix world;
	matrix worldInverseTranspose;
	matrix view;
	matrix projection;
}

// Struct representing a single vertex worth of data
struct VertexShaderInput
{
	float3 localPosition	: POSITION;
	float2 uv				: TEXCOORD;
	float3 normal			: NORMAL;
	float3 tangent			: TANGENT;
};

// Struct representing the data we're sending down the pipeline
struct VertexToPixel
{
	float4 screenPosition	: SV_POSITION;
	float2 uv				: TEXCOORD;
	float3 normal			: NORMAL;
	float3 tangent			: TANGENT;
	float3 worldPos			: POSITION;
};

VertexToPixel main( VertexShaderInput input )
{
	// Set up output struct
	VertexToPixel output;

	matrix wvp = mul(projection, mul(view, world));
	output.screenPosition = mul(wvp, float4(input.localPosition, 1.0f));

	//World space lighting vectors
	output.normal = normalize(mul((float3x3)worldInverseTranspose, input.normal));
	output.tangent = normalize(mul((float3x3)worldInverseTranspose, input.tangent));

	// Calc vertex world pos
	output.worldPos = mul(world, float4(input.localPosition, 1.0f)).xyz;

	// Pass through the uv
	output.uv = input.uv;

	return output;
}