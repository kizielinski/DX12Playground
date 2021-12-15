//Kyle Zielinski
//3/02/2021
//Class implementation of a camera object that the user can operate to navigate around the scene.
#include "Camera.h"

using namespace DirectX;
Camera::Camera(float x, float y, float z, float aspectRatio)
{
	transform.SetPosition(x, y, z);

	UpdateViewMatrix();
	UpdateProjectionMatrix(aspectRatio);
}

Camera::~Camera()
{
}


void Camera::Update(float deltaTime, HWND windowHandle)
{
	//TODO: Move this into an input class
	//if (!ImGui::GetIO().WantCaptureMouse)
	{
		//speed
		float speed = deltaTime * 2.0f;

		//Check for user key press
		if (GetAsyncKeyState('W') & 0x8000) { transform.MoveRelative(0, 0, speed); }
		if (GetAsyncKeyState('S') & 0x8000) { transform.MoveRelative(0, 0, -speed); }
		if (GetAsyncKeyState('A') & 0x8000) { transform.MoveRelative(-speed, 0, 0); }
		if (GetAsyncKeyState('D') & 0x8000) { transform.MoveRelative(speed, 0, 0); }

		//Get current Mouse position
		POINT mousePos = {};
		GetCursorPos(&mousePos);
		ScreenToClient(windowHandle, &mousePos);

		if (GetAsyncKeyState(VK_LBUTTON) & 0x8000)
		{
			//Calculate change in cursor pos
			float xDifference = deltaTime * (mousePos.x - prevMousePosition.x);
			float yDifference = deltaTime * (mousePos.y - prevMousePosition.y);

			transform.Rotate(yDifference, xDifference, 0); //Yes in that order :(
		}

		UpdateViewMatrix();
		prevMousePosition = mousePos;
	}
}

void Camera::UpdateViewMatrix()
{
	//Get rotation value
	XMFLOAT3 pitchYawRollValues = transform.GetRotation();
	XMVECTOR forwardDirection = XMVector3Rotate(
		XMVectorSet(0, 0, 1, 0),
		XMQuaternionRotationRollPitchYawFromVector(XMLoadFloat3(&pitchYawRollValues)));

	//Look to is looking in a direction
	//Look at matarix means look at specific location
	XMFLOAT3 position = transform.GetPosition();
	XMMATRIX view = XMMatrixLookToLH(
		XMLoadFloat3(&position),
		forwardDirection,
		XMVectorSet(0, 1, 0, 0));

	XMStoreFloat4x4(&viewMatrix, view);
}

void Camera::UpdateProjectionMatrix(float aspectRatio)
{
	XMMATRIX proj = XMMatrixPerspectiveFovLH(
		XM_PIDIV4, //RADIAN VALUE
		aspectRatio,
		0.01f, //Small but never zero, that way view doesn't disappear. Near clip
		100.0f); //Large but not massive (1k is general limit); Far clip

	XMStoreFloat4x4(&projectionMatrix, proj);
}

void Camera::CenterCamera()
{
}

DirectX::XMFLOAT4X4 Camera::GetViewMatrix()
{
	return viewMatrix;
}

DirectX::XMFLOAT4X4 Camera::GetProjectionMatrix()
{
	return projectionMatrix;
}

DirectX::XMFLOAT3 Camera::GetPosition()
{
	return transform.GetPosition();
}
