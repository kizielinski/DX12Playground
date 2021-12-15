#pragma once
#include <DirectXMath.h>
#include <vector>

class Transform
{
public:
	
	Transform();
	~Transform();

	// Methods to adjust existing transforms
	void MoveAbsolute(float x, float y, float z);
	void MoveRelative(float x, float y, float z);
	void Rotate(float pitch, float yaw, float roll);
	void Scale(float x, float y, float z);

	// Methods to overwrite existing transforms
	void SetPosition(float x, float y, float z);
	void SetRotation(float pitch, float yaw, float roll);
	void SetScale(float x, float y, float z);

	// Methods to retrieve transform data
	DirectX::XMFLOAT3 GetPosition();
	DirectX::XMFLOAT3 GetRotation();
	DirectX::XMFLOAT3 GetScale();
	
	// Method to return/calculate the resulting world matrix
	DirectX::XMFLOAT4X4 GetWorldMatrix();
	DirectX::XMFLOAT4X4 GetWorldITMatrix();
	// No reason to have a SET, the result will always be the total result of the 3 transformations.

	//Hierarchy Methods
	void AddChild(Transform* child);
	void RemoveChild(Transform* child);
	void SetChild(Transform* newParent);
	Transform* GetParent();
	Transform* GetChild(unsigned int index);
	int IndexOfChild(Transform* child);
	unsigned int GetChildCount();

private:

	void MarkChildTransformDirty();

	//A place to store the raw transform values
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT3 pitchYawRoll; // Not a quaternion, simply 3 Euler angles
	DirectX::XMFLOAT3 scale; 

	DirectX::XMFLOAT4X4 worldMatrix; // Most recent matrix created
	DirectX::XMFLOAT4X4 worldInverseTransposeMatrix; // Inverse&transpose of current worldmatrix
	bool isDirty; // Has the matrix value been changed? If so remake matrix

	Transform* parent; //Should be null(0) if there is no parent

	std::vector<Transform*> children;
};

