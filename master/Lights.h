#pragma once

#include <DirectXMath.h>

//Match these definitions with those in the shaders.
#define MAX_LIGHTS 10
#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT 1
//#define LIGHT_TYPE_SPOT 2

//Remember the 16 byte rule? Oh yeah it's back!
struct Light
{
	int Type;
	DirectX::XMFLOAT3 Direction;
	float Range;
	DirectX::XMFLOAT3 Position;
	float Intensity;
	DirectX::XMFLOAT3 Color;
	float SpotFalloff;
	DirectX::XMFLOAT3 Padding;
};