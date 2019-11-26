#pragma once

#include "SharedHeader.h"
#include "Material.h"

class CGame;
class CShader;

struct SModel
{
	std::vector<SMesh>						vMeshes{};
	std::vector<CMaterialData>				vMaterialData{};
};

class CObject3D
{
public:
	enum class EFlagsRendering
	{
		None = 0x00,
		NoCulling = 0x01,
		NoLighting = 0x02,
		NoTexture = 0x04,
		UseRawVertexColor = 0x08
	};

	enum class ETessellationType
	{
		FractionalOdd,
		FractionalEven,
		Integer
	};

	struct SCBTessFactorData
	{
		SCBTessFactorData() {}
		SCBTessFactorData(float UniformTessFactor) : EdgeTessFactor{ UniformTessFactor }, InsideTessFactor{ UniformTessFactor } {}

		float		EdgeTessFactor{ 2.0f };
		float		InsideTessFactor{ 2.0f };
		float		Pads[2]{};
	};

	struct SCBDisplacementData
	{
		BOOL		bUseDisplacement{ TRUE };
		float		DisplacementFactor{ 1.0f };
		float		Pads[2]{};
	};

	struct SComponentTransform
	{
		XMVECTOR	Translation{};
		XMVECTOR	Scaling{ XMVectorSet(1, 1, 1, 0) };
		XMMATRIX	MatrixWorld{ XMMatrixIdentity() };

		float		Pitch{};
		float		Yaw{};
		float		Roll{};
	};

	struct SComponentPhysics
	{
		SBoundingSphere	BoundingSphere{};
		bool			bIsPickable{ true };
	};

	struct SComponentRender
	{
		CShader*	PtrVS{};
		CShader*	PtrPS{};
		bool		bIsTransparent{ false };
		bool		bShouldAnimate{ false };
	};

private:
	struct SMeshBuffers
	{
		ComPtr<ID3D11Buffer>	VertexBuffer{};
		UINT					VertexBufferStride{ sizeof(SVertex3D) };
		UINT					VertexBufferOffset{};

		ComPtr<ID3D11Buffer>	IndexBuffer{};
	};

public:
	CObject3D(const std::string& Name, ID3D11Device* const PtrDevice, ID3D11DeviceContext* const PtrDeviceContext, CGame* const PtrGame) :
		m_Name{ Name }, m_PtrDevice{ PtrDevice }, m_PtrDeviceContext{ PtrDeviceContext }, m_PtrGame{ PtrGame }
	{
		assert(m_PtrDevice);
		assert(m_PtrDeviceContext);
		assert(m_PtrGame);
	}
	~CObject3D() {}

public:
	void* operator new(size_t Size)
	{
		return _aligned_malloc(Size, 16);
	}

	void operator delete(void* Pointer)
	{
		_aligned_free(Pointer);
	}

public:
	void Create(const SMesh& Mesh);
	void Create(const SMesh& Mesh, const CMaterialData& MaterialData);
	void Create(const SModel& Model);
	void CreatePatches(size_t ControlPointCountPerPatch, size_t PatchCount);

public:
	void AddMaterial(const CMaterialData& MaterialData);
	void SetMaterial(size_t Index, const CMaterialData& MaterialData);
	size_t GetMaterialCount() const;

	void UpdateQuadUV(const XMFLOAT2& UVOffset, const XMFLOAT2& UVSize);
	void UpdateMeshBuffer(size_t MeshIndex = 0);

	void UpdateWorldMatrix();

	void Draw(bool bIgnoreOwnTexture = false, bool bIgnoreInstances = false) const;

public:
	bool ShouldTessellate() const { return m_bShouldTesselate; }
	void ShouldTessellate(bool Value);

	void TessellationType(ETessellationType eType);
	ETessellationType TessellationType() const;

	void SetTessFactorData(const CObject3D::SCBTessFactorData& Data);
	const CObject3D::SCBTessFactorData& GetTessFactorData() const;

	void SetDisplacementData(const CObject3D::SCBDisplacementData& Data);
	const CObject3D::SCBDisplacementData& GetDisplacementData() const;

public:
	bool IsCreated() const { return m_bIsCreated; }
	bool IsPatches() const { return m_bIsPatch; }
	size_t GetControlPointCountPerPatch() const { return m_ControlPointCountPerPatch; }
	size_t GetPatchCount() const { return m_PatchCount; }
	const SModel& GetModel() const { return m_Model; }
	SModel& GetModel() { return m_Model; }
	const std::string& GetName() const { return m_Name; }
	const std::string& GetModelFileName() const { return m_ModelFileName; }
	CMaterialTextureSet* GetMaterialTextureSet(size_t iMaterial);

private:
	void CreateMeshBuffers();
	void CreateMeshBuffer(size_t MeshIndex);

	void CreateMaterialTextures();
	void CreateMaterialTexture(size_t Index);

	void LimitFloatRotation(float& Value, const float Min, const float Max);

public:
	SComponentTransform			ComponentTransform{};
	SComponentRender			ComponentRender{};
	SComponentPhysics			ComponentPhysics{};
	EFlagsRendering				eFlagsRendering{};

private:
	ID3D11Device* const			m_PtrDevice{};
	ID3D11DeviceContext* const	m_PtrDeviceContext{};
	CGame* const				m_PtrGame{};

private:
	std::string						m_Name{};
	std::string						m_ModelFileName{};
	bool							m_bIsCreated{ false };
	bool							m_bIsPatch{ false };
	size_t							m_ControlPointCountPerPatch{};
	size_t							m_PatchCount{};
	SModel							m_Model{};
	std::vector<std::unique_ptr<CMaterialTextureSet>> m_vMaterialTextureSets{};
	std::vector<SMeshBuffers>		m_vMeshBuffers{};
	SCBTessFactorData				m_CBTessFactorData{};
	SCBDisplacementData				m_CBDisplacementData{};

	bool							m_bShouldTesselate{ false };
	ETessellationType				m_eTessellationType{};
};

ENUM_CLASS_FLAG(CObject3D::EFlagsRendering)