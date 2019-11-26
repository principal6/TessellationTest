#include "Object3D.h"
#include "Game.h"

using std::max;
using std::min;
using std::vector;
using std::string;
using std::to_string;
using std::make_unique;

void CObject3D::Create(const SMesh& Mesh)
{
	m_Model.vMeshes.clear();
	m_Model.vMeshes.emplace_back(Mesh);

	m_Model.vMaterialData.clear();
	m_Model.vMaterialData.emplace_back();

	CreateMeshBuffers();
	CreateMaterialTextures();

	m_bIsCreated = true;
}

void CObject3D::Create(const SMesh& Mesh, const CMaterialData& MaterialData)
{
	m_Model.vMeshes.clear();
	m_Model.vMeshes.emplace_back(Mesh);

	m_Model.vMaterialData.clear();
	m_Model.vMaterialData.emplace_back(MaterialData);
	
	CreateMeshBuffers();
	CreateMaterialTextures();

	m_bIsCreated = true;
}

void CObject3D::Create(const SModel& Model)
{
	m_Model = Model;

	CreateMeshBuffers();
	CreateMaterialTextures();

	m_bIsCreated = true;
}

void CObject3D::CreatePatches(size_t ControlPointCountPerPatch, size_t PatchCount)
{
	assert(ControlPointCountPerPatch > 0);
	assert(PatchCount > 0);

	m_ControlPointCountPerPatch = ControlPointCountPerPatch;
	m_PatchCount = PatchCount;
	m_bIsPatch = true;

	ShouldTessellate(true);

	m_bIsCreated = true;
}

void CObject3D::AddMaterial(const CMaterialData& MaterialData)
{
	m_Model.vMaterialData.emplace_back(MaterialData);
	m_Model.vMaterialData.back().Index(m_Model.vMaterialData.size() - 1);

	CreateMaterialTexture(m_Model.vMaterialData.size() - 1);
}

void CObject3D::SetMaterial(size_t Index, const CMaterialData& MaterialData)
{
	assert(Index < m_Model.vMaterialData.size());

	m_Model.vMaterialData[Index] = MaterialData;
	m_Model.vMaterialData[Index].Index(Index);

	CreateMaterialTexture(Index);
}

size_t CObject3D::GetMaterialCount() const
{
	return m_Model.vMaterialData.size();
}

void CObject3D::CreateMeshBuffers()
{
	m_vMeshBuffers.clear();
	m_vMeshBuffers.resize(m_Model.vMeshes.size());
	for (size_t iMesh = 0; iMesh < m_Model.vMeshes.size(); ++iMesh)
	{
		CreateMeshBuffer(iMesh);
	}
}

void CObject3D::CreateMeshBuffer(size_t MeshIndex)
{
	const SMesh& Mesh{ m_Model.vMeshes[MeshIndex] };

	{
		D3D11_BUFFER_DESC BufferDesc{};
		BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		BufferDesc.ByteWidth = static_cast<UINT>(sizeof(SVertex3D) * Mesh.vVertices.size());
		BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		BufferDesc.MiscFlags = 0;
		BufferDesc.StructureByteStride = 0;
		BufferDesc.Usage = D3D11_USAGE_DYNAMIC;

		D3D11_SUBRESOURCE_DATA SubresourceData{};
		SubresourceData.pSysMem = &Mesh.vVertices[0];
		m_PtrDevice->CreateBuffer(&BufferDesc, &SubresourceData, &m_vMeshBuffers[MeshIndex].VertexBuffer);
	}

	{
		D3D11_BUFFER_DESC BufferDesc{};
		BufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		BufferDesc.ByteWidth = static_cast<UINT>(sizeof(STriangle) * Mesh.vTriangles.size());
		BufferDesc.CPUAccessFlags = 0;
		BufferDesc.MiscFlags = 0;
		BufferDesc.StructureByteStride = 0;
		BufferDesc.Usage = D3D11_USAGE_DEFAULT;

		D3D11_SUBRESOURCE_DATA SubresourceData{};
		SubresourceData.pSysMem = &Mesh.vTriangles[0];
		m_PtrDevice->CreateBuffer(&BufferDesc, &SubresourceData, &m_vMeshBuffers[MeshIndex].IndexBuffer);
	}
}

void CObject3D::CreateMaterialTextures()
{
	m_vMaterialTextureSets.clear();
	
	for (CMaterialData& MaterialData : m_Model.vMaterialData)
	{
		// @important
		m_vMaterialTextureSets.emplace_back(make_unique<CMaterialTextureSet>(m_PtrDevice, m_PtrDeviceContext));

		if (MaterialData.HasAnyTexture())
		{
			m_vMaterialTextureSets.back()->CreateTextures(MaterialData);
		}
	}
}

void CObject3D::CreateMaterialTexture(size_t Index)
{
	if (Index == m_vMaterialTextureSets.size())
	{
		m_vMaterialTextureSets.emplace_back(make_unique<CMaterialTextureSet>(m_PtrDevice, m_PtrDeviceContext));
	}
	else
	{
		m_vMaterialTextureSets[Index] = make_unique<CMaterialTextureSet>(m_PtrDevice, m_PtrDeviceContext);
	}
	m_vMaterialTextureSets[Index]->CreateTextures(m_Model.vMaterialData[Index]);
}

void CObject3D::UpdateQuadUV(const XMFLOAT2& UVOffset, const XMFLOAT2& UVSize)
{
	float U0{ UVOffset.x };
	float V0{ UVOffset.y };
	float U1{ U0 + UVSize.x };
	float V1{ V0 + UVSize.y };

	m_Model.vMeshes[0].vVertices[0].TexCoord = XMVectorSet(U0, V0, 0, 0);
	m_Model.vMeshes[0].vVertices[1].TexCoord = XMVectorSet(U1, V0, 0, 0);
	m_Model.vMeshes[0].vVertices[2].TexCoord = XMVectorSet(U0, V1, 0, 0);
	m_Model.vMeshes[0].vVertices[3].TexCoord = XMVectorSet(U1, V1, 0, 0);

	UpdateMeshBuffer();
}

void CObject3D::UpdateMeshBuffer(size_t MeshIndex)
{
	D3D11_MAPPED_SUBRESOURCE MappedSubresource{};
	if (SUCCEEDED(m_PtrDeviceContext->Map(m_vMeshBuffers[MeshIndex].VertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedSubresource)))
	{
		memcpy(MappedSubresource.pData, &m_Model.vMeshes[MeshIndex].vVertices[0], sizeof(SVertex3D) * m_Model.vMeshes[MeshIndex].vVertices.size());

		m_PtrDeviceContext->Unmap(m_vMeshBuffers[MeshIndex].VertexBuffer.Get(), 0);
	}
}

void CObject3D::LimitFloatRotation(float& Value, const float Min, const float Max)
{
	if (Value > Max) Value = Min;
	if (Value < Min) Value = Max;
}

void CObject3D::UpdateWorldMatrix()
{
	LimitFloatRotation(ComponentTransform.Pitch, CGame::KRotationMinLimit, CGame::KRotationMaxLimit);
	LimitFloatRotation(ComponentTransform.Yaw, CGame::KRotationMinLimit, CGame::KRotationMaxLimit);
	LimitFloatRotation(ComponentTransform.Roll, CGame::KRotationMinLimit, CGame::KRotationMaxLimit);

	if (XMVectorGetX(ComponentTransform.Scaling) < CGame::KScalingMinLimit)
		ComponentTransform.Scaling = XMVectorSetX(ComponentTransform.Scaling, CGame::KScalingMinLimit);
	if (XMVectorGetY(ComponentTransform.Scaling) < CGame::KScalingMinLimit)
		ComponentTransform.Scaling = XMVectorSetY(ComponentTransform.Scaling, CGame::KScalingMinLimit);
	if (XMVectorGetZ(ComponentTransform.Scaling) < CGame::KScalingMinLimit)
		ComponentTransform.Scaling = XMVectorSetZ(ComponentTransform.Scaling, CGame::KScalingMinLimit);

	XMMATRIX Translation{ XMMatrixTranslationFromVector(ComponentTransform.Translation) };
	XMMATRIX Rotation{ XMMatrixRotationRollPitchYaw(ComponentTransform.Pitch,
		ComponentTransform.Yaw, ComponentTransform.Roll) };
	XMMATRIX Scaling{ XMMatrixScalingFromVector(ComponentTransform.Scaling) };

	// @important
	float ScalingX{ XMVectorGetX(ComponentTransform.Scaling) };
	float ScalingY{ XMVectorGetY(ComponentTransform.Scaling) };
	float ScalingZ{ XMVectorGetZ(ComponentTransform.Scaling) };
	float MaxScaling{ max(ScalingX, max(ScalingY, ScalingZ)) };
	ComponentPhysics.BoundingSphere.Radius = ComponentPhysics.BoundingSphere.RadiusBias * MaxScaling;

	XMMATRIX BoundingSphereTranslation{ XMMatrixTranslationFromVector(ComponentPhysics.BoundingSphere.CenterOffset) };
	XMMATRIX BoundingSphereTranslationOpposite{ XMMatrixTranslationFromVector(-ComponentPhysics.BoundingSphere.CenterOffset) };

	ComponentTransform.MatrixWorld = Scaling * BoundingSphereTranslationOpposite * Rotation * Translation * BoundingSphereTranslation;
}

void CObject3D::ShouldTessellate(bool Value)
{
	m_bShouldTesselate = Value;
}

void CObject3D::TessellationType(ETessellationType eType)
{
	m_eTessellationType = eType;
}

CObject3D::ETessellationType CObject3D::TessellationType() const
{
	return m_eTessellationType;
}

void CObject3D::SetTessFactorData(const CObject3D::SCBTessFactorData& Data)
{
	m_CBTessFactorData = Data;
}

const CObject3D::SCBTessFactorData& CObject3D::GetTessFactorData() const
{
	return m_CBTessFactorData;
}

void CObject3D::SetDisplacementData(const CObject3D::SCBDisplacementData& Data)
{
	m_CBDisplacementData = Data;
}

const CObject3D::SCBDisplacementData& CObject3D::GetDisplacementData() const
{
	return m_CBDisplacementData;
}

CMaterialTextureSet* CObject3D::GetMaterialTextureSet(size_t iMaterial)
{
	if (iMaterial >= m_vMaterialTextureSets.size()) return nullptr;
	return m_vMaterialTextureSets[iMaterial].get();
}

void CObject3D::Draw(bool bIgnoreOwnTexture, bool bIgnoreInstances) const
{
	if (IsPatches())
	{
		switch (m_ControlPointCountPerPatch)
		{
		default:
		case 1:
			m_PtrDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST);
			break;
		case 2:
			m_PtrDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST);
			break;
		case 3:
			m_PtrDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
			break;
		case 4:
			m_PtrDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
			break;
		}
		
		m_PtrDeviceContext->Draw((uint32_t)m_PatchCount, 0);
	}
	else
	{
		for (size_t iMesh = 0; iMesh < m_Model.vMeshes.size(); ++iMesh)
		{
			const SMesh& Mesh{ m_Model.vMeshes[iMesh] };
			const CMaterialData& MaterialData{ m_Model.vMaterialData[Mesh.MaterialID] };

			// per mesh
			m_PtrGame->UpdateCBMaterialData(MaterialData);

			if (MaterialData.HasAnyTexture() && !bIgnoreOwnTexture)
			{
				const CMaterialTextureSet* MaterialTextureSet{ m_vMaterialTextureSets[Mesh.MaterialID].get() };
				MaterialTextureSet->UseTextures();
			}

			if (ShouldTessellate())
			{
				m_PtrDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
			}
			else
			{
				m_PtrDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			}

			m_PtrDeviceContext->IASetIndexBuffer(m_vMeshBuffers[iMesh].IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

			m_PtrDeviceContext->IASetVertexBuffers(0, 1, m_vMeshBuffers[iMesh].VertexBuffer.GetAddressOf(),
				&m_vMeshBuffers[iMesh].VertexBufferStride, &m_vMeshBuffers[iMesh].VertexBufferOffset);

			m_PtrDeviceContext->DrawIndexed(static_cast<UINT>(Mesh.vTriangles.size() * 3), 0, 0);
		}
	}
}