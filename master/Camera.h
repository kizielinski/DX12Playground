//Kyle Zielinski
//3/02/2021
//Class header for a material object that holds data regaring colortint, pixelshaders, and vertexshaders.
#pragma once

#include "Transform.h"
//#include "imgGUI/imgui.h"
#include <DirectXMath.h>
#include <Windows.h>

class Camera
{

public:
	Camera(float x, float y, float z, float aspectRatio);
	~Camera();

	//Update functions
	void Update(float deltaTime, HWND windowHandle);
	void UpdateViewMatrix();
	void UpdateProjectionMatrix(float aspectRatio);
	void CenterCamera(); //Needs parameters of entities;

	//Getters
	DirectX::XMFLOAT4X4 GetViewMatrix();
	DirectX::XMFLOAT4X4 GetProjectionMatrix();
	DirectX::XMFLOAT3 GetPosition();

private:
	DirectX::XMFLOAT4X4 viewMatrix;
	DirectX::XMFLOAT4X4 projectionMatrix;
	Transform transform;
	POINT prevMousePosition;
};

