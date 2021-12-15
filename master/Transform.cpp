#include "Transform.h"

using namespace DirectX; //Because this is .cpp this using namespace won't propogate to rest of code

Transform::Transform()
{
	position = XMFLOAT3(0, 0, 0);
	pitchYawRoll = XMFLOAT3(0, 0, 0);
	scale = XMFLOAT3(1, 1, 1);

	parent = nullptr;

	XMStoreFloat4x4(&worldMatrix, XMMatrixIdentity());
	XMStoreFloat4x4(&worldInverseTransposeMatrix, XMMatrixIdentity());
	isDirty = false;
}

Transform::~Transform()
{
	for (int i = 0; i < children.size(); i++)
	{
		children[i] = nullptr;
	}
	parent = nullptr;
}

void Transform::MoveAbsolute(float x, float y, float z)
{
	XMVECTOR newPos = XMVectorAdd(XMLoadFloat3(&position), XMVectorSet(x, y, z, 0));
	XMStoreFloat3(&position, newPos);
	MarkChildTransformDirty();
	isDirty = true;
	//simple version - this is the same as above, but the above code becomes optimized by compiler
	/*
	* position.x += x;
	* position.y += y;
	* position.z += z;
	*/
}

void Transform::MoveRelative(float x, float y, float z)
{
	//Overall, this will adjust position, not through adding xyz to current values
	//but by taking into account how the need to be rotated to match our rotation. 

	// Desired movement in "world" space
	XMVECTOR desiredMovement = XMVectorSet(x, y, z, 0);
	
	//Pitch yaw roll as our current rotation in quaternion form.
	XMVECTOR rotationQuaternion = XMQuaternionRotationRollPitchYawFromVector(XMLoadFloat3(&pitchYawRoll));

	//Rotate desiredMovement by the same amount we are rotated.
	XMVECTOR relativeMovement = XMVector3Rotate(
		desiredMovement,
		rotationQuaternion);

	//Update our position
	//aka pos = pos + relativeMove;
	XMStoreFloat3(&position, XMLoadFloat3(&position) + relativeMovement);

	//Call update to matrix
	MarkChildTransformDirty();
	isDirty = true;
}

void Transform::Rotate(float pitch, float yaw, float roll)
{ 
	XMVECTOR newRotation = XMVectorAdd(XMLoadFloat3(&pitchYawRoll), XMVectorSet(pitch, yaw, roll, 0));
	XMStoreFloat3(&pitchYawRoll, newRotation);
	MarkChildTransformDirty();
	isDirty = true;
}

void Transform::Scale(float x, float y, float z)
{
	XMVECTOR newScale = XMVectorMultiply(XMLoadFloat3(&scale), XMVectorSet(x, y, z, 0));
	XMStoreFloat3(&scale, newScale);
	MarkChildTransformDirty();
	isDirty = true;
}

void Transform::SetPosition(float x, float y, float z)
{
	XMStoreFloat3(&position, XMVectorSet(x, y, z, 0));
	MarkChildTransformDirty();
	isDirty = true;
}

void Transform::SetRotation(float pitch, float yaw, float roll)
{
	XMStoreFloat3(&pitchYawRoll, XMVectorSet(pitch, yaw, roll, 0));
	isDirty = true;
}

void Transform::SetScale(float x, float y, float z)
{
	XMStoreFloat3(&scale, XMVectorSet(x, y, z, 0));
	isDirty = true;
}

DirectX::XMFLOAT3 Transform::GetPosition()
{
	return position;
}

DirectX::XMFLOAT3 Transform::GetRotation()
{
	return pitchYawRoll;
}

DirectX::XMFLOAT3 Transform::GetScale()
{
	return scale;
}

DirectX::XMFLOAT4X4 Transform::GetWorldMatrix()
{
	if (isDirty)
	{
		XMMATRIX translationMat = XMMatrixTranslationFromVector(XMLoadFloat3(&position));
		XMMATRIX rotationMat = XMMatrixRotationRollPitchYawFromVector(XMLoadFloat3(&pitchYawRoll));
		XMMATRIX scaleMat = XMMatrixScalingFromVector(XMLoadFloat3(&scale));

		//Store world matrix & store it once you invert and transpose
		XMMATRIX worldMat = scaleMat * rotationMat * translationMat;
		if(parent)
		{
			XMMATRIX parentMatrix = XMLoadFloat4x4(&GetParent()->worldMatrix);
			worldMat =  XMMatrixMultiply(worldMat, parentMatrix);		
		}
		XMStoreFloat4x4(&worldMatrix, worldMat);
		XMStoreFloat4x4(&worldInverseTransposeMatrix, XMMatrixInverse(0, XMMatrixTranspose(worldMat)));

		isDirty = false;
	}

	return worldMatrix;
}

DirectX::XMFLOAT4X4 Transform::GetWorldITMatrix()
{
	return worldInverseTransposeMatrix;
}

void Transform::AddChild(Transform* child)
{
	if (child != nullptr)
	{
		bool duplicateLock = false;
		for(int i = 0; i < children.size(); i++)
		{
			if (&children[i] == &child)
			{
				duplicateLock = true;
				break;
			}
		}
		
		if (!duplicateLock)
		{
			child->parent = this;
			child->isDirty = true;
			child->MarkChildTransformDirty();
			children.push_back(child);
		}
	}
}

void Transform::RemoveChild(Transform* child)
{
	if (child != nullptr)
	{
		bool removeLock = false;
		int indexToRemove;
		for (int i = 0; i < children.size(); i++)
		{
			if (children[i] == child)
			{
				removeLock = true;
				indexToRemove = i;
				break;
			}
		}

		if (removeLock)
		{
			children[indexToRemove]->parent = nullptr;
			children[indexToRemove]->isDirty = true;
			children[indexToRemove]->MarkChildTransformDirty();
			children.erase(children.begin() + indexToRemove);
		}
	}
}

void Transform::SetChild(Transform* newParent)
{
	this->parent = newParent;

	if (newParent != nullptr)
	{
		newParent->AddChild(this);
		this->isDirty = true;
		this->MarkChildTransformDirty();
	}
}

Transform* Transform::GetParent()
{
	return parent;
}

Transform* Transform::GetChild(unsigned int index)
{
	if (index < 0 || index >= children.size())
	{
		return nullptr;
	}
	else 
	{
		return children[index];
	}
}

int Transform::IndexOfChild(Transform* child)
{
	int indexToReturn = -1;
	for (int i = 0; i < children.size(); i++)
	{
		if (&children[i] == &child)
		{
			indexToReturn = i;
			break;
		}
	}
	
	return indexToReturn;
}

unsigned int Transform::GetChildCount()
{
	return children.size();
}

void Transform::MarkChildTransformDirty()
{
	for (int i = 0; i < children.size(); i++)
	{
		children[i]->isDirty = true;
		children[i]->MarkChildTransformDirty();
	}
}
