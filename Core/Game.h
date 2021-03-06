#pragma once

#define NOMINMAX 0

#include <Windows.h>
#include <chrono>

#include "Math.h"
#include "Camera.h"
#include "Shader.h"
#include "ConstantBuffer.h"
#include "Material.h"
#include "Object3D.h"
#include "Object3DLine.h"
#include "Object2D.h"
#include "PrimitiveGenerator.h"

#include "TinyXml2/tinyxml2.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_impl_dx11.h"

class CGame
{
public:
	enum class EBaseShader
	{
		VSBase,
		VSSky,
		VSLine,
		VSGizmo,
		VSScreenQuad,
		VSBase2D,
		VSNull,

		HSTriOdd,
		HSTriEven,
		HSTriInteger,
		HSQuadSphere,

		DSTri,
		DSQuadSphere,

		GSNormal,

		PSBase,
		PSVertexColor,
		PSLine,
		PSGizmo,
		PSScreenQuad,
		PSSky,
		PSBase2D,
		PSTest
	};

	struct SCBSpaceWVPData
	{
		XMMATRIX	World{};
		XMMATRIX	ViewProjection{};
		XMMATRIX	WVP{};
	};

	struct SCBSpaceVPData
	{
		XMMATRIX	ViewProjection{};
	};

	struct SCBSpace2DData
	{
		XMMATRIX	World{};
		XMMATRIX	Projection{};
	};

	struct SCBPSFlagsData
	{
		BOOL		bUseTexture{};
		BOOL		bUseLighting{};
		BOOL		bUsePhysicallyBasedRendering{};
		uint32_t	EnvironmentTextureMipLevels{};

		uint32_t	PrefilteredRadianceTextureMipLevels{};
		float		Pads[3]{};
	};

	struct SCBLightData
	{
		XMVECTOR	DirectionalLightDirection{ XMVectorSet(0, 1, 0, 0) };
		XMFLOAT3	DirectionalLightColor{ 1, 1, 1 };
		float		Exposure{ 1.0 };
		XMFLOAT3	AmbientLightColor{ 1, 1, 1 };
		float		AmbientLightIntensity{ 0.5f };
		XMVECTOR	EyePosition{};
	};

	struct SCBMaterialData
	{
		XMFLOAT3	AmbientColor{};
		float		SpecularExponent{ 1 };
		XMFLOAT3	DiffuseColor{};
		float		SpecularIntensity{ 0 };
		XMFLOAT3	SpecularColor{};
		float		Roughness{};

		float		Metalness{};
		uint32_t	FlagsHasTexture{};
		uint32_t	FlagsIsTextureSRGB{};
		float		Reserved{};
	};

	struct SCBGizmoColorFactorData
	{
		XMVECTOR	ColorFactor{};
	};

	struct SCBPS2DFlagsData
	{
		BOOL		bUseTexture{};
		BOOL		Pad[3]{};
	};

	struct SCBEditorTimeData
	{
		float		NormalizedTime{};
		float		NormalizedTimeHalfSpeed{};
		float		Pads[2]{};
	};

	struct SCBScreenData
	{
		XMFLOAT2	InverseScreenSize{};
		float		Pads[2]{};
	};

	enum class EFlagsRendering
	{
		None = 0x000,
		DrawWireFrame = 0x001,
		DrawNormals = 0x002,
		Use3DGizmos = 0x004,
		DrawMiniAxes = 0x008,
		DrawPickingData = 0x010,
		DrawBoundingSphere = 0x020,
		UseLighting = 0x400,
		UsePhysicallyBasedRendering = 0x800
	};

	enum class ERasterizerState
	{
		CullNone,
		CullClockwise,
		CullCounterClockwise,
		WireFrame
	};

	enum class E3DGizmoMode
	{
		Translation,
		Rotation,
		Scaling
	};

	enum class E3DGizmoAxis
	{
		None,
		AxisX,
		AxisY,
		AxisZ
	};

	struct SEditorGUIBools
	{
		bool bShowWindowPropertyEditor{ true };
		bool bShowWindowSceneEditor{ true };
		bool bShowPopupObjectAdder{ false };
		bool bShowPopupMaterialNameChanger{ false };
		bool bShowPopupMaterialTextureExplorer{ false };
	};

	struct SObject3DPickingCandiate
	{
		SObject3DPickingCandiate() {};
		SObject3DPickingCandiate(CObject3D* _PtrObject3D, XMVECTOR _T) : PtrObject3D{ _PtrObject3D }, T{ _T } {}
		SObject3DPickingCandiate(CObject3D* _PtrObject3D, int _InstanceID, XMVECTOR _T) : PtrObject3D{ _PtrObject3D }, InstanceID{ _InstanceID }, T{ _T } {}

		CObject3D*	PtrObject3D{};
		int			InstanceID{ -1 };
		XMVECTOR	T{};
		bool		bHasFailedPickingTest{ false };
	};

	struct SScreenQuadVertex
	{
		SScreenQuadVertex(const XMFLOAT4& _Position, const XMFLOAT3& _TexCoord) : Position{ _Position }, TexCoord{ _TexCoord } {}
		XMFLOAT4 Position;
		XMFLOAT3 TexCoord;
	};

public:
	CGame(HINSTANCE hInstance, const XMFLOAT2& WindowSize) : m_hInstance{ hInstance }, m_WindowSize{ WindowSize } {}
	~CGame() {}

public:
	void CreateWin32(WNDPROC const WndProc, const std::string& WindowName, bool bWindowed);
	void CreateSpriteFont(const std::wstring& FontFileName);
	void Destroy();

private:
	void CreateWin32Window(WNDPROC const WndProc, const std::string& WindowName);
	void InitializeDirectX(bool bWindowed);
	void InitializeEditorAssets();
	void InitializeImGui(const std::string& FontFileName, float FontSize);

private:
	void CreateSwapChain(bool bWindowed);
	void CreateViews();
	void InitializeViewports();
	void CreateDepthStencilStates();
	void CreateBlendStates();
	void CreateInputDevices();
	void CreateConstantBuffers();
	void CreateBaseShaders();
	void CreateMiniAxes();
	void CreatePickingRay();
	void CreatePickedTriangle();
	void CreateBoundingSphere();
	void Create3DGizmos();
	void CreateScreenQuadVertexBuffer();

	// Advanced settings
public:
	void SetProjectionMatrices(float FOV, float NearZ, float FarZ);
	void SetRenderingFlags(EFlagsRendering Flags);
	void ToggleGameRenderingFlags(EFlagsRendering Flags);
	void Set3DGizmoMode(E3DGizmoMode Mode);
	void SetUniversalRSState();
	void SetUniversalbUseLighiting();
	E3DGizmoMode Get3DGizmoMode() const { return m_e3DGizmoMode; }
	CommonStates* GetCommonStates() const { return m_CommonStates.get(); }

	// Shader-related settings
public:
	void UpdateCBMaterialData(const CMaterialData& MaterialData);

private:
	void UpdateCBSpace(const XMMATRIX& World = KMatrixIdentity);

	void UpdateCBTessFactorData(const CObject3D::SCBTessFactorData& Data);
	void UpdateCBDisplacementData(const CObject3D::SCBDisplacementData& Data);

public:
	void CreateStaticSky(float ScalingFactor);

public:
	void SetDirectionalLight(const XMVECTOR& LightSourcePosition, const XMFLOAT3& Color);
	void SetDirectionalLightDirection(const XMVECTOR& LightSourcePosition);
	void SetDirectionalLightColor(const XMFLOAT3& Color);
	const XMVECTOR& GetDirectionalLightDirection() const;
	const XMFLOAT3& GetDirectionalLightColor() const;

	void SetAmbientlLight(const XMFLOAT3& Color, float Intensity);
	const XMFLOAT3& GetAmbientLightColor() const;
	float GetAmbientLightIntensity() const;

	void SetExposure(float Value);
	float GetExposure();

	// Object pool
public:
	bool InsertCamera(const std::string& Name);
	void DeleteCamera(const std::string& Name);
	void ClearCameras();
	CCamera* GetCamera(const std::string& Name, bool bShowWarning = true);
	const std::map<std::string, size_t>& GetCameraMap() const { return m_mapCameraNameToIndex; }

private:
	void CreateEditorCamera();
	CCamera* GetEditorCamera(bool bShowWarning = true);

public:
	CShader* AddCustomShader();
	CShader* GetCustomShader(size_t Index) const;
	CShader* GetBaseShader(EBaseShader eShader) const;

	bool InsertObject3D(const std::string& Name);
	void DeleteObject3D(const std::string& Name);
	void ClearObject3Ds();
	CObject3D* GetObject3D(const std::string& Name, bool bShowWarning = true) const;
	const std::map<std::string, size_t>& GetObject3DMap() const { return m_mapObject3DNameToIndex; }

	bool InsertObject3DLine(const std::string& Name, bool bShowWarning = true);
	void ClearObject3DLines();
	CObject3DLine* GetObject3DLine(const std::string& Name, bool bShowWarning = true) const;

	bool InsertObject2D(const std::string& Name);
	void DeleteObject2D(const std::string& Name);
	void ClearObject2Ds();
	CObject2D* GetObject2D(const std::string& Name, bool bShowWarning = true) const;
	const std::map<std::string, size_t>& GetObject2DMap() const { return m_mapObject2DNameToIndex; }

	bool InsertMaterial(const std::string& Name, bool bShowWarning = true);
	bool InsertMaterialCreateTextures(const CMaterialData& MaterialData, bool bShowWarning = true);
	void DeleteMaterial(const std::string& Name);
	void CreateMaterialTextures(CMaterialData& MaterialData);
	void ClearMaterials();
	CMaterialData* GetMaterial(const std::string& Name, bool bShowWarning = true);
	CMaterialTextureSet* GetMaterialTextureSet(const std::string& Name, bool bShowWarning = true);
	size_t GetMaterialCount() const;
	bool ChangeMaterialName(const std::string& OldName, const std::string& NewName);
	const std::map<std::string, size_t>& GetMaterialMap() const { return m_mapMaterialNameToIndex; }
	ID3D11ShaderResourceView* GetMaterialTextureSRV(STextureData::EType eType, const std::string& Name) const;

public:
	void NotifyMouseLeftDown();
	void NotifyMouseLeftUp();

private:
	bool Pick();
	const std::string& GetPickedObject3DName() const;

	void SelectObject3D(const std::string& Name);
	void DeselectObject3D();
	bool IsAnyObject3DSelected() const;
	CObject3D* GetSelectedObject3D();
	const std::string& GetSelectedObject3DName() const;

	void SelectObject2D(const std::string& Name);
	void DeselectObject2D();
	bool IsAnyObject2DSelected() const;
	CObject2D* GetSelectedObject2D();
	const std::string& GetSelectedObject2DName() const;

	void SelectCamera(const std::string& Name);
	void DeselectCamera();
	bool IsAnyCameraSelected() const;
	CCamera* GetSelectedCamera();
	const std::string& GetSelectedCameraName() const;
	CCamera* GetCurrentCamera();

	void Select3DGizmos();
	void Deselect3DGizmos();
	bool IsGizmoHovered() const;
	bool IsGizmoSelected() const;
	bool ShouldSelectRotationGizmo(const CObject3D* const Gizmo, E3DGizmoAxis Axis);
	bool ShouldSelectTranslationScalingGizmo(const CObject3D* const Gizmo, E3DGizmoAxis Axis);

	void DeselectAll();

private:
	void CastPickingRay();
	void PickBoundingSphere();
	bool PickTriangle();

public:
	void BeginRendering(const FLOAT* ClearColor);
	void Update();
	void Draw();
	void EndRendering();

private:
	void DrawScreenQuadToSceen(CShader* const PixelShader, bool bShouldClearDeviceRTV);

public:
	auto GethWnd() const->HWND { return m_hWnd; }
	auto GetDevicePtr() const->ID3D11Device* { return m_Device.Get(); }
	auto GetDeviceContextPtr() const->ID3D11DeviceContext* { return m_DeviceContext.Get(); }
	auto GetKeyState() const->Keyboard::State;
	auto GetMouseState() const->Mouse::State;
	auto GetSpriteBatchPtr() const->SpriteBatch* { return m_SpriteBatch.get(); }
	auto GetSpriteFontPtr() const->SpriteFont* { return m_SpriteFont.get(); }
	auto GetWindowSize() const->const XMFLOAT2&;
	auto GetDepthStencilStateLessEqualNoWrite() const->ID3D11DepthStencilState* { return m_DepthStencilStateLessEqualNoWrite.Get(); }
	auto GetBlendStateAlphaToCoverage() const->ID3D11BlendState* { return m_BlendAlphaToCoverage.Get(); }
	auto GetWorkingDirectory() const->const char* { return m_WorkingDirectory; }
	auto GetDeltaTime() const->float { return m_DeltaTimeF; }

private:
	void UpdateObject3D(CObject3D* const PtrObject3D);
	void DrawObject3D(const CObject3D* const PtrObject3D, bool bIgnoreInstances = false, bool bIgnoreOwnTexture = false);
	void DrawObject3DBoundingSphere(const CObject3D* const PtrObject3D);

	void DrawObject3DLines();

	void DrawObject2Ds();

	void DrawMiniAxes();

	void UpdatePickingRay();
	void DrawPickingRay();
	void DrawPickedTriangle();

	void DrawSky(float DeltaTime);

	void Draw3DGizmos();
	void Draw3DGizmoTranslations(E3DGizmoAxis Axis);
	void Draw3DGizmoRotations(E3DGizmoAxis Axis);
	void Draw3DGizmoScalings(E3DGizmoAxis Axis);
	void Draw3DGizmo(CObject3D* const Gizmo, bool bShouldHighlight);

	void DrawEditorGUI();
	void DrawEditorGUIMenuBar();
	void DrawEditorGUIPopupObjectAdder();
	void DrawEditorGUIWindowPropertyEditor();

	// return true if any interaction is required
	bool DrawEditorGUIWindowPropertyEditor_MaterialData(CMaterialData& MaterialData, CMaterialTextureSet* const TextureSet,
		STextureData::EType& eSeletedTextureType, float ItemsOffsetX);
	void DrawEditorGUIPopupMaterialNameChanger(CMaterialData*& capturedMaterialData, bool bIsEditorMaterial);
	void DrawEditorGUIPopupMaterialTextureExplorer(CMaterialData* const capturedMaterialData, CMaterialTextureSet* const capturedMaterialTextureSet,
		STextureData::EType eSelectedTextureType);
	void DrawEditorGUIWindowSceneEditor();

public:
	static constexpr float KTranslationMinLimit{ -1000.0f };
	static constexpr float KTranslationMaxLimit{ +1000.0f };
	static constexpr float KTranslationDelta{ +0.0078125f };
	static constexpr float KRotationMaxLimit{ +XM_2PI };
	static constexpr float KRotationMinLimit{ -XM_2PI };
	static constexpr float KRotationDelta{ +0.25f };
	static constexpr int KRotation360MaxLimit{ 360 };
	static constexpr int KRotation360MinLimit{ 360 };
	static constexpr int KRotation360Unit{ 1 };
	static constexpr float KRotation360To2PI{ 1.0f / 360.0f * XM_2PI };
	static constexpr float KRotation2PITo360{ 1.0f / XM_2PI * 360.0f };
	static constexpr float KScalingMaxLimit{ +100.0f };
	static constexpr float KScalingMinLimit{ +0.001f };
	static constexpr float KScalingDelta{ +0.0078125f };
	static constexpr float KBSCenterOffsetMinLimit{ -10.0f };
	static constexpr float KBSCenterOffsetMaxLimit{ +10.0f };
	static constexpr float KBSCenterOffsetDelta{ +0.01f };
	static constexpr float KBSRadiusMinLimit{ 0.001f };
	static constexpr float KBSRadiusMaxLimit{ 10.0f };
	static constexpr float KBSRadiusDelta{ +0.01f };
	static constexpr float KBSRadiusBiasMinLimit{ 0.001f };
	static constexpr float KBSRadiusBiasMaxLimit{ 1000.0f };
	static constexpr float KBSRadiusBiasDelta{ +0.01f };
	static constexpr int KAssetNameMaxLength{ 100 };

private:
	static constexpr float KDefaultFOV{ 50.0f / 360.0f * XM_2PI };
	static constexpr float KDefaultNearZ{ 0.1f };
	static constexpr float KDefaultFarZ{ 1000.0f };
	static constexpr float KSkyDistance{ 100.0f };
	static constexpr float KPickingRayLength{ 1000.0f };
	static constexpr uint32_t KSkySphereSegmentCount{ 32 };
	static constexpr XMVECTOR KColorWhite{ 1.0f, 1.0f, 1.0f, 1.0f };
	static constexpr XMVECTOR KSkySphereColorUp{ 0.1f, 0.5f, 1.0f, 1.0f };
	static constexpr XMVECTOR KSkySphereColorBottom{ 1.2f, 1.2f, 1.2f, 1.0f };
	static constexpr float K3DGizmoRadius{ 0.05f };
	static constexpr float K3DGizmoSelectionRadius{ 1.1f };
	static constexpr float K3DGizmoSelectionLowBoundary{ 0.8f };
	static constexpr float K3DGizmoSelectionHighBoundary{ 1.2f };
	static constexpr float K3DGizmoMovementFactor{ 0.01f };
	static constexpr float K3DGizmoCameraDistanceThreshold{ 0.03125f };
	static constexpr float K3DGizmoDistanceFactorExponent{ 0.75f };
	static constexpr int KRotationGizmoRingSegmentCount{ 36 };
	static constexpr int KEnvironmentTextureSlot{ 50 };
	static constexpr int KIrradianceTextureSlot{ 51 };
	static constexpr int KPrefilteredRadianceTextureSlot{ 52 };
	static constexpr int KIntegratedBRDFTextureSlot{ 53 };

	static constexpr char KTextureDialogFilter[45]{ "JPG 파일\0*.jpg\0PNG 파일\0*.png\0모든 파일\0*.*\0" };
	static constexpr char KTextureDialogTitle[16]{ "텍스쳐 불러오기" };

private:
	std::unique_ptr<CShader>	m_VSBase{};
	std::unique_ptr<CShader>	m_VSSky{};
	std::unique_ptr<CShader>	m_VSLine{};
	std::unique_ptr<CShader>	m_VSGizmo{};
	std::unique_ptr<CShader>	m_VSScreenQuad{};
	std::unique_ptr<CShader>	m_VSBase2D{};
	std::unique_ptr<CShader>	m_VSNull{};

	std::unique_ptr<CShader>	m_HSTriOdd{};
	std::unique_ptr<CShader>	m_HSTriEven{};
	std::unique_ptr<CShader>	m_HSTriInteger{};
	std::unique_ptr<CShader>	m_HSQuadSphere{};

	std::unique_ptr<CShader>	m_DSTri{};
	std::unique_ptr<CShader>	m_DSQuadSphere{};

	std::unique_ptr<CShader>	m_GSNormal{};

	std::unique_ptr<CShader>	m_PSBase{};
	std::unique_ptr<CShader>	m_PSVertexColor{};
	std::unique_ptr<CShader>	m_PSLine{};
	std::unique_ptr<CShader>	m_PSGizmo{};
	std::unique_ptr<CShader>	m_PSScreenQuad{};
	std::unique_ptr<CShader>	m_PSSky{};
	std::unique_ptr<CShader>	m_PSBase2D{};
	std::unique_ptr<CShader>	m_PSTest{};

private:
	std::unique_ptr<CConstantBuffer> m_CBSpaceWVP{};
	std::unique_ptr<CConstantBuffer> m_CBSpaceVP{};
	std::unique_ptr<CConstantBuffer> m_CBSpace2D{};
	std::unique_ptr<CConstantBuffer> m_CBTessFactor{};
	std::unique_ptr<CConstantBuffer> m_CBDisplacement{};
	std::unique_ptr<CConstantBuffer> m_CBLight{};
	std::unique_ptr<CConstantBuffer> m_CBMaterial{};
	std::unique_ptr<CConstantBuffer> m_CBPSFlags{}; // ...
	std::unique_ptr<CConstantBuffer> m_CBGizmoColorFactor{};
	std::unique_ptr<CConstantBuffer> m_CBPS2DFlags{}; // ...
	std::unique_ptr<CConstantBuffer> m_CBEditorTime{};
	std::unique_ptr<CConstantBuffer> m_CBScreen{};

	SCBSpaceWVPData				m_CBSpaceWVPData{};
	SCBSpaceVPData				m_CBSpaceVPData{};
	SCBSpace2DData				m_CBSpace2DData{};

	CObject3D::SCBTessFactorData	m_CBTessFactorData{};
	CObject3D::SCBDisplacementData	m_CBDisplacementData{};

	SCBLightData					m_CBLightData{};
	SCBMaterialData					m_CBMaterialData{};
	SCBPSFlagsData					m_CBPSFlagsData{};
	SCBGizmoColorFactorData			m_CBGizmoColorFactorData{};

	SCBPS2DFlagsData					m_CBPS2DFlagsData{};
	SCBEditorTimeData					m_CBEditorTimeData{};
	SCBScreenData						m_CBScreenData{};

private:
	std::vector<std::unique_ptr<CShader>>				m_vShaders{};
	std::vector<std::unique_ptr<CObject3D>>				m_vObject3Ds{};
	std::vector<std::unique_ptr<CObject3DLine>>			m_vObject3DLines{};
	std::vector<std::unique_ptr<CObject2D>>				m_vObject2Ds{};
	std::vector<CMaterialData>							m_vMaterialData{};
	std::vector<std::unique_ptr<CMaterialTextureSet>>	m_vMaterialTextureSets{};

	std::unique_ptr<CObject3DLine>				m_Object3DLinePickingRay{};
	std::unique_ptr<CObject3D>					m_Object3DPickedTriangle{};

	std::unique_ptr<CObject3D>					m_Object3DBoundingSphere{};

	std::vector<std::unique_ptr<CObject3D>>		m_vObject3DMiniAxes{};

	float									m_SkyScalingFactor{};
	std::unique_ptr<CObject3D>				m_Object3DSkySphere{};

	std::map<std::string, size_t>	m_mapMaterialNameToIndex{};
	std::map<std::string, size_t>	m_mapCameraNameToIndex{};
	std::map<std::string, size_t>	m_mapObject3DNameToIndex{};
	std::map<std::string, size_t>	m_mapObject3DLineNameToIndex{};
	std::map<std::string, size_t>	m_mapObject2DNameToIndex{};

	size_t							m_PrimitiveCreationCounter{};

private:
	std::unique_ptr<CObject3D>		m_Object3D_3DGizmoRotationPitch{};
	std::unique_ptr<CObject3D>		m_Object3D_3DGizmoRotationYaw{};
	std::unique_ptr<CObject3D>		m_Object3D_3DGizmoRotationRoll{};

	std::unique_ptr<CObject3D>		m_Object3D_3DGizmoTranslationX{};
	std::unique_ptr<CObject3D>		m_Object3D_3DGizmoTranslationY{};
	std::unique_ptr<CObject3D>		m_Object3D_3DGizmoTranslationZ{};

	std::unique_ptr<CObject3D>		m_Object3D_3DGizmoScalingX{};
	std::unique_ptr<CObject3D>		m_Object3D_3DGizmoScalingY{};
	std::unique_ptr<CObject3D>		m_Object3D_3DGizmoScalingZ{};

	bool						m_bIsGizmoHovered{ false };
	bool						m_bIsGizmoSelected{ false };
	E3DGizmoAxis				m_e3DGizmoSelectedAxis{};
	E3DGizmoMode				m_e3DGizmoMode{};
	float						m_3DGizmoDistanceScalar{};
	XMVECTOR					m_CapturedGizmoTranslation{};

private:
	std::vector<D3D11_VIEWPORT>	m_vViewports{};

private:
	HWND			m_hWnd{};
	HINSTANCE		m_hInstance{};
	XMFLOAT2		m_WindowSize{};
	char			m_WorkingDirectory[MAX_PATH]{};

private:
	XMMATRIX		m_MatrixProjection{};
	XMMATRIX		m_MatrixProjection2D{};
	float			m_NearZ{};
	float			m_FarZ{};

	XMMATRIX								m_MatrixView{};
	std::vector<std::unique_ptr<CCamera>>	m_vCameras{};
	CCamera*								m_PtrCurrentCamera{};
	CCamera*								m_PtrSelectedCamera{};
	float									m_CameraMovementFactor{ 10.0f };

private:
	XMVECTOR	m_PickingRayWorldSpaceOrigin{};
	XMVECTOR	m_PickingRayWorldSpaceDirection{};
	std::vector<SObject3DPickingCandiate>	m_vObject3DPickingCandidates{};
	CObject3D*	m_PtrPickedObject3D{};
	CObject3D*	m_PtrSelectedObject3D{};
	CObject2D*	m_PtrSelectedObject2D{};
	XMVECTOR	m_PickedTriangleV0{};
	XMVECTOR	m_PickedTriangleV1{};
	XMVECTOR	m_PickedTriangleV2{};

private:
	ImFont*						m_EditorGUIFont{};
	SEditorGUIBools				m_EditorGUIBools{};

private:
	std::chrono::steady_clock	m_Clock{};
	long long					m_TimeNow{};
	long long					m_TimePrev{};
	long long					m_PreviousFrameTime{};
	long long					m_FPS{};
	long long					m_FrameCount{};
	float						m_DeltaTimeF{};

private:
	ERasterizerState	m_eRasterizerState{ ERasterizerState::CullCounterClockwise };
	EFlagsRendering		m_eFlagsRendering{};

private:
	std::vector<SScreenQuadVertex>		m_vScreenQuadVertices
	{
		{ XMFLOAT4(-1, +1, 0, 1), XMFLOAT3(0, 0, 0) },
		{ XMFLOAT4(+1, +1, 0, 1), XMFLOAT3(1, 0, 0) },
		{ XMFLOAT4(-1, -1, 0, 1), XMFLOAT3(0, 1, 0) },
		{ XMFLOAT4(+1, +1, 0, 1), XMFLOAT3(1, 0, 0) },
		{ XMFLOAT4(+1, -1, 0, 1), XMFLOAT3(1, 1, 0) },
		{ XMFLOAT4(-1, -1, 0, 1), XMFLOAT3(0, 1, 0) },
	};
	ComPtr<ID3D11Buffer>				m_ScreenQuadVertexBuffer{};
	UINT								m_ScreenQuadVertexBufferStride{ sizeof(SScreenQuadVertex) };
	UINT								m_ScreenQuadVertexBufferOffset{};

private:
	ComPtr<IDXGISwapChain>				m_SwapChain{};
	ComPtr<ID3D11Device>				m_Device{};
	ComPtr<ID3D11DeviceContext>			m_DeviceContext{};

	ComPtr<ID3D11RenderTargetView>		m_DeviceRTV{};

	ComPtr<ID3D11RenderTargetView>		m_ScreenQuadRTV{};
	ComPtr<ID3D11ShaderResourceView>	m_ScreenQuadSRV{};
	ComPtr<ID3D11Texture2D>				m_ScreenQuadTexture{};

	std::unique_ptr<CTexture>			m_EnvironmentTexture{};
	std::unique_ptr<CTexture>			m_IrradianceTexture{};
	std::unique_ptr<CTexture>			m_PrefilteredRadianceTexture{};
	std::unique_ptr<CTexture>			m_IntegratedBRDFTexture{};

	ComPtr<ID3D11DepthStencilView>		m_DepthStencilView{};
	ComPtr<ID3D11Texture2D>				m_DepthStencilBuffer{};

	ComPtr<ID3D11DepthStencilState>		m_DepthStencilStateLessEqualNoWrite{};
	ComPtr<ID3D11DepthStencilState>		m_DepthStencilStateAlways{};
	ComPtr<ID3D11BlendState>			m_BlendAlphaToCoverage{};

	std::unique_ptr<Keyboard>			m_Keyboard{};
	std::unique_ptr<Mouse>				m_Mouse{};
	Keyboard::State						m_CapturedKeyboardState{};
	Mouse::State						m_CapturedMouseState{};
	bool								m_bLeftButtonPressedOnce{ false };
	int									m_PrevCapturedMouseX{};
	int									m_PrevCapturedMouseY{};
	std::unique_ptr<SpriteBatch>		m_SpriteBatch{};
	std::unique_ptr<SpriteFont>			m_SpriteFont{};
	std::unique_ptr<CommonStates>		m_CommonStates{};
	bool								m_IsDestroyed{ false };
};

ENUM_CLASS_FLAG(CGame::EFlagsRendering)