//Kyle Zielinski
//3/02/2021
//Class header for an entity object that holds data for drawing a mesh with a material
#pragma once

#include <wrl/client.h>
#include <DirectXMath.h>
#include "Mesh.h"
#include "Material.h"
#include "Transform.h"
#include "Camera.h"
//#include "DataStruct.h"
#include <memory>

class Entity
{
public:
	//Entity(Mesh* newMesh, Material* newMaterial, EntityDef newEntity, GraphicData graphicData);
	Entity(std::shared_ptr<Mesh> newMesh, std::shared_ptr<Material> newMaterial);
	~Entity();
	std::shared_ptr<Mesh> GetMesh();
	std::shared_ptr<Material> GetMaterial();
	void AssignMaterial(std::shared_ptr<Material> newMaterial);
	void AssignMesh(std::shared_ptr<Mesh> newMesh);
	Transform* GetTransform();
	//void DrawEntity(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, Camera* cam);
	//void SetDataStruct(EntityDef inputStruct);
	//EntityDef GetDataStruct();
	//GraphicData GetGraphicDataStruct();
	//EntityPosition GetPositionDataStruct();
	//void SetPositionDataStruct(EntityPosition newPosition);
	//int GetIndex();

private:
	Transform transform;
	std::shared_ptr<Mesh> mesh;
	std::shared_ptr<Material> material;
	//EntityDef myDef;
	//GraphicData currentGraphicDef;
	//EntityPosition myPosition;
};

