//Kyle Zielinski
//3/02/2021
//Class to make an Entity object that holds a mesh, a material and a transform.
#include "Entity.h"

//Entity::Entity(Mesh* newMesh, Material* newMaterial, EntityDef newEntity, GraphicData graphicData)
//{
//	mesh = std::make_shared<Mesh>(*newMesh);
//	material = std::make_shared<Material>(*newMaterial);
//	transform = Transform();
//	myDef = newEntity;
//	currentGraphicDef = graphicData;
//}

Entity::Entity(std::shared_ptr<Mesh> newMesh, std::shared_ptr<Material> newMaterial) : mesh(newMesh), material(newMaterial)
{
}

Entity::~Entity()
{
}

std::shared_ptr<Mesh> Entity::GetMesh()
{
	return mesh;
}

std::shared_ptr<Material> Entity::GetMaterial()
{
	return material;
}

void Entity::AssignMaterial(std::shared_ptr<Material> newMaterial)
{
	material = newMaterial;
}

void Entity::AssignMesh(std::shared_ptr<Mesh> newMesh)
{
	mesh = newMesh;
}

//void Entity::DrawEntity(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, Camera* cam)
//{
//	material->GetVertexShader()->SetShader();
//	material->GetPixelShader()->SetShader();
//
//	material->GetPixelShader()->SetShaderResourceView("albedoTexture", material->BaseTexture());
//	if (material->NormalTexture() != nullptr)
//	{
//		material->GetPixelShader()->SetShaderResourceView("normalMapTexture", material->NormalTexture());
//	}
//	material->GetPixelShader()->SetShaderResourceView("metalMapTexture", material->MetalTexture()); 
//	material->GetPixelShader()->SetShaderResourceView("roughMapTexture", material->RoughTexture());
//
//	material->GetPixelShader()->SetSamplerState("basicSampler", material->GetSampleState());
//	material->GetPixelShader()->SetSamplerState("clampSampler", material->GetClampSampleState());
//
//
//	SimpleVertexShader* vs = material->GetVertexShader();
//	vs->SetFloat4("colorTint", material->GetColorTint());
//	vs->SetMatrix4x4("world", transform.GetWorldMatrix());
//	vs->SetMatrix4x4("view", cam->GetViewMatrix());
//	vs->SetMatrix4x4("projection", cam->GetProjectionMatrix());
//
//	// This should be sending data to the gpu
//	vs->CopyAllBufferData();
//
//	// Draw the mesh
//	mesh->DrawUsingBuffs(context);
//}
//
//void Entity::SetDataStruct(EntityDef inputStruct)
//{
//	myDef = inputStruct;
//}
//
//EntityDef Entity::GetDataStruct()
//{
//	return myDef;
//}
//
//GraphicData Entity::GetGraphicDataStruct()
//{
//	return currentGraphicDef;
//}
//
//EntityPosition Entity::GetPositionDataStruct()
//{
//	return myPosition;
//}
//
//void Entity::SetPositionDataStruct(EntityPosition value)
//{
//	myPosition.X = value.X;
//	myPosition.Y = value.Y;
//	myPosition.Z = value.Z;
//
//	transform.SetPosition(value.X, value.Y, value.Z);
//}
//
//int Entity::GetIndex()
//{
//	return myDef.index;
//}
//
//std::shared_ptr<Material> Entity::GetMaterial()
//{
//	return material;
//}
//
//void Entity::AssignMaterial(std::shared_ptr<Material> newMaterial)
//{
//	material.reset();
//	material = newMaterial;
//}

//void Entity::AssignMesh(std::shared_ptr<Mesh> newMesh)
//{
//	mesh.reset();
//	mesh = newMesh;
//}

Transform* Entity::GetTransform()
{
	return &transform;
}
