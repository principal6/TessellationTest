#include <thread>

#include "Game.h"
#include "Core/FileDialog.h"

using std::max;
using std::min;
using std::vector;
using std::string;
using std::wstring;
using std::thread;
using std::chrono::steady_clock;
using std::to_string;
using std::stof;
using std::make_unique;
using std::swap;

static constexpr D3D11_INPUT_ELEMENT_DESC KBaseInputElementDescs[]
{
	{ "POSITION"	, 0, DXGI_FORMAT_R32G32B32A32_FLOAT	, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR"		, 0, DXGI_FORMAT_R32G32B32A32_FLOAT	, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD"	, 0, DXGI_FORMAT_R32G32B32A32_FLOAT	, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "NORMAL"		, 0, DXGI_FORMAT_R32G32B32A32_FLOAT	, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TANGENT"		, 0, DXGI_FORMAT_R32G32B32A32_FLOAT	, 0, 64, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

// FOR DEBUGGING SHADER...
static constexpr D3D11_INPUT_ELEMENT_DESC KScreenQuadInputElementDescs[]
{
	{ "POSITION"	, 0, DXGI_FORMAT_R32G32B32A32_FLOAT	, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD"	, 0, DXGI_FORMAT_R32G32B32_FLOAT	, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

void CGame::CreateWin32(WNDPROC const WndProc, const std::string& WindowName, bool bWindowed)
{
	GetCurrentDirectoryA(MAX_PATH, m_WorkingDirectory);

	CreateWin32Window(WndProc, WindowName);

	InitializeDirectX(bWindowed);

	InitializeEditorAssets();

	InitializeImGui("Asset\\D2Coding.ttf", 15.0f);
}

void CGame::CreateSpriteFont(const wstring& FontFileName)
{
	if (!m_Device)
	{
		MB_WARN("아직 Device가 생성되지 않았습니다", "SpriteFont 생성 실패");
		return;
	}

	m_SpriteBatch = make_unique<SpriteBatch>(m_DeviceContext.Get());
	m_SpriteFont = make_unique<SpriteFont>(m_Device.Get(), FontFileName.c_str());
}

void CGame::Destroy()
{
	DestroyWindow(m_hWnd);

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	m_IsDestroyed = true;
}

void CGame::CreateWin32Window(WNDPROC const WndProc, const std::string& WindowName)
{
	if (m_hWnd) return;

	constexpr LPCSTR KClassName{ "GameWindow" };
	constexpr DWORD KWindowStyle{ WS_CAPTION | WS_SYSMENU };

	WNDCLASSEXA WindowClass{};
	WindowClass.cbSize = sizeof(WNDCLASSEX);
	WindowClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	WindowClass.hCursor = LoadCursorA(nullptr, IDC_ARROW);
	WindowClass.hIcon = WindowClass.hIconSm = LoadIconA(nullptr, IDI_APPLICATION);
	WindowClass.hInstance = m_hInstance;
	WindowClass.lpfnWndProc = WndProc;
	WindowClass.lpszClassName = KClassName;
	WindowClass.lpszMenuName = nullptr;
	WindowClass.style = CS_VREDRAW | CS_HREDRAW;
	RegisterClassExA(&WindowClass);

	RECT WindowRect{};
	WindowRect.right = static_cast<LONG>(m_WindowSize.x);
	WindowRect.bottom = static_cast<LONG>(m_WindowSize.y);
	AdjustWindowRect(&WindowRect, KWindowStyle, false);

	assert(m_hWnd = CreateWindowExA(0, KClassName, WindowName.c_str(), KWindowStyle,
		CW_USEDEFAULT, CW_USEDEFAULT, WindowRect.right - WindowRect.left, WindowRect.bottom - WindowRect.top,
		nullptr, nullptr, m_hInstance, nullptr));

	ShowWindow(m_hWnd, SW_SHOW);
	UpdateWindow(m_hWnd);
}

void CGame::InitializeDirectX(bool bWindowed)
{
	CreateSwapChain(bWindowed);

	CreateViews();

	CreateDepthStencilStates();
	CreateBlendStates();

	CreateInputDevices();

	CreateConstantBuffers();
	CreateBaseShaders();

	CreateMiniAxes();
	CreatePickingRay();
	CreatePickedTriangle();
	CreateBoundingSphere();
	Create3DGizmos();

	CreateScreenQuadVertexBuffer();

	SetProjectionMatrices(KDefaultFOV, KDefaultNearZ, KDefaultFarZ);
	InitializeViewports();

	m_CommonStates = make_unique<CommonStates>(m_Device.Get());
}

void CGame::InitializeEditorAssets()
{
	CreateEditorCamera();

	if (!m_EnvironmentTexture)
	{
		// @important: use already mipmapped cubemap texture
		m_EnvironmentTexture = make_unique<CTexture>(m_Device.Get(), m_DeviceContext.Get());
		m_EnvironmentTexture->CreateCubeMapFromFile("Asset\\uffizi_environment.dds");
		m_EnvironmentTexture->SetSlot(KEnvironmentTextureSlot);
	}
	
	if (!m_IrradianceTexture)
	{
		// @important: use already mipmapped cubemap texture
		m_IrradianceTexture = make_unique<CTexture>(m_Device.Get(), m_DeviceContext.Get());
		m_IrradianceTexture->CreateCubeMapFromFile("Asset\\uffizi_irradiance.dds");
		m_IrradianceTexture->SetSlot(KIrradianceTextureSlot);
	}

	if (!m_PrefilteredRadianceTexture)
	{
		// @important: use already mipmapped cubemap texture
		m_PrefilteredRadianceTexture = make_unique<CTexture>(m_Device.Get(), m_DeviceContext.Get());
		m_PrefilteredRadianceTexture->CreateCubeMapFromFile("Asset\\uffizi_prefiltered_radiance.dds");
		m_PrefilteredRadianceTexture->SetSlot(KPrefilteredRadianceTextureSlot);
	}

	if (!m_IntegratedBRDFTexture)
	{
		// @important: this is not cubemap nor mipmapped!
		m_IntegratedBRDFTexture = make_unique<CTexture>(m_Device.Get(), m_DeviceContext.Get());
		m_IntegratedBRDFTexture->CreateTextureFromFile("Asset\\integrated_brdf.dds", false);
		m_IntegratedBRDFTexture->SetSlot(KIntegratedBRDFTextureSlot);
	}

	if (InsertObject3DLine("Default3DAxes", false))
	{
		CObject3DLine* Grid{ GetObject3DLine("Default3DAxes") };
		Grid->Create(Generate3DGrid(0));
	}
}

void CGame::InitializeImGui(const std::string& FontFileName, float FontSize)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(m_hWnd);
	ImGui_ImplDX11_Init(m_Device.Get(), m_DeviceContext.Get());

	ImGuiIO& igIO{ ImGui::GetIO() };
	igIO.Fonts->AddFontDefault();
	
	m_EditorGUIFont = igIO.Fonts->AddFontFromFileTTF(FontFileName.c_str(), FontSize, nullptr, igIO.Fonts->GetGlyphRangesKorean());
}

void CGame::CreateSwapChain(bool bWindowed)
{
	DXGI_SWAP_CHAIN_DESC SwapChainDesc{};
	SwapChainDesc.BufferCount = 1;
	SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // LDR
	//SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; // HDR
	SwapChainDesc.BufferDesc.Width = static_cast<UINT>(m_WindowSize.x);
	SwapChainDesc.BufferDesc.Height = static_cast<UINT>(m_WindowSize.y);
	SwapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	SwapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
	SwapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	SwapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	SwapChainDesc.Flags = 0;
	SwapChainDesc.OutputWindow = m_hWnd;
	SwapChainDesc.SampleDesc.Count = 1;
	SwapChainDesc.SampleDesc.Quality = 0;
	SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	SwapChainDesc.Windowed = bWindowed;

	D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
		&SwapChainDesc, &m_SwapChain, &m_Device, nullptr, &m_DeviceContext);
}

void CGame::CreateViews()
{
	// Create back buffer RTV
	ComPtr<ID3D11Texture2D> BackBuffer{};
	m_SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &BackBuffer);
	m_Device->CreateRenderTargetView(BackBuffer.Get(), nullptr, &m_DeviceRTV);

	// Create deferred RTV
	{
		D3D11_TEXTURE2D_DESC Texture2DDesc{};
		Texture2DDesc.ArraySize = 1;
		Texture2DDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		Texture2DDesc.CPUAccessFlags = 0;
		Texture2DDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // LDR
		//Texture2DDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; // HDR
		Texture2DDesc.Height = static_cast<UINT>(m_WindowSize.y);
		Texture2DDesc.MipLevels = 1;
		Texture2DDesc.SampleDesc.Count = 1;
		Texture2DDesc.SampleDesc.Quality = 0;
		Texture2DDesc.Usage = D3D11_USAGE_DEFAULT;
		Texture2DDesc.Width = static_cast<UINT>(m_WindowSize.x);
		m_Device->CreateTexture2D(&Texture2DDesc, nullptr, m_ScreenQuadTexture.ReleaseAndGetAddressOf());

		D3D11_SHADER_RESOURCE_VIEW_DESC ScreenQuadSRVDesc{};
		ScreenQuadSRVDesc.Format = Texture2DDesc.Format;
		ScreenQuadSRVDesc.Texture2D.MipLevels = 1;
		ScreenQuadSRVDesc.Texture2D.MostDetailedMip = 0;
		ScreenQuadSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		m_Device->CreateShaderResourceView(m_ScreenQuadTexture.Get(), &ScreenQuadSRVDesc, m_ScreenQuadSRV.ReleaseAndGetAddressOf());

		D3D11_RENDER_TARGET_VIEW_DESC ScreenQuadRTVDesc{};
		ScreenQuadRTVDesc.Format = Texture2DDesc.Format;
		ScreenQuadRTVDesc.Texture2D.MipSlice = 0;
		ScreenQuadRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		m_Device->CreateRenderTargetView(m_ScreenQuadTexture.Get(), &ScreenQuadRTVDesc, m_ScreenQuadRTV.ReleaseAndGetAddressOf());
	}

	// Create depth-stencil view
	D3D11_TEXTURE2D_DESC DepthStencilBufferDesc{};
	DepthStencilBufferDesc.ArraySize = 1;
	DepthStencilBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	DepthStencilBufferDesc.CPUAccessFlags = 0;
	DepthStencilBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DepthStencilBufferDesc.Width = static_cast<UINT>(m_WindowSize.x);
	DepthStencilBufferDesc.Height = static_cast<UINT>(m_WindowSize.y);
	DepthStencilBufferDesc.MipLevels = 0;
	DepthStencilBufferDesc.MiscFlags = 0;
	DepthStencilBufferDesc.SampleDesc.Count = 1;
	DepthStencilBufferDesc.SampleDesc.Quality = 0;
	DepthStencilBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	m_Device->CreateTexture2D(&DepthStencilBufferDesc, nullptr, &m_DepthStencilBuffer);
	m_Device->CreateDepthStencilView(m_DepthStencilBuffer.Get(), nullptr, &m_DepthStencilView);
}

void CGame::InitializeViewports()
{
	{
		m_vViewports.emplace_back();

		D3D11_VIEWPORT& Viewport{ m_vViewports.back() };
		Viewport.TopLeftX = 0.0f;
		Viewport.TopLeftY = 0.0f;
		Viewport.Width = m_WindowSize.x;
		Viewport.Height = m_WindowSize.y;
		Viewport.MinDepth = 0.0f;
		Viewport.MaxDepth = 1.0f;
	}

	{
		m_vViewports.emplace_back();

		D3D11_VIEWPORT& Viewport{ m_vViewports.back() };
		Viewport.TopLeftX = 0.0f;
		Viewport.TopLeftY = 20.0f;
		Viewport.Width = m_WindowSize.x / 8.0f;
		Viewport.Height = m_WindowSize.y / 8.0f;
		Viewport.MinDepth = 0.0f;
		Viewport.MaxDepth = 1.0f;
	}

	{
		m_vViewports.emplace_back();

		D3D11_VIEWPORT& Viewport{ m_vViewports.back() };
		Viewport.TopLeftX = 0.0f;
		Viewport.TopLeftY = m_WindowSize.y * 7.0f / 8.0f;
		Viewport.Width = m_WindowSize.x / 8.0f;
		Viewport.Height = m_WindowSize.y / 8.0f;
		Viewport.MinDepth = 0.0f;
		Viewport.MaxDepth = 1.0f;
	}

	{
		m_vViewports.emplace_back();

		D3D11_VIEWPORT& Viewport{ m_vViewports.back() };
		Viewport.TopLeftX = m_WindowSize.x * 1.0f / 8.0f;
		Viewport.TopLeftY = m_WindowSize.y * 7.0f / 8.0f;
		Viewport.Width = m_WindowSize.x / 8.0f;
		Viewport.Height = m_WindowSize.y / 8.0f;
		Viewport.MinDepth = 0.0f;
		Viewport.MaxDepth = 1.0f;
	}

	{
		m_vViewports.emplace_back();

		D3D11_VIEWPORT& Viewport{ m_vViewports.back() };
		Viewport.TopLeftX = m_WindowSize.x * 2.0f / 8.0f;
		Viewport.TopLeftY = m_WindowSize.y * 7.0f / 8.0f;
		Viewport.Width = m_WindowSize.x / 8.0f;
		Viewport.Height = m_WindowSize.y / 8.0f;
		Viewport.MinDepth = 0.0f;
		Viewport.MaxDepth = 1.0f;
	}
}

void CGame::CreateDepthStencilStates()
{
	D3D11_DEPTH_STENCIL_DESC DepthStencilDesc{};
	DepthStencilDesc.DepthEnable = TRUE;
	DepthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	DepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	DepthStencilDesc.StencilEnable = FALSE;

	assert(SUCCEEDED(m_Device->CreateDepthStencilState(&DepthStencilDesc, m_DepthStencilStateLessEqualNoWrite.ReleaseAndGetAddressOf())));

	DepthStencilDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	DepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;

	assert(SUCCEEDED(m_Device->CreateDepthStencilState(&DepthStencilDesc, m_DepthStencilStateAlways.ReleaseAndGetAddressOf())));
}

void CGame::CreateBlendStates()
{
	D3D11_BLEND_DESC BlendDesc{};
	BlendDesc.AlphaToCoverageEnable = TRUE;
	BlendDesc.IndependentBlendEnable = FALSE;
	BlendDesc.RenderTarget[0].BlendEnable = TRUE;
	BlendDesc.RenderTarget[0].BlendOp;
	BlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	BlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
	BlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	BlendDesc.RenderTarget[0].BlendOp = BlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	m_Device->CreateBlendState(&BlendDesc, m_BlendAlphaToCoverage.ReleaseAndGetAddressOf());
}

void CGame::CreateInputDevices()
{
	m_Keyboard = make_unique<Keyboard>();

	m_Mouse = make_unique<Mouse>();
	m_Mouse->SetWindow(m_hWnd);
	m_Mouse->SetMode(Mouse::Mode::MODE_ABSOLUTE);
}

void CGame::CreateConstantBuffers()
{
	m_CBSpaceWVP = make_unique<CConstantBuffer>(m_Device.Get(), m_DeviceContext.Get(),
		&m_CBSpaceWVPData, sizeof(m_CBSpaceWVPData));
	m_CBSpaceVP = make_unique<CConstantBuffer>(m_Device.Get(), m_DeviceContext.Get(),
		&m_CBSpaceVPData, sizeof(m_CBSpaceVPData));
	m_CBSpace2D = make_unique<CConstantBuffer>(m_Device.Get(), m_DeviceContext.Get(),
		&m_CBSpace2DData, sizeof(m_CBSpace2DData));
	m_CBTessFactor = make_unique<CConstantBuffer>(m_Device.Get(), m_DeviceContext.Get(),
		&m_CBTessFactorData, sizeof(m_CBTessFactorData));
	m_CBDisplacement = make_unique<CConstantBuffer>(m_Device.Get(), m_DeviceContext.Get(),
		&m_CBDisplacementData, sizeof(m_CBDisplacementData));
	m_CBLight = make_unique<CConstantBuffer>(m_Device.Get(), m_DeviceContext.Get(),
		&m_CBLightData, sizeof(m_CBLightData));
	m_CBMaterial = make_unique<CConstantBuffer>(m_Device.Get(), m_DeviceContext.Get(),
		&m_CBMaterialData, sizeof(m_CBMaterialData));
	m_CBPSFlags = make_unique<CConstantBuffer>(m_Device.Get(), m_DeviceContext.Get(),
		&m_CBPSFlagsData, sizeof(m_CBPSFlagsData));
	m_CBGizmoColorFactor = make_unique<CConstantBuffer>(m_Device.Get(), m_DeviceContext.Get(),
		&m_CBGizmoColorFactorData, sizeof(m_CBGizmoColorFactorData));
	m_CBPS2DFlags = make_unique<CConstantBuffer>(m_Device.Get(), m_DeviceContext.Get(),
		&m_CBPS2DFlagsData, sizeof(m_CBPS2DFlagsData));
	m_CBEditorTime = make_unique<CConstantBuffer>(m_Device.Get(), m_DeviceContext.Get(),
		&m_CBEditorTimeData, sizeof(m_CBEditorTimeData));
	m_CBScreen = make_unique<CConstantBuffer>(m_Device.Get(), m_DeviceContext.Get(),
		&m_CBScreenData, sizeof(m_CBScreenData));

	m_CBSpaceWVP->Create();
	m_CBSpaceVP->Create();
	m_CBSpace2D->Create();
	m_CBTessFactor->Create();
	m_CBDisplacement->Create();
	m_CBLight->Create();
	m_CBMaterial->Create();
	m_CBPSFlags->Create();
	m_CBGizmoColorFactor->Create();
	m_CBPS2DFlags->Create();
	m_CBEditorTime->Create();
	m_CBScreen->Create();
}

void CGame::CreateBaseShaders()
{
	m_VSBase = make_unique<CShader>(m_Device.Get(), m_DeviceContext.Get());
	m_VSBase->Create(EShaderType::VertexShader, L"Shader\\VSBase.hlsl", "main",
		KBaseInputElementDescs, ARRAYSIZE(KBaseInputElementDescs));
	m_VSBase->AttachConstantBuffer(m_CBSpaceWVP.get());

	m_VSSky = make_unique<CShader>(m_Device.Get(), m_DeviceContext.Get());
	m_VSSky->Create(EShaderType::VertexShader, L"Shader\\VSSky.hlsl", "main", 
		KBaseInputElementDescs, ARRAYSIZE(KBaseInputElementDescs));
	m_VSSky->AttachConstantBuffer(m_CBSpaceWVP.get());

	m_VSLine = make_unique<CShader>(m_Device.Get(), m_DeviceContext.Get());
	m_VSLine->Create(EShaderType::VertexShader, L"Shader\\VSLine.hlsl", "main", 
		CObject3DLine::KInputElementDescs, ARRAYSIZE(CObject3DLine::KInputElementDescs));
	m_VSLine->AttachConstantBuffer(m_CBSpaceWVP.get());

	m_VSGizmo = make_unique<CShader>(m_Device.Get(), m_DeviceContext.Get());
	m_VSGizmo->Create(EShaderType::VertexShader, L"Shader\\VSGizmo.hlsl", "main", 
		KBaseInputElementDescs, ARRAYSIZE(KBaseInputElementDescs));
	m_VSGizmo->AttachConstantBuffer(m_CBSpaceWVP.get());

	m_VSScreenQuad = make_unique<CShader>(m_Device.Get(), m_DeviceContext.Get());
	//m_VSScreenQuad->Create(EShaderType::VertexShader, L"Shader\\VSScreenQuad.hlsl", "main");
	m_VSScreenQuad->Create(EShaderType::VertexShader, L"Shader\\VSScreenQuad.hlsl", "main", 
		KScreenQuadInputElementDescs, ARRAYSIZE(KScreenQuadInputElementDescs));

	m_VSBase2D = make_unique<CShader>(m_Device.Get(), m_DeviceContext.Get());
	m_VSBase2D->Create(EShaderType::VertexShader, L"Shader\\VSBase2D.hlsl", "main",
		CObject2D::KInputLayout, ARRAYSIZE(CObject2D::KInputLayout));
	m_VSBase2D->AttachConstantBuffer(m_CBSpace2D.get());

	m_VSNull = make_unique<CShader>(m_Device.Get(), m_DeviceContext.Get());
	m_VSNull->Create(EShaderType::VertexShader, L"Shader\\VSNull.hlsl", "main");

	m_HSTriOdd = make_unique<CShader>(m_Device.Get(), m_DeviceContext.Get());
	m_HSTriOdd->Create(EShaderType::HullShader, L"Shader\\HSTri.hlsl", "main");
	m_HSTriOdd->AttachConstantBuffer(m_CBTessFactor.get());

	m_HSTriEven = make_unique<CShader>(m_Device.Get(), m_DeviceContext.Get());
	m_HSTriEven->Create(EShaderType::HullShader, L"Shader\\HSTri.hlsl", "even");
	m_HSTriEven->AttachConstantBuffer(m_CBTessFactor.get());

	m_HSTriInteger = make_unique<CShader>(m_Device.Get(), m_DeviceContext.Get());
	m_HSTriInteger->Create(EShaderType::HullShader, L"Shader\\HSTri.hlsl", "integer");
	m_HSTriInteger->AttachConstantBuffer(m_CBTessFactor.get());

	m_HSQuadSphere = make_unique<CShader>(m_Device.Get(), m_DeviceContext.Get());
	m_HSQuadSphere->Create(EShaderType::HullShader, L"Shader\\HSQuadSphere.hlsl", "main");
	m_HSQuadSphere->AttachConstantBuffer(m_CBTessFactor.get());

	m_DSTri = make_unique<CShader>(m_Device.Get(), m_DeviceContext.Get());
	m_DSTri->Create(EShaderType::DomainShader, L"Shader\\DSTri.hlsl", "main");
	m_DSTri->AttachConstantBuffer(m_CBSpaceVP.get());
	m_DSTri->AttachConstantBuffer(m_CBDisplacement.get());

	m_DSQuadSphere = make_unique<CShader>(m_Device.Get(), m_DeviceContext.Get());
	m_DSQuadSphere->Create(EShaderType::DomainShader, L"Shader\\DSQuadSphere.hlsl", "main");
	m_DSQuadSphere->AttachConstantBuffer(m_CBSpaceWVP.get());

	m_GSNormal = make_unique<CShader>(m_Device.Get(), m_DeviceContext.Get());
	m_GSNormal->Create(EShaderType::GeometryShader, L"Shader\\GSNormal.hlsl", "main");
	m_GSNormal->AttachConstantBuffer(m_CBSpaceVP.get());

	m_PSBase = make_unique<CShader>(m_Device.Get(), m_DeviceContext.Get());
	m_PSBase->Create(EShaderType::PixelShader, L"Shader\\PSBase.hlsl", "main");
	m_PSBase->AttachConstantBuffer(m_CBPSFlags.get());
	m_PSBase->AttachConstantBuffer(m_CBLight.get());
	m_PSBase->AttachConstantBuffer(m_CBMaterial.get());

	m_PSVertexColor = make_unique<CShader>(m_Device.Get(), m_DeviceContext.Get());
	m_PSVertexColor->Create(EShaderType::PixelShader, L"Shader\\PSVertexColor.hlsl", "main");

	m_PSLine = make_unique<CShader>(m_Device.Get(), m_DeviceContext.Get());
	m_PSLine->Create(EShaderType::PixelShader, L"Shader\\PSLine.hlsl", "main");

	m_PSGizmo = make_unique<CShader>(m_Device.Get(), m_DeviceContext.Get());
	m_PSGizmo->Create(EShaderType::PixelShader, L"Shader\\PSGizmo.hlsl", "main");
	m_PSGizmo->AttachConstantBuffer(m_CBGizmoColorFactor.get());

	m_PSScreenQuad = make_unique<CShader>(m_Device.Get(), m_DeviceContext.Get());
	m_PSScreenQuad->Create(EShaderType::PixelShader, L"Shader\\PSScreenQuad.hlsl", "main");

	m_CBScreenData.InverseScreenSize = XMFLOAT2(1.0f / m_WindowSize.x, 1.0f / m_WindowSize.y);
	m_CBScreen->Update();

	m_PSSky = make_unique<CShader>(m_Device.Get(), m_DeviceContext.Get());
	m_PSSky->Create(EShaderType::PixelShader, L"Shader\\PSSky.hlsl", "main");

	m_PSBase2D = make_unique<CShader>(m_Device.Get(), m_DeviceContext.Get());
	m_PSBase2D->Create(EShaderType::PixelShader, L"Shader\\PSBase2D.hlsl", "main");
	m_PSBase2D->AttachConstantBuffer(m_CBPS2DFlags.get());

	m_PSTest = make_unique<CShader>(m_Device.Get(), m_DeviceContext.Get());
	m_PSTest->Create(EShaderType::PixelShader, L"Shader\\PSTest.hlsl", "main");
}

void CGame::CreateMiniAxes()
{
	m_vObject3DMiniAxes.emplace_back(make_unique<CObject3D>("AxisX", m_Device.Get(), m_DeviceContext.Get(), this));
	m_vObject3DMiniAxes.emplace_back(make_unique<CObject3D>("AxisY", m_Device.Get(), m_DeviceContext.Get(), this));
	m_vObject3DMiniAxes.emplace_back(make_unique<CObject3D>("AxisZ", m_Device.Get(), m_DeviceContext.Get(), this));

	const SMesh KAxisCone{ GenerateCone(0, 1.0f, 1.0f, 16) };
	vector<CMaterialData> vMaterialData{};
	vMaterialData.resize(3);
	vMaterialData[0].SetUniformColor(XMFLOAT3(1, 0, 0));
	vMaterialData[1].SetUniformColor(XMFLOAT3(0, 1, 0));
	vMaterialData[2].SetUniformColor(XMFLOAT3(0, 0, 1));
	m_vObject3DMiniAxes[0]->Create(KAxisCone, vMaterialData[0]);
	m_vObject3DMiniAxes[0]->ComponentRender.PtrVS = m_VSBase.get();
	m_vObject3DMiniAxes[0]->ComponentRender.PtrPS = m_PSBase.get();
	m_vObject3DMiniAxes[0]->ComponentTransform.Roll = -XM_PIDIV2;
	m_vObject3DMiniAxes[0]->eFlagsRendering = CObject3D::EFlagsRendering::NoLighting;

	m_vObject3DMiniAxes[1]->Create(KAxisCone, vMaterialData[1]);
	m_vObject3DMiniAxes[1]->ComponentRender.PtrVS = m_VSBase.get();
	m_vObject3DMiniAxes[1]->ComponentRender.PtrPS = m_PSBase.get();
	m_vObject3DMiniAxes[1]->eFlagsRendering = CObject3D::EFlagsRendering::NoLighting;

	m_vObject3DMiniAxes[2]->Create(KAxisCone, vMaterialData[2]);
	m_vObject3DMiniAxes[2]->ComponentRender.PtrVS = m_VSBase.get();
	m_vObject3DMiniAxes[2]->ComponentRender.PtrPS = m_PSBase.get();
	m_vObject3DMiniAxes[2]->ComponentTransform.Yaw = -XM_PIDIV2;
	m_vObject3DMiniAxes[2]->ComponentTransform.Roll = -XM_PIDIV2;
	m_vObject3DMiniAxes[2]->eFlagsRendering = CObject3D::EFlagsRendering::NoLighting;

	m_vObject3DMiniAxes[0]->ComponentTransform.Scaling =
		m_vObject3DMiniAxes[1]->ComponentTransform.Scaling =
		m_vObject3DMiniAxes[2]->ComponentTransform.Scaling = XMVectorSet(0.1f, 0.8f, 0.1f, 0);
}

void CGame::CreatePickingRay()
{
	m_Object3DLinePickingRay = make_unique<CObject3DLine>("PickingRay", m_Device.Get(), m_DeviceContext.Get());

	vector<SVertex3DLine> Vertices{};
	Vertices.emplace_back(XMVectorSet(0, 0, 0, 1), XMVectorSet(1, 0, 0, 1));
	Vertices.emplace_back(XMVectorSet(10.0f, 10.0f, 0, 1), XMVectorSet(0, 1, 0, 1));

	m_Object3DLinePickingRay->Create(Vertices);
}

void CGame::CreatePickedTriangle()
{
	m_Object3DPickedTriangle = make_unique<CObject3D>("PickedTriangle", m_Device.Get(), m_DeviceContext.Get(), this);

	m_Object3DPickedTriangle->Create(GenerateTriangle(XMVectorSet(0, 0, 1.5f, 1), XMVectorSet(+1.0f, 0, 0, 1), XMVectorSet(-1.0f, 0, 0, 1),
		XMVectorSet(1.0f, 1.0f, 0.0f, 1.0f)));
}

void CGame::CreateBoundingSphere()
{
	m_Object3DBoundingSphere = make_unique<CObject3D>("BoundingSphere", m_Device.Get(), m_DeviceContext.Get(), this);

	m_Object3DBoundingSphere->Create(GenerateSphere(16));
}

void CGame::Create3DGizmos()
{
	const static XMVECTOR ColorX{ XMVectorSet(1.00f, 0.25f, 0.25f, 1) };
	const static XMVECTOR ColorY{ XMVectorSet(0.25f, 1.00f, 0.25f, 1) };
	const static XMVECTOR ColorZ{ XMVectorSet(0.25f, 0.25f, 1.00f, 1) };

	if (!m_Object3D_3DGizmoRotationPitch)
	{
		m_Object3D_3DGizmoRotationPitch = make_unique<CObject3D>("Gizmo", m_Device.Get(), m_DeviceContext.Get(), this);
		SMesh MeshRing{ GenerateTorus(K3DGizmoRadius, 16, KRotationGizmoRingSegmentCount, ColorX) };
		SMesh MeshAxis{ GenerateCylinder(K3DGizmoRadius, 1.0f, 16, ColorX) };
		TranslateMesh(MeshAxis, XMVectorSet(0, 0.5f, 0, 0));
		m_Object3D_3DGizmoRotationPitch->Create(MergeStaticMeshes(MeshRing, MeshAxis));
		m_Object3D_3DGizmoRotationPitch->ComponentTransform.Roll = -XM_PIDIV2;
	}
	
	if (!m_Object3D_3DGizmoRotationYaw)
	{
		m_Object3D_3DGizmoRotationYaw = make_unique<CObject3D>("Gizmo", m_Device.Get(), m_DeviceContext.Get(), this);
		SMesh MeshRing{ GenerateTorus(K3DGizmoRadius, 16, KRotationGizmoRingSegmentCount, ColorY) };
		SMesh MeshAxis{ GenerateCylinder(K3DGizmoRadius, 1.0f, 16, ColorY) };
		TranslateMesh(MeshAxis, XMVectorSet(0, 0.5f, 0, 0));
		m_Object3D_3DGizmoRotationYaw->Create(MergeStaticMeshes(MeshRing, MeshAxis));
	}

	if (!m_Object3D_3DGizmoRotationRoll)
	{
		m_Object3D_3DGizmoRotationRoll = make_unique<CObject3D>("Gizmo", m_Device.Get(), m_DeviceContext.Get(), this);
		SMesh MeshRing{ GenerateTorus(K3DGizmoRadius, 16, KRotationGizmoRingSegmentCount, ColorZ) };
		SMesh MeshAxis{ GenerateCylinder(K3DGizmoRadius, 1.0f, 16, ColorZ) };
		TranslateMesh(MeshAxis, XMVectorSet(0, 0.5f, 0, 0));
		m_Object3D_3DGizmoRotationRoll->Create(MergeStaticMeshes(MeshRing, MeshAxis));
		m_Object3D_3DGizmoRotationRoll->ComponentTransform.Pitch = XM_PIDIV2;
	}
	
	if (!m_Object3D_3DGizmoTranslationX)
	{
		m_Object3D_3DGizmoTranslationX = make_unique<CObject3D>("Gizmo", m_Device.Get(), m_DeviceContext.Get(), this);
		SMesh MeshAxis{ GenerateCylinder(K3DGizmoRadius, 1.0f, 16, ColorX) };
		SMesh MeshCone{ GenerateCone(0, 0.1f, 0.5f, 16, ColorX) };
		TranslateMesh(MeshCone, XMVectorSet(0, 0.5f, 0, 0));
		MeshAxis = MergeStaticMeshes(MeshAxis, MeshCone);
		TranslateMesh(MeshAxis, XMVectorSet(0, 0.5f, 0, 0));
		m_Object3D_3DGizmoTranslationX->Create(MeshAxis);
		m_Object3D_3DGizmoTranslationX->ComponentTransform.Roll = -XM_PIDIV2;
	}
	
	if (!m_Object3D_3DGizmoTranslationY)
	{
		m_Object3D_3DGizmoTranslationY = make_unique<CObject3D>("Gizmo", m_Device.Get(), m_DeviceContext.Get(), this);
		SMesh MeshAxis{ GenerateCylinder(K3DGizmoRadius, 1.0f, 16, ColorY) };
		SMesh MeshCone{ GenerateCone(0, 0.1f, 0.5f, 16, ColorY) };
		TranslateMesh(MeshCone, XMVectorSet(0, 0.5f, 0, 0));
		MeshAxis = MergeStaticMeshes(MeshAxis, MeshCone);
		TranslateMesh(MeshAxis, XMVectorSet(0, 0.5f, 0, 0));
		m_Object3D_3DGizmoTranslationY->Create(MeshAxis);
	}
	
	if (!m_Object3D_3DGizmoTranslationZ)
	{
		m_Object3D_3DGizmoTranslationZ = make_unique<CObject3D>("Gizmo", m_Device.Get(), m_DeviceContext.Get(), this);
		SMesh MeshAxis{ GenerateCylinder(K3DGizmoRadius, 1.0f, 16, ColorZ) };
		SMesh MeshCone{ GenerateCone(0, 0.1f, 0.5f, 16, ColorZ) };
		TranslateMesh(MeshCone, XMVectorSet(0, 0.5f, 0, 0));
		MeshAxis = MergeStaticMeshes(MeshAxis, MeshCone);
		TranslateMesh(MeshAxis, XMVectorSet(0, 0.5f, 0, 0));
		m_Object3D_3DGizmoTranslationZ->Create(MeshAxis);
		m_Object3D_3DGizmoTranslationZ->ComponentTransform.Pitch = XM_PIDIV2;
	}

	if (!m_Object3D_3DGizmoScalingX)
	{
		m_Object3D_3DGizmoScalingX = make_unique<CObject3D>("Gizmo", m_Device.Get(), m_DeviceContext.Get(), this);
		SMesh MeshAxis{ GenerateCylinder(K3DGizmoRadius, 1.0f, 16, ColorX) };
		SMesh MeshCube{ GenerateCube(ColorX) };
		ScaleMesh(MeshCube, XMVectorSet(0.2f, 0.2f, 0.2f, 0));
		TranslateMesh(MeshCube, XMVectorSet(0, 0.5f, 0, 0));
		MeshAxis = MergeStaticMeshes(MeshAxis, MeshCube);
		TranslateMesh(MeshAxis, XMVectorSet(0, 0.5f, 0, 0));
		m_Object3D_3DGizmoScalingX->Create(MeshAxis);
		m_Object3D_3DGizmoScalingX->ComponentTransform.Roll = -XM_PIDIV2;
	}

	if (!m_Object3D_3DGizmoScalingY)
	{
		m_Object3D_3DGizmoScalingY = make_unique<CObject3D>("Gizmo", m_Device.Get(), m_DeviceContext.Get(), this);
		SMesh MeshAxis{ GenerateCylinder(K3DGizmoRadius, 1.0f, 16, ColorY) };
		SMesh MeshCube{ GenerateCube(ColorY) };
		ScaleMesh(MeshCube, XMVectorSet(0.2f, 0.2f, 0.2f, 0));
		TranslateMesh(MeshCube, XMVectorSet(0, 0.5f, 0, 0));
		MeshAxis = MergeStaticMeshes(MeshAxis, MeshCube);
		TranslateMesh(MeshAxis, XMVectorSet(0, 0.5f, 0, 0));
		m_Object3D_3DGizmoScalingY->Create(MeshAxis);
	}

	if (!m_Object3D_3DGizmoScalingZ)
	{
		m_Object3D_3DGizmoScalingZ = make_unique<CObject3D>("Gizmo", m_Device.Get(), m_DeviceContext.Get(), this);
		SMesh MeshAxis{ GenerateCylinder(K3DGizmoRadius, 1.0f, 16, ColorZ) };
		SMesh MeshCube{ GenerateCube(ColorZ) };
		ScaleMesh(MeshCube, XMVectorSet(0.2f, 0.2f, 0.2f, 0));
		TranslateMesh(MeshCube, XMVectorSet(0, 0.5f, 0, 0));
		MeshAxis = MergeStaticMeshes(MeshAxis, MeshCube);
		TranslateMesh(MeshAxis, XMVectorSet(0, 0.5f, 0, 0));
		m_Object3D_3DGizmoScalingZ->Create(MeshAxis);
		m_Object3D_3DGizmoScalingZ->ComponentTransform.Pitch = XM_PIDIV2;
	}

	m_Object3D_3DGizmoRotationPitch->ComponentRender.PtrVS =
		m_Object3D_3DGizmoRotationYaw->ComponentRender.PtrVS = m_Object3D_3DGizmoRotationRoll->ComponentRender.PtrVS =
		m_Object3D_3DGizmoTranslationX->ComponentRender.PtrVS =
		m_Object3D_3DGizmoTranslationY->ComponentRender.PtrVS = m_Object3D_3DGizmoTranslationZ->ComponentRender.PtrVS =
		m_Object3D_3DGizmoScalingX->ComponentRender.PtrVS =
		m_Object3D_3DGizmoScalingY->ComponentRender.PtrVS = m_Object3D_3DGizmoScalingZ->ComponentRender.PtrVS = m_VSGizmo.get();

	m_Object3D_3DGizmoRotationPitch->ComponentRender.PtrPS =
		m_Object3D_3DGizmoRotationYaw->ComponentRender.PtrPS = m_Object3D_3DGizmoRotationRoll->ComponentRender.PtrPS =
		m_Object3D_3DGizmoTranslationX->ComponentRender.PtrPS =
		m_Object3D_3DGizmoTranslationY->ComponentRender.PtrPS = m_Object3D_3DGizmoTranslationZ->ComponentRender.PtrPS =
		m_Object3D_3DGizmoScalingX->ComponentRender.PtrPS = 
		m_Object3D_3DGizmoScalingY->ComponentRender.PtrPS = m_Object3D_3DGizmoScalingZ->ComponentRender.PtrPS = m_PSGizmo.get();
}

void CGame::CreateScreenQuadVertexBuffer()
{
	D3D11_BUFFER_DESC BufferDesc{};
	BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	BufferDesc.ByteWidth = static_cast<UINT>(sizeof(SScreenQuadVertex) * m_vScreenQuadVertices.size());
	BufferDesc.CPUAccessFlags = 0;
	BufferDesc.MiscFlags = 0;
	BufferDesc.StructureByteStride = 0;
	BufferDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SUBRESOURCE_DATA SubresourceData{};
	SubresourceData.pSysMem = &m_vScreenQuadVertices[0];
	m_Device->CreateBuffer(&BufferDesc, &SubresourceData, &m_ScreenQuadVertexBuffer);
}

void CGame::SetProjectionMatrices(float FOV, float NearZ, float FarZ)
{
	m_NearZ = NearZ;
	m_FarZ = FarZ;

	m_MatrixProjection = XMMatrixPerspectiveFovLH(FOV, m_WindowSize.x / m_WindowSize.y, m_NearZ, m_FarZ);
	m_MatrixProjection2D = XMMatrixOrthographicLH(m_WindowSize.x, m_WindowSize.y, 0.0f, 1.0f);
}

void CGame::SetRenderingFlags(EFlagsRendering Flags)
{
	m_eFlagsRendering = Flags;
}

void CGame::ToggleGameRenderingFlags(EFlagsRendering Flags)
{
	m_eFlagsRendering ^= Flags;
}

void CGame::Set3DGizmoMode(E3DGizmoMode Mode)
{
	m_e3DGizmoMode = Mode;
}

void CGame::SetUniversalRSState()
{
	switch (m_eRasterizerState)
	{
	case ERasterizerState::CullNone:
		m_DeviceContext->RSSetState(m_CommonStates->CullNone());
		break;
	case ERasterizerState::CullClockwise:
		m_DeviceContext->RSSetState(m_CommonStates->CullClockwise());
		break;
	case ERasterizerState::CullCounterClockwise:
		m_DeviceContext->RSSetState(m_CommonStates->CullCounterClockwise());
		break;
	case ERasterizerState::WireFrame:
		m_DeviceContext->RSSetState(m_CommonStates->Wireframe());
		break;
	default:
		break;
	}
}

void CGame::SetUniversalbUseLighiting()
{
	if (EFLAG_HAS(m_eFlagsRendering, EFlagsRendering::UseLighting))
	{
		m_CBPSFlagsData.bUseLighting = TRUE;
	}
	m_CBPSFlags->Update();
}

void CGame::UpdateCBSpace(const XMMATRIX& World)
{
	m_CBSpaceWVPData.World = m_CBSpace2DData.World = XMMatrixTranspose(World);
	m_CBSpaceWVPData.ViewProjection = XMMatrixTranspose(m_MatrixView * m_MatrixProjection);
	m_CBSpaceWVPData.WVP = XMMatrixTranspose(World * m_MatrixView * m_MatrixProjection);
	m_CBSpaceWVP->Update();

	m_CBSpaceVPData.ViewProjection = XMMatrixTranspose(m_MatrixView * m_MatrixProjection);
	m_CBSpaceVP->Update();

	m_CBSpace2DData.Projection = XMMatrixTranspose(m_MatrixProjection2D);
	m_CBSpace2D->Update();
}

void CGame::UpdateCBTessFactorData(const CObject3D::SCBTessFactorData& Data)
{
	m_CBTessFactorData = Data;
	m_CBTessFactor->Update();
}

void CGame::UpdateCBDisplacementData(const CObject3D::SCBDisplacementData& Data)
{
	m_CBDisplacementData = Data;
	m_CBDisplacement->Update();
}

void CGame::UpdateCBMaterialData(const CMaterialData& MaterialData)
{
	m_CBMaterialData.AmbientColor = MaterialData.AmbientColor();
	m_CBMaterialData.DiffuseColor = MaterialData.DiffuseColor();
	m_CBMaterialData.SpecularColor = MaterialData.SpecularColor();
	m_CBMaterialData.SpecularExponent = MaterialData.SpecularExponent();
	m_CBMaterialData.SpecularIntensity = MaterialData.SpecularIntensity();
	m_CBMaterialData.Roughness = MaterialData.Roughness();
	m_CBMaterialData.Metalness = MaterialData.Metalness();

	uint32_t FlagsHasTexture{};
	FlagsHasTexture += MaterialData.HasTexture(STextureData::EType::DiffuseTexture) ? 0x01 : 0;
	FlagsHasTexture += MaterialData.HasTexture(STextureData::EType::NormalTexture) ? 0x02 : 0;
	FlagsHasTexture += MaterialData.HasTexture(STextureData::EType::OpacityTexture) ? 0x04 : 0;
	FlagsHasTexture += MaterialData.HasTexture(STextureData::EType::SpecularIntensityTexture) ? 0x08 : 0;
	FlagsHasTexture += MaterialData.HasTexture(STextureData::EType::RoughnessTexture) ? 0x10 : 0;
	FlagsHasTexture += MaterialData.HasTexture(STextureData::EType::MetalnessTexture) ? 0x20 : 0;
	FlagsHasTexture += MaterialData.HasTexture(STextureData::EType::AmbientOcclusionTexture) ? 0x40 : 0;
	// Displacement texture is usually not used in PS
	m_CBMaterialData.FlagsHasTexture = FlagsHasTexture;

	uint32_t FlagsIsTextureSRGB{};
	FlagsIsTextureSRGB += MaterialData.IsTextureSRGB(STextureData::EType::DiffuseTexture) ? 0x01 : 0;
	FlagsIsTextureSRGB += MaterialData.IsTextureSRGB(STextureData::EType::NormalTexture) ? 0x02 : 0;
	FlagsIsTextureSRGB += MaterialData.IsTextureSRGB(STextureData::EType::OpacityTexture) ? 0x04 : 0;
	FlagsIsTextureSRGB += MaterialData.IsTextureSRGB(STextureData::EType::SpecularIntensityTexture) ? 0x08 : 0;
	FlagsIsTextureSRGB += MaterialData.IsTextureSRGB(STextureData::EType::RoughnessTexture) ? 0x10 : 0;
	FlagsIsTextureSRGB += MaterialData.IsTextureSRGB(STextureData::EType::MetalnessTexture) ? 0x20 : 0;
	FlagsIsTextureSRGB += MaterialData.IsTextureSRGB(STextureData::EType::AmbientOcclusionTexture) ? 0x40 : 0;
	// Displacement texture is usually not used in PS
	if (m_EnvironmentTexture) FlagsIsTextureSRGB += m_EnvironmentTexture->IssRGB() ? 0x4000 : 0;
	if (m_IrradianceTexture) FlagsIsTextureSRGB += m_IrradianceTexture->IssRGB() ? 0x8000 : 0;

	m_CBMaterialData.FlagsIsTextureSRGB = FlagsIsTextureSRGB;
	m_CBMaterial->Update();
}

void CGame::CreateStaticSky(float ScalingFactor)
{
	m_SkyScalingFactor = ScalingFactor;

	m_Object3DSkySphere = make_unique<CObject3D>("SkySphere", m_Device.Get(), m_DeviceContext.Get(), this);
	//m_Object3DSkySphere->Create(GenerateSphere(KSkySphereSegmentCount, KSkySphereColorUp, KSkySphereColorBottom));
	m_Object3DSkySphere->Create(GenerateCubemapSphere(KSkySphereSegmentCount));
	m_Object3DSkySphere->ComponentTransform.Scaling = XMVectorSet(KSkyDistance, KSkyDistance, KSkyDistance, 0);
	m_Object3DSkySphere->ComponentRender.PtrVS = m_VSSky.get();
	m_Object3DSkySphere->ComponentRender.PtrPS = m_PSSky.get();
	m_Object3DSkySphere->ComponentPhysics.bIsPickable = false;
	m_Object3DSkySphere->eFlagsRendering = CObject3D::EFlagsRendering::NoCulling | CObject3D::EFlagsRendering::NoLighting;
}

void CGame::SetDirectionalLight(const XMVECTOR& LightSourcePosition, const XMFLOAT3& Color)
{
	m_CBLightData.DirectionalLightDirection = XMVector3Normalize(LightSourcePosition);
	m_CBLightData.DirectionalLightColor = Color;

	m_CBLight->Update();
}

void CGame::SetDirectionalLightDirection(const XMVECTOR& LightSourcePosition)
{
	m_CBLightData.DirectionalLightDirection = XMVector3Normalize(LightSourcePosition);

	m_CBLight->Update();
}

void CGame::SetDirectionalLightColor(const XMFLOAT3& Color)
{
	m_CBLightData.DirectionalLightColor = Color;

	m_CBLight->Update();
}

const XMVECTOR& CGame::GetDirectionalLightDirection() const
{
	return m_CBLightData.DirectionalLightDirection;
}

const XMFLOAT3& CGame::GetDirectionalLightColor() const
{
	return m_CBLightData.DirectionalLightColor;
}

void CGame::SetAmbientlLight(const XMFLOAT3& Color, float Intensity)
{
	m_CBLightData.AmbientLightColor = Color;
	m_CBLightData.AmbientLightIntensity = Intensity;

	m_CBLight->Update();
}

const XMFLOAT3& CGame::GetAmbientLightColor() const
{
	return m_CBLightData.AmbientLightColor;
}

float CGame::GetAmbientLightIntensity() const
{
	return m_CBLightData.AmbientLightIntensity;
}

void CGame::SetExposure(float Value)
{
	m_CBLightData.Exposure = Value;

	m_CBLight->Update();
}

float CGame::GetExposure()
{
	return m_CBLightData.Exposure;
}

bool CGame::InsertCamera(const string& Name)
{
	if (m_mapCameraNameToIndex.find(Name) != m_mapCameraNameToIndex.end())
	{
		MB_WARN(("이미 존재하는 이름입니다. (" + Name + ")").c_str(), "Camera 생성 실패");
		return false;
	}

	if (Name.size() >= KAssetNameMaxLength)
	{
		MB_WARN(("이름이 너무 깁니다. (" + Name + ")").c_str(), "Camera 생성 실패");
		return false;
	}
	else if (Name.size() == 0)
	{
		MB_WARN("이름은 공백일 수 없습니다.", "Camera 생성 실패");
		return false;
	}

	m_vCameras.emplace_back(make_unique<CCamera>(Name));
	m_mapCameraNameToIndex[Name] = m_vCameras.size() - 1;

	return true;
}

void CGame::DeleteCamera(const std::string& Name)
{
	if (Name == m_vCameras[0]->GetName()) return; // @important
	if (Name.length() == 0) return;
	if (m_mapCameraNameToIndex.find(Name) == m_mapCameraNameToIndex.end())
	{
		MB_WARN(("존재하지 않는 이름입니다. (" + Name + ")").c_str(), "Camera 삭제 실패");
		return;
	}

	size_t iCamera{ m_mapCameraNameToIndex[Name] };
	if (iCamera < m_vCameras.size() - 1)
	{
		const string& SwappedName{ m_vCameras.back()->GetName() };
		swap(m_vCameras[iCamera], m_vCameras.back());
		
		m_mapCameraNameToIndex[SwappedName] = iCamera;
	}

	if (IsAnyCameraSelected())
	{
		if (Name == GetSelectedCameraName())
		{
			DeselectCamera();
		}
	}

	m_vCameras.pop_back();
	m_mapCameraNameToIndex.erase(Name);

	m_PtrCurrentCamera = GetEditorCamera();
}

void CGame::ClearCameras()
{
	CCamera* EditorCamera{ GetEditorCamera() };
	CCamera::SCameraData EditorCameraData{ EditorCamera->GetData() };

	m_mapCameraNameToIndex.clear();
	m_vCameras.clear();

	CreateEditorCamera();
}

CCamera* CGame::GetCamera(const string& Name, bool bShowWarning)
{
	if (m_mapCameraNameToIndex.find(Name) == m_mapCameraNameToIndex.end())
	{
		if (bShowWarning) MB_WARN(("존재하지 않는 이름입니다. (" + Name + ")").c_str(), "Camera 얻어오기 실패");
		return nullptr;
	}
	return m_vCameras[m_mapCameraNameToIndex.at(Name)].get();
}

void CGame::CreateEditorCamera()
{
	if (GetEditorCamera(false)) return;

	assert(InsertCamera(u8"Editor Camera"));
	CCamera* EditorCamera{ GetEditorCamera() };
	EditorCamera->SetData(CCamera::SCameraData(CCamera::EType::FreeLook, XMVectorSet(0, 0, 0, 0), XMVectorSet(0, 0, 1, 0)));
	EditorCamera->SetEyePosition(XMVectorSet(0, 2, 0, 1));

	m_PtrCurrentCamera = EditorCamera;
}

CCamera* CGame::GetEditorCamera(bool bShowWarning)
{
	return GetCamera(u8"Editor Camera", bShowWarning);
}

CShader* CGame::AddCustomShader()
{
	m_vShaders.emplace_back(make_unique<CShader>(m_Device.Get(), m_DeviceContext.Get()));
	return m_vShaders.back().get();
}

CShader* CGame::GetCustomShader(size_t Index) const
{
	assert(Index < m_vShaders.size());
	return m_vShaders[Index].get();
}

CShader* CGame::GetBaseShader(EBaseShader eShader) const
{
	CShader* Result{};
	switch (eShader)
	{
	case EBaseShader::VSBase:
		Result = m_VSBase.get();
		break;
	case EBaseShader::VSSky:
		Result = m_VSSky.get();
		break;
	case EBaseShader::VSLine:
		Result = m_VSLine.get();
		break;
	case EBaseShader::VSGizmo:
		Result = m_VSGizmo.get();
		break;
	case EBaseShader::VSScreenQuad:
		Result = m_VSScreenQuad.get();
		break;
	case EBaseShader::VSBase2D:
		Result = m_VSBase2D.get();
		break;
	case EBaseShader::VSNull:
		Result = m_VSNull.get();
		break;
	case EBaseShader::HSTriOdd:
		Result = m_HSTriOdd.get();
		break;
	case EBaseShader::HSTriEven:
		Result = m_HSTriEven.get();
		break;
	case EBaseShader::HSTriInteger:
		Result = m_HSTriInteger.get();
		break;
	case EBaseShader::HSQuadSphere:
		Result = m_HSQuadSphere.get();
		break;
	case EBaseShader::DSTri:
		Result = m_DSTri.get();
		break;
	case EBaseShader::DSQuadSphere:
		Result = m_DSQuadSphere.get();
		break;
	case EBaseShader::GSNormal:
		Result = m_GSNormal.get();
		break;
	case EBaseShader::PSBase:
		Result = m_PSBase.get();
		break;
	case EBaseShader::PSVertexColor:
		Result = m_PSVertexColor.get();
		break;
	case EBaseShader::PSLine:
		Result = m_PSLine.get();
		break;
	case EBaseShader::PSGizmo:
		Result = m_PSGizmo.get();
		break;
	case EBaseShader::PSScreenQuad:
		Result = m_PSScreenQuad.get();
		break;
	case EBaseShader::PSSky:
		Result = m_PSSky.get();
		break;
	case EBaseShader::PSBase2D:
		Result = m_PSBase2D.get();
		break;
	case EBaseShader::PSTest:
		Result = m_PSTest.get();
		break;
	default:
		assert(Result);
		break;
	}

	return Result;
}

bool CGame::InsertObject3D(const string& Name)
{
	if (m_mapObject3DNameToIndex.find(Name) != m_mapObject3DNameToIndex.end())
	{
		MB_WARN(("이미 존재하는 이름입니다. (" + Name + ")").c_str(), "Object3D 생성 실패");
		return false;
	}

	if (Name.size() >= KAssetNameMaxLength)
	{
		MB_WARN(("이름이 너무 깁니다. (" + Name + ")").c_str(), "Object3D 생성 실패");
		return false;
	}
	else if (Name.size() == 0)
	{
		MB_WARN("이름은 공백일 수 없습니다.", "Object3D 생성 실패");
		return false;
	}

	m_vObject3Ds.emplace_back(make_unique<CObject3D>(Name, m_Device.Get(), m_DeviceContext.Get(), this));
	m_vObject3Ds.back()->ComponentRender.PtrVS = m_VSBase.get();
	m_vObject3Ds.back()->ComponentRender.PtrPS = m_PSBase.get();

	m_mapObject3DNameToIndex[Name] = m_vObject3Ds.size() - 1;

	return true;
}

void CGame::DeleteObject3D(const string& Name)
{
	if (!m_vObject3Ds.size()) return;
	if (Name.length() == 0) return;
	if (m_mapObject3DNameToIndex.find(Name) == m_mapObject3DNameToIndex.end())
	{
		MB_WARN(("존재하지 않는 이름입니다. (" + Name + ")").c_str(), "Object3D 삭제 실패");
		return;
	}

	size_t iObject3D{ m_mapObject3DNameToIndex[Name] };
	if (iObject3D < m_vObject3Ds.size() - 1)
	{
		const string& SwappedName{ m_vObject3Ds.back()->GetName() };
		swap(m_vObject3Ds[iObject3D], m_vObject3Ds.back());

		m_mapObject3DNameToIndex[SwappedName] = iObject3D;
	}

	if (IsAnyObject3DSelected())
	{
		if (Name == GetSelectedObject3DName())
		{
			DeselectObject3D();
		}
	}

	m_mapObject3DNameToIndex.erase(Name);
	m_vObject3Ds.pop_back();
}

void CGame::ClearObject3Ds()
{
	m_mapObject3DNameToIndex.clear();
	m_vObject3Ds.clear();

	m_PtrSelectedObject3D = nullptr;
}

CObject3D* CGame::GetObject3D(const string& Name, bool bShowWarning) const
{
	if (m_mapObject3DNameToIndex.find(Name) == m_mapObject3DNameToIndex.end())
	{
		if (bShowWarning) MB_WARN(("존재하지 않는 이름입니다. (" + Name + ")").c_str(), "Object3D 얻어오기 실패");
		return nullptr;
	}
	return m_vObject3Ds[m_mapObject3DNameToIndex.at(Name)].get();
}

bool CGame::InsertObject3DLine(const string& Name, bool bShowWarning)
{
	if (m_mapObject3DLineNameToIndex.find(Name) != m_mapObject3DLineNameToIndex.end())
	{
		if (bShowWarning) MB_WARN(("이미 존재하는 이름입니다. (" + Name + ")").c_str(), "Object3DLine 생성 실패");
		return false;
	}

	if (Name.size() >= KAssetNameMaxLength)
	{
		if (bShowWarning) MB_WARN(("이름이 너무 깁니다. (" + Name + ")").c_str(), "Object3DLine 생성 실패");
		return false;
	}
	else if (Name.size() == 0)
	{
		if (bShowWarning) MB_WARN("이름은 공백일 수 없습니다.", "Object3DLine 생성 실패");
		return false;
	}

	m_vObject3DLines.emplace_back(make_unique<CObject3DLine>(Name, m_Device.Get(), m_DeviceContext.Get()));
	m_mapObject3DLineNameToIndex[Name] = m_vObject3DLines.size() - 1;

	return true;
}

void CGame::ClearObject3DLines()
{
	m_mapObject3DLineNameToIndex.clear();
	m_vObject3DLines.clear();
}

CObject3DLine* CGame::GetObject3DLine(const string& Name, bool bShowWarning) const
{
	if (m_mapObject3DLineNameToIndex.find(Name) == m_mapObject3DLineNameToIndex.end())
	{
		if (bShowWarning) MB_WARN(("존재하지 않는 이름입니다. (" + Name + ")").c_str(), "Object3DLine 얻어오기 실패");
		return nullptr;
	}
	return m_vObject3DLines[m_mapObject3DLineNameToIndex.at(Name)].get();
}

bool CGame::InsertObject2D(const string& Name)
{
	if (m_mapObject2DNameToIndex.find(Name) != m_mapObject2DNameToIndex.end())
	{
		MB_WARN(("이미 존재하는 이름입니다. (" + Name + ")").c_str(), "Object2D 생성 실패");
		return false;
	}

	if (Name.size() >= KAssetNameMaxLength)
	{
		MB_WARN(("이름이 너무 깁니다. (" + Name + ")").c_str(), "Object2D 생성 실패");
		return false;
	}
	else if (Name.size() == 0)
	{
		MB_WARN("이름은 공백일 수 없습니다.", "Object2D 생성 실패");
		return false;
	}

	m_vObject2Ds.emplace_back(make_unique<CObject2D>(Name, m_Device.Get(), m_DeviceContext.Get()));
	m_mapObject2DNameToIndex[Name] = m_vObject2Ds.size() - 1;

	return true;
}

void CGame::DeleteObject2D(const std::string& Name)
{
	if (!m_vObject2Ds.size()) return;
	if (Name.length() == 0) return;
	if (m_mapObject2DNameToIndex.find(Name) == m_mapObject2DNameToIndex.end())
	{
		MB_WARN(("존재하지 않는 이름입니다. (" + Name + ")").c_str(), "Object2D 삭제 실패");
		return;
	}

	size_t iObject2D{ m_mapObject2DNameToIndex[Name] };
	if (iObject2D < m_vObject2Ds.size() - 1)
	{
		const string& SwappedName{ m_vObject2Ds.back()->GetName() };
		swap(m_vObject2Ds[iObject2D], m_vObject2Ds.back());

		m_mapObject2DNameToIndex[SwappedName] = iObject2D;
	}

	if (IsAnyObject2DSelected())
	{
		if (Name == GetSelectedObject2DName())
		{
			DeselectObject2D();
		}
	}

	m_vObject2Ds.pop_back();
	m_mapObject2DNameToIndex.erase(Name);
}

void CGame::ClearObject2Ds()
{
	m_mapObject2DNameToIndex.clear();
	m_vObject2Ds.clear();
}

CObject2D* CGame::GetObject2D(const string& Name, bool bShowWarning) const
{
	if (m_mapObject2DNameToIndex.find(Name) == m_mapObject2DNameToIndex.end())
	{
		if (bShowWarning) MB_WARN(("존재하지 않는 이름입니다. (" + Name + ")").c_str(), "Object2D 얻어오기 실패");
		return nullptr;
	}
	return m_vObject2Ds[m_mapObject2DNameToIndex.at(Name)].get();
}

bool CGame::InsertMaterial(const string& Name, bool bShowWarning)
{
	if (m_mapMaterialNameToIndex.find(Name) != m_mapMaterialNameToIndex.end())
	{
		if (bShowWarning) MB_WARN(("이미 존재하는 이름입니다. (" + Name + ")").c_str(), "Material 생성 실패");
		return false;
	}

	if (Name.size() >= KAssetNameMaxLength)
	{
		if (bShowWarning) MB_WARN(("이름이 너무 깁니다. (" + Name + ")").c_str(), "Material 생성 실패");
		return false;
	}
	else if (Name.size() == 0)
	{
		if (bShowWarning) MB_WARN("이름은 공백일 수 없습니다.", "Material 생성 실패");
		return false;
	}

	m_vMaterialData.emplace_back();
	m_vMaterialData.back().Name(Name);
	m_vMaterialTextureSets.emplace_back();
	
	m_mapMaterialNameToIndex[Name] = m_vMaterialData.size() - 1;
	
	return true;
}

bool CGame::InsertMaterialCreateTextures(const CMaterialData& MaterialData, bool bShowWarning)
{
	if (InsertMaterial(MaterialData.Name(), bShowWarning))
	{
		CMaterialData* const Material{ GetMaterial(MaterialData.Name()) };
		*Material = MaterialData; // copy it!

		CreateMaterialTextures(*Material);
		return true;
	}
	return false;
}

void CGame::DeleteMaterial(const std::string& Name)
{
	if (!m_vMaterialData.size()) return;
	if (Name.length() == 0) return;
	if (m_mapMaterialNameToIndex.find(Name) == m_mapMaterialNameToIndex.end())
	{
		MB_WARN(("존재하지 않는 이름입니다. (" + Name + ")").c_str(), "Material 삭제 실패");
		return;
	}

	size_t iMaterial{ m_mapMaterialNameToIndex[Name] };
	if (iMaterial < m_vMaterialData.size() - 1)
	{
		const string& SwappedName{ m_vMaterialData.back().Name() };

		swap(m_vMaterialData[iMaterial], m_vMaterialData.back());
		swap(m_vMaterialTextureSets[iMaterial], m_vMaterialTextureSets.back());

		m_mapMaterialNameToIndex[SwappedName] = iMaterial;
	}

	m_mapMaterialNameToIndex.erase(Name);
	m_vMaterialData.pop_back();
	m_vMaterialTextureSets.pop_back();
}

void CGame::CreateMaterialTextures(CMaterialData& MaterialData)
{
	size_t iMaterial{ m_mapMaterialNameToIndex[MaterialData.Name()] };
	m_vMaterialTextureSets[iMaterial] = make_unique<CMaterialTextureSet>(m_Device.Get(), m_DeviceContext.Get());
	m_vMaterialTextureSets[iMaterial]->CreateTextures(MaterialData);
}

CMaterialData* CGame::GetMaterial(const string& Name, bool bShowWarning)
{
	if (m_mapMaterialNameToIndex.find(Name) == m_mapMaterialNameToIndex.end())
	{
		if (bShowWarning) MB_WARN(("존재하지 않는 이름입니다. (" + Name + ")").c_str(), "Material 얻어오기 실패");
		return nullptr;
	}
	return &m_vMaterialData[m_mapMaterialNameToIndex.at(Name)];
}

CMaterialTextureSet* CGame::GetMaterialTextureSet(const std::string& Name, bool bShowWarning)
{
	if (m_mapMaterialNameToIndex.find(Name) == m_mapMaterialNameToIndex.end())
	{
		if (bShowWarning) MB_WARN(("존재하지 않는 이름입니다. (" + Name + ")").c_str(), "Material 얻어오기 실패");
		return nullptr;
	}
	return m_vMaterialTextureSets[m_mapMaterialNameToIndex.at(Name)].get();
}

void CGame::ClearMaterials()
{
	m_vMaterialData.clear();
	m_vMaterialTextureSets.clear();
	m_mapMaterialNameToIndex.clear();
}

size_t CGame::GetMaterialCount() const
{
	return m_vMaterialData.size();
}

bool CGame::ChangeMaterialName(const string& OldName, const string& NewName)
{
	if (GetMaterial(NewName, false))
	{
		MB_WARN(("[" + NewName + "] 은 이미 존재하는 이름입니다. 다른 이름을 골라주세요.").c_str(), "재질 이름 충돌");
		return false;
	}

	size_t iMaterial{ m_mapMaterialNameToIndex[OldName] };
	CMaterialData* Material{ GetMaterial(OldName) };
	auto a =m_mapMaterialNameToIndex.find(OldName);
	
	m_mapMaterialNameToIndex.erase(OldName);
	m_mapMaterialNameToIndex.insert(make_pair(NewName, iMaterial));

	Material->Name(NewName);

	return true;
}

ID3D11ShaderResourceView* CGame::GetMaterialTextureSRV(STextureData::EType eType, const string& Name) const
{
	assert(m_mapMaterialNameToIndex.find(Name) != m_mapMaterialNameToIndex.end());
	size_t iMaterial{ m_mapMaterialNameToIndex.at(Name) };

	if (m_vMaterialTextureSets[iMaterial]) return m_vMaterialTextureSets[iMaterial]->GetTextureSRV(eType);
	return nullptr;
}

void CGame::NotifyMouseLeftDown()
{
	m_bLeftButtonPressedOnce = true;
}

void CGame::NotifyMouseLeftUp()
{
	m_bLeftButtonPressedOnce = false;
}

bool CGame::Pick()
{
	CastPickingRay();

	UpdatePickingRay();

	PickBoundingSphere();

	PickTriangle();

	if (m_PtrPickedObject3D) return true;
	return false;
}

const string& CGame::GetPickedObject3DName() const
{
	assert(m_PtrPickedObject3D);
	return m_PtrPickedObject3D->GetName();
}

void CGame::SelectObject3D(const string& Name)
{
	m_PtrSelectedObject3D = GetObject3D(Name);
	if (m_PtrSelectedObject3D)
	{
		const XMVECTOR& BSTransaltion{ m_PtrSelectedObject3D->ComponentPhysics.BoundingSphere.CenterOffset };
		const XMVECTOR KObjectTranslation{ m_PtrSelectedObject3D->ComponentTransform.Translation + BSTransaltion };
		m_CapturedGizmoTranslation = KObjectTranslation;
	}
}

void CGame::DeselectObject3D()
{
	m_PtrSelectedObject3D = nullptr;
}

bool CGame::IsAnyObject3DSelected() const
{
	return (m_PtrSelectedObject3D) ? true : false;
}

CObject3D* CGame::GetSelectedObject3D()
{
	return m_PtrSelectedObject3D;
}

const string& CGame::GetSelectedObject3DName() const
{
	assert(m_PtrSelectedObject3D);
	return m_PtrSelectedObject3D->GetName();
}

void CGame::SelectObject2D(const string& Name)
{
	m_PtrSelectedObject2D = GetObject2D(Name);
}

void CGame::DeselectObject2D()
{
	m_PtrSelectedObject2D = nullptr;
}

bool CGame::IsAnyObject2DSelected() const
{
	return (m_PtrSelectedObject2D) ? true : false;
}

CObject2D* CGame::GetSelectedObject2D()
{
	return m_PtrSelectedObject2D;
}

const string& CGame::GetSelectedObject2DName() const
{
	assert(m_PtrSelectedObject2D);
	return m_PtrSelectedObject2D->GetName();
}

void CGame::SelectCamera(const string& Name)
{
	m_PtrSelectedCamera = GetCamera(Name);
}

void CGame::DeselectCamera()
{
	m_PtrSelectedCamera = nullptr;
}

bool CGame::IsAnyCameraSelected() const
{
	return (m_PtrSelectedCamera) ? true : false;
}

CCamera* CGame::GetSelectedCamera()
{
	return m_PtrSelectedCamera;
}

const string& CGame::GetSelectedCameraName() const
{
	assert(m_PtrSelectedCamera);
	return m_PtrSelectedCamera->GetName();
}

CCamera* CGame::GetCurrentCamera()
{
	return m_PtrCurrentCamera;
}

void CGame::Select3DGizmos()
{
	if (EFLAG_HAS_NO(m_eFlagsRendering, EFlagsRendering::Use3DGizmos)) return;
	if (!IsAnyObject3DSelected()) return;

	XMVECTOR* pTranslation{ &m_PtrSelectedObject3D->ComponentTransform.Translation };
	XMVECTOR* pScaling{ &m_PtrSelectedObject3D->ComponentTransform.Scaling };
	float* pPitch{ &m_PtrSelectedObject3D->ComponentTransform.Pitch };
	float* pYaw{ &m_PtrSelectedObject3D->ComponentTransform.Yaw };
	float* pRoll{ &m_PtrSelectedObject3D->ComponentTransform.Roll };

	// Calculate scalar IAW the distance from the camera
	m_3DGizmoDistanceScalar = XMVectorGetX(XMVector3Length(m_PtrCurrentCamera->GetEyePosition() - *pTranslation)) * 0.1f;
	m_3DGizmoDistanceScalar = pow(m_3DGizmoDistanceScalar, 0.7f);

	// Translate 3D gizmos
	const XMVECTOR& BSTransaltion{ m_PtrSelectedObject3D->ComponentPhysics.BoundingSphere.CenterOffset };
	const XMVECTOR KGizmoTranslation{ *pTranslation + BSTransaltion };
	m_Object3D_3DGizmoTranslationX->ComponentTransform.Translation =
		m_Object3D_3DGizmoTranslationY->ComponentTransform.Translation = m_Object3D_3DGizmoTranslationZ->ComponentTransform.Translation =
		m_Object3D_3DGizmoRotationPitch->ComponentTransform.Translation =
		m_Object3D_3DGizmoRotationYaw->ComponentTransform.Translation = m_Object3D_3DGizmoRotationRoll->ComponentTransform.Translation =
		m_Object3D_3DGizmoScalingX->ComponentTransform.Translation =
		m_Object3D_3DGizmoScalingY->ComponentTransform.Translation = m_Object3D_3DGizmoScalingZ->ComponentTransform.Translation = KGizmoTranslation;

	if (IsGizmoSelected())
	{
		int DeltaX{ m_CapturedMouseState.x - m_PrevCapturedMouseX };
		int DeltaY{ m_CapturedMouseState.y - m_PrevCapturedMouseY };
		int DeltaSum{ DeltaY - DeltaX };

		float DistanceObejctCamera{ abs(XMVectorGetX(m_CapturedGizmoTranslation - GetCurrentCamera()->GetEyePosition())) };
		if (DistanceObejctCamera < K3DGizmoCameraDistanceThreshold) DistanceObejctCamera = K3DGizmoCameraDistanceThreshold;
		const float KDeltaFactor{ pow(DistanceObejctCamera, K3DGizmoDistanceFactorExponent) };

		float TranslationX{ XMVectorGetX(*pTranslation) };
		float TranslationY{ XMVectorGetY(*pTranslation) };
		float TranslationZ{ XMVectorGetZ(*pTranslation) };
		float ScalingX{ XMVectorGetX(*pScaling) };
		float ScalingY{ XMVectorGetY(*pScaling) };
		float ScalingZ{ XMVectorGetZ(*pScaling) };

		switch (m_e3DGizmoMode)
		{
		case E3DGizmoMode::Translation:
			switch (m_e3DGizmoSelectedAxis)
			{
			case E3DGizmoAxis::AxisX:
				*pTranslation = XMVectorSetX(*pTranslation, TranslationX - DeltaSum * KTranslationDelta * KDeltaFactor);
				break;
			case E3DGizmoAxis::AxisY:
				*pTranslation = XMVectorSetY(*pTranslation, TranslationY - DeltaSum * KTranslationDelta * KDeltaFactor);
				break;
			case E3DGizmoAxis::AxisZ:
				*pTranslation = XMVectorSetZ(*pTranslation, TranslationZ - DeltaSum * KTranslationDelta * KDeltaFactor);
				break;
			default:
				break;
			}
			break;
		case E3DGizmoMode::Rotation:
			switch (m_e3DGizmoSelectedAxis)
			{
			case E3DGizmoAxis::AxisX:
				*pPitch -= DeltaSum * KRotation360To2PI * KRotationDelta * KDeltaFactor;
				break;
			case E3DGizmoAxis::AxisY:
				*pYaw -= DeltaSum * KRotation360To2PI * KRotationDelta * KDeltaFactor;
				break;
			case E3DGizmoAxis::AxisZ:
				*pRoll -= DeltaSum * KRotation360To2PI * KRotationDelta * KDeltaFactor;
				break;
			default:
				break;
			}
			break;
		case E3DGizmoMode::Scaling:
		{
			switch (m_e3DGizmoSelectedAxis)
			{
			case E3DGizmoAxis::AxisX:
				*pScaling = XMVectorSetX(*pScaling, ScalingX - DeltaSum * KScalingDelta * KDeltaFactor);
				break;
			case E3DGizmoAxis::AxisY:
				*pScaling = XMVectorSetY(*pScaling, ScalingY - DeltaSum * KScalingDelta * KDeltaFactor);
				break;
			case E3DGizmoAxis::AxisZ:
				*pScaling = XMVectorSetZ(*pScaling, ScalingZ - DeltaSum * KScalingDelta * KDeltaFactor);
				break;
			default:
				break;
			}
		}
			break;
		default:
			break;
		}
		m_PtrSelectedObject3D->UpdateWorldMatrix();
	}
	else
	{
		// Gizmo is not selected.
		CastPickingRay();

		switch (m_e3DGizmoMode)
		{
		case E3DGizmoMode::Translation:
			m_bIsGizmoHovered = true;
			if (ShouldSelectTranslationScalingGizmo(m_Object3D_3DGizmoTranslationX.get(), E3DGizmoAxis::AxisX))
			{
				m_e3DGizmoSelectedAxis = E3DGizmoAxis::AxisX;
			}
			else if (ShouldSelectTranslationScalingGizmo(m_Object3D_3DGizmoTranslationY.get(), E3DGizmoAxis::AxisY))
			{
				m_e3DGizmoSelectedAxis = E3DGizmoAxis::AxisY;
			}
			else if (ShouldSelectTranslationScalingGizmo(m_Object3D_3DGizmoTranslationZ.get(), E3DGizmoAxis::AxisZ))
			{
				m_e3DGizmoSelectedAxis = E3DGizmoAxis::AxisZ;
			}
			else
			{
				m_bIsGizmoHovered = false;
			}
			break;
		case E3DGizmoMode::Rotation:
			m_bIsGizmoHovered = true;
			if (ShouldSelectRotationGizmo(m_Object3D_3DGizmoRotationPitch.get(), E3DGizmoAxis::AxisX))
			{
				m_e3DGizmoSelectedAxis = E3DGizmoAxis::AxisX;
			}
			else if (ShouldSelectRotationGizmo(m_Object3D_3DGizmoRotationYaw.get(), E3DGizmoAxis::AxisY))
			{
				m_e3DGizmoSelectedAxis = E3DGizmoAxis::AxisY;
			}
			else if (ShouldSelectRotationGizmo(m_Object3D_3DGizmoRotationRoll.get(), E3DGizmoAxis::AxisZ))
			{
				m_e3DGizmoSelectedAxis = E3DGizmoAxis::AxisZ;
			}
			else
			{
				m_bIsGizmoHovered = false;
			}
			break;
		case E3DGizmoMode::Scaling:
			m_bIsGizmoHovered = true;
			if (ShouldSelectTranslationScalingGizmo(m_Object3D_3DGizmoScalingX.get(), E3DGizmoAxis::AxisX))
			{
				m_e3DGizmoSelectedAxis = E3DGizmoAxis::AxisX;
			}
			else if (ShouldSelectTranslationScalingGizmo(m_Object3D_3DGizmoScalingY.get(), E3DGizmoAxis::AxisY))
			{
				m_e3DGizmoSelectedAxis = E3DGizmoAxis::AxisY;
			}
			else if (ShouldSelectTranslationScalingGizmo(m_Object3D_3DGizmoScalingZ.get(), E3DGizmoAxis::AxisZ))
			{
				m_e3DGizmoSelectedAxis = E3DGizmoAxis::AxisZ;
			}
			else
			{
				m_bIsGizmoHovered = false;
			}
			break;
		default:
			break;
		}

		if (m_bIsGizmoHovered && m_CapturedMouseState.leftButton)
		{
			m_bIsGizmoSelected = true;
		}
	}

	m_PrevCapturedMouseX = m_CapturedMouseState.x;
	m_PrevCapturedMouseY = m_CapturedMouseState.y;
}

void CGame::Deselect3DGizmos()
{
	m_bIsGizmoSelected = false;
}

bool CGame::IsGizmoHovered() const
{
	return m_bIsGizmoHovered;
}

bool CGame::IsGizmoSelected() const
{
	return m_bIsGizmoSelected;
}

bool CGame::ShouldSelectRotationGizmo(const CObject3D* const Gizmo, E3DGizmoAxis Axis)
{
	XMVECTOR PlaneNormal{};
	switch (Axis)
	{
	case E3DGizmoAxis::None:
		return false;
		break;
	case E3DGizmoAxis::AxisX:
		PlaneNormal = XMVectorSet(1, 0, 0, 0);
		break;
	case E3DGizmoAxis::AxisY:
		PlaneNormal = XMVectorSet(0, 1, 0, 0);
		break;
	case E3DGizmoAxis::AxisZ:
		PlaneNormal = XMVectorSet(0, 0, 1, 0);
		break;
	default:
		break;
	}

	if (IntersectRaySphere(m_PickingRayWorldSpaceOrigin, m_PickingRayWorldSpaceDirection,
		K3DGizmoSelectionRadius * m_3DGizmoDistanceScalar, Gizmo->ComponentTransform.Translation, nullptr))
	{
		XMVECTOR PlaneT{};
		if (IntersectRayPlane(m_PickingRayWorldSpaceOrigin, m_PickingRayWorldSpaceDirection, Gizmo->ComponentTransform.Translation, PlaneNormal, &PlaneT))
		{
			XMVECTOR PointOnPlane{ m_PickingRayWorldSpaceOrigin + PlaneT * m_PickingRayWorldSpaceDirection };

			float Dist{ XMVectorGetX(XMVector3Length(PointOnPlane - Gizmo->ComponentTransform.Translation)) };
			if (Dist >= K3DGizmoSelectionLowBoundary * m_3DGizmoDistanceScalar && Dist <= K3DGizmoSelectionHighBoundary * m_3DGizmoDistanceScalar) return true;
		}
	}
	return false;
}

bool CGame::ShouldSelectTranslationScalingGizmo(const CObject3D* const Gizmo, E3DGizmoAxis Axis)
{
	static constexpr float KGizmoLengthFactor{ 1.1875f };
	static constexpr float KGizmoRaidus{ 0.05859375f };
	XMVECTOR CylinderSpaceRayOrigin{ m_PickingRayWorldSpaceOrigin - Gizmo->ComponentTransform.Translation };
	XMVECTOR CylinderSpaceRayDirection{ m_PickingRayWorldSpaceDirection };
	switch (Axis)
	{
	case E3DGizmoAxis::None:
		return false;
		break;
	case E3DGizmoAxis::AxisX:
		{
			XMMATRIX RotationMatrix{ XMMatrixRotationZ(XM_PIDIV2) };
			CylinderSpaceRayOrigin = XMVector3TransformCoord(CylinderSpaceRayOrigin, RotationMatrix);
			CylinderSpaceRayDirection = XMVector3TransformNormal(CylinderSpaceRayDirection, RotationMatrix);
			if (IntersectRayCylinder(CylinderSpaceRayOrigin, CylinderSpaceRayDirection, 
				KGizmoLengthFactor * m_3DGizmoDistanceScalar, KGizmoRaidus * m_3DGizmoDistanceScalar)) return true;
		}
		break;
	case E3DGizmoAxis::AxisY:
		if (IntersectRayCylinder(CylinderSpaceRayOrigin, CylinderSpaceRayDirection, 
			KGizmoLengthFactor * m_3DGizmoDistanceScalar, KGizmoRaidus * m_3DGizmoDistanceScalar)) return true;
		break;
	case E3DGizmoAxis::AxisZ:
		{
			XMMATRIX RotationMatrix{ XMMatrixRotationX(-XM_PIDIV2) };
			CylinderSpaceRayOrigin = XMVector3TransformCoord(CylinderSpaceRayOrigin, RotationMatrix);
			CylinderSpaceRayDirection = XMVector3TransformNormal(CylinderSpaceRayDirection, RotationMatrix);
			if (IntersectRayCylinder(CylinderSpaceRayOrigin, CylinderSpaceRayDirection, 
				KGizmoLengthFactor * m_3DGizmoDistanceScalar, KGizmoRaidus * m_3DGizmoDistanceScalar)) return true;
		}
		break;
	default:
		break;
	}
	return false;
}

void CGame::DeselectAll()
{
	DeselectObject3D();
	DeselectObject2D();
	DeselectCamera();
	Deselect3DGizmos();
}

void CGame::CastPickingRay()
{
	float ViewSpaceRayDirectionX{ (m_CapturedMouseState.x / (m_WindowSize.x / 2.0f) - 1.0f) / XMVectorGetX(m_MatrixProjection.r[0]) };
	float ViewSpaceRayDirectionY{ (-(m_CapturedMouseState.y / (m_WindowSize.y / 2.0f) - 1.0f)) / XMVectorGetY(m_MatrixProjection.r[1]) };
	static float ViewSpaceRayDirectionZ{ 1.0f };

	static XMVECTOR ViewSpaceRayOrigin{ XMVectorSet(0, 0, 0, 1) };
	XMVECTOR ViewSpaceRayDirection{ XMVectorSet(ViewSpaceRayDirectionX, ViewSpaceRayDirectionY, ViewSpaceRayDirectionZ, 0) };

	XMMATRIX MatrixViewInverse{ XMMatrixInverse(nullptr, m_MatrixView) };
	m_PickingRayWorldSpaceOrigin = XMVector3TransformCoord(ViewSpaceRayOrigin, MatrixViewInverse);
	m_PickingRayWorldSpaceDirection = XMVector3TransformNormal(ViewSpaceRayDirection, MatrixViewInverse);
}

void CGame::PickBoundingSphere()
{
	m_vObject3DPickingCandidates.clear();
	m_PtrPickedObject3D = nullptr;

	XMVECTOR T{ KVectorGreatest };
	for (auto& i : m_vObject3Ds)
	{
		auto* Object3D{ i.get() };
		if (Object3D->ComponentPhysics.bIsPickable)
		{
			XMVECTOR NewT{ KVectorGreatest };
			if (IntersectRaySphere(m_PickingRayWorldSpaceOrigin, m_PickingRayWorldSpaceDirection,
				Object3D->ComponentPhysics.BoundingSphere.Radius, Object3D->ComponentTransform.Translation + Object3D->ComponentPhysics.BoundingSphere.CenterOffset, &NewT))
			{
				m_vObject3DPickingCandidates.emplace_back(Object3D, NewT);
			}
		}
	}
}

bool CGame::PickTriangle()
{
	XMVECTOR T{ KVectorGreatest };
	if (m_PtrPickedObject3D == nullptr)
	{
		for (SObject3DPickingCandiate& Candidate : m_vObject3DPickingCandidates)
		{
			Candidate.bHasFailedPickingTest = true;
			XMMATRIX WorldMatrix{ Candidate.PtrObject3D->ComponentTransform.MatrixWorld };
			for (const SMesh& Mesh : Candidate.PtrObject3D->GetModel().vMeshes)
			{
				for (const STriangle& Triangle : Mesh.vTriangles)
				{
					XMVECTOR V0{ Mesh.vVertices[Triangle.I0].Position };
					XMVECTOR V1{ Mesh.vVertices[Triangle.I1].Position };
					XMVECTOR V2{ Mesh.vVertices[Triangle.I2].Position };
					V0 = XMVector3TransformCoord(V0, WorldMatrix);
					V1 = XMVector3TransformCoord(V1, WorldMatrix);
					V2 = XMVector3TransformCoord(V2, WorldMatrix);

					XMVECTOR NewT{};
					if (IntersectRayTriangle(m_PickingRayWorldSpaceOrigin, m_PickingRayWorldSpaceDirection, V0, V1, V2, &NewT))
					{
						if (XMVector3Less(NewT, T))
						{
							T = NewT;

							Candidate.bHasFailedPickingTest = false;
							Candidate.T = NewT;

							XMVECTOR N{ CalculateTriangleNormal(V0, V1, V2) };

							m_PickedTriangleV0 = V0 + N * 0.01f;
							m_PickedTriangleV1 = V1 + N * 0.01f;
							m_PickedTriangleV2 = V2 + N * 0.01f;

							continue;
						}
					}
				}
			}
		}

		vector<SObject3DPickingCandiate> vFilteredCandidates{};
		for (const SObject3DPickingCandiate& Candidate : m_vObject3DPickingCandidates)
		{
			if (Candidate.bHasFailedPickingTest == false) vFilteredCandidates.emplace_back(Candidate);
		}
		if (!vFilteredCandidates.empty())
		{
			XMVECTOR TCmp{ KVectorGreatest };
			for (const SObject3DPickingCandiate& FilteredCandidate : vFilteredCandidates)
			{
				if (XMVector3Less(FilteredCandidate.T, TCmp))
				{
					m_PtrPickedObject3D = FilteredCandidate.PtrObject3D;
				}
			}
			return true;
		}
	}
	return false;
}

void CGame::BeginRendering(const FLOAT* ClearColor)
{
	m_DeviceContext->OMSetRenderTargets(1, m_DeviceRTV.GetAddressOf(), m_DepthStencilView.Get());

	m_DeviceContext->ClearRenderTargetView(m_DeviceRTV.Get(), Colors::CornflowerBlue);
	m_DeviceContext->ClearDepthStencilView(m_DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	ID3D11SamplerState* LinearWrapSampler{ m_CommonStates->LinearWrap() };
	ID3D11SamplerState* LinearClampSampler{ m_CommonStates->LinearClamp() };
	m_DeviceContext->PSSetSamplers(0, 1, &LinearWrapSampler);
	m_DeviceContext->PSSetSamplers(1, 1, &LinearClampSampler);
	m_DeviceContext->DSSetSamplers(0, 1, &LinearWrapSampler); // @important: in order to use displacement mapping

	m_DeviceContext->OMSetBlendState(m_CommonStates->NonPremultiplied(), nullptr, 0xFFFFFFFF);

	SetUniversalRSState();

	XMVECTOR EyePosition{ m_PtrCurrentCamera->GetEyePosition() };
	XMVECTOR FocusPosition{ m_PtrCurrentCamera->GetFocusPosition() };
	XMVECTOR UpDirection{ m_PtrCurrentCamera->GetUpDirection() };
	m_MatrixView = XMMatrixLookAtLH(EyePosition, FocusPosition, UpDirection);

	if (m_EnvironmentTexture) m_EnvironmentTexture->Use();
	if (m_IrradianceTexture) m_IrradianceTexture->Use();
	if (m_PrefilteredRadianceTexture) m_PrefilteredRadianceTexture->Use();
	if (m_IntegratedBRDFTexture) m_IntegratedBRDFTexture->Use();
}

void CGame::Update()
{
	// Calculate time
	m_TimeNow = m_Clock.now().time_since_epoch().count();
	if (m_TimePrev == 0) m_TimePrev = m_TimeNow;
	m_DeltaTimeF = static_cast<float>((m_TimeNow - m_TimePrev) * 0.000'000'001);

	if (m_TimeNow > m_PreviousFrameTime + 1'000'000'000)
	{
		m_FPS = m_FrameCount;
		m_FrameCount = 0;
		m_PreviousFrameTime = m_TimeNow;
	}

	// Capture inputs
	m_CapturedKeyboardState = GetKeyState();
	m_CapturedMouseState = GetMouseState();

	// Process keyboard inputs
	if (m_CapturedKeyboardState.LeftAlt && m_CapturedKeyboardState.Q)
	{
		Destroy();
		return;
	}
	if (m_CapturedKeyboardState.Escape)
	{
		DeselectAll();
	}
	if (!ImGui::IsAnyItemActive())
	{
		if (m_CapturedKeyboardState.W)
		{
			m_PtrCurrentCamera->Move(CCamera::EMovementDirection::Forward, m_DeltaTimeF * m_CameraMovementFactor);
		}
		if (m_CapturedKeyboardState.S)
		{
			m_PtrCurrentCamera->Move(CCamera::EMovementDirection::Backward, m_DeltaTimeF * m_CameraMovementFactor);
		}
		if (m_CapturedKeyboardState.A  && !m_CapturedKeyboardState.LeftControl)
		{
			m_PtrCurrentCamera->Move(CCamera::EMovementDirection::Leftward, m_DeltaTimeF * m_CameraMovementFactor);
		}
		if (m_CapturedKeyboardState.D)
		{
			m_PtrCurrentCamera->Move(CCamera::EMovementDirection::Rightward, m_DeltaTimeF * m_CameraMovementFactor);
		}
		if (m_CapturedKeyboardState.D1)
		{
			Set3DGizmoMode(E3DGizmoMode::Translation);
		}
		if (m_CapturedKeyboardState.D2)
		{
			Set3DGizmoMode(E3DGizmoMode::Rotation);
		}
		if (m_CapturedKeyboardState.D3)
		{
			Set3DGizmoMode(E3DGizmoMode::Scaling);
		}
		if (m_CapturedKeyboardState.Delete)
		{
			// Object3D
			if (IsAnyObject3DSelected())
			{
				DeleteObject3D(GetSelectedObject3DName());
				DeselectObject3D();
			}
		}
	}

	// Process moue inputs
	static int PrevMouseX{ m_CapturedMouseState.x };
	static int PrevMouseY{ m_CapturedMouseState.y };
	if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
	{
		if (m_CapturedMouseState.rightButton) ImGui::SetWindowFocus(nullptr);

		if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow))
		{
			Select3DGizmos();
			
			if (m_bLeftButtonPressedOnce)
			{
				if (Pick() && !IsGizmoSelected())
				{
					DeselectAll();

					SelectObject3D(GetPickedObject3DName());
				}
				m_bLeftButtonPressedOnce = false;
			}

			if (!m_CapturedMouseState.leftButton) Deselect3DGizmos();
			if (m_CapturedMouseState.rightButton)
			{
				DeselectAll();
			}
		}

		if (m_CapturedMouseState.x != PrevMouseX || m_CapturedMouseState.y != PrevMouseY)
		{
			if (m_CapturedMouseState.middleButton)
			//if (m_CapturedMouseState.rightButton)
			{
				m_PtrCurrentCamera->Rotate(m_CapturedMouseState.x - PrevMouseX, m_CapturedMouseState.y - PrevMouseY, m_DeltaTimeF);
			}

			PrevMouseX = m_CapturedMouseState.x;
			PrevMouseY = m_CapturedMouseState.y;
		}
	}

	m_TimePrev = m_TimeNow;
	++m_FrameCount;
}

void CGame::Draw()
{
	if (m_IsDestroyed) return;

	m_CBEditorTimeData.NormalizedTime += m_DeltaTimeF;
	m_CBEditorTimeData.NormalizedTimeHalfSpeed += m_DeltaTimeF * 0.5f;
	if (m_CBEditorTimeData.NormalizedTime > 1.0f) m_CBEditorTimeData.NormalizedTime = 0.0f;
	if (m_CBEditorTimeData.NormalizedTimeHalfSpeed > 1.0f) m_CBEditorTimeData.NormalizedTimeHalfSpeed = 0.0f;
	m_CBEditorTime->Update();

	m_DeviceContext->RSSetViewports(1, &m_vViewports[0]);

	m_CBLightData.EyePosition = m_PtrCurrentCamera->GetEyePosition();
	m_CBLight->Update();

	m_CBPSFlagsData.EnvironmentTextureMipLevels = (m_EnvironmentTexture) ? m_EnvironmentTexture->GetMipLevels() : 0;
	m_CBPSFlagsData.PrefilteredRadianceTextureMipLevels = (m_PrefilteredRadianceTexture) ? m_PrefilteredRadianceTexture->GetMipLevels() : 0;
	m_CBPSFlagsData.bUsePhysicallyBasedRendering = EFLAG_HAS(m_eFlagsRendering, EFlagsRendering::UsePhysicallyBasedRendering);
	m_CBPSFlags->Update();

	if (EFLAG_HAS(m_eFlagsRendering, EFlagsRendering::DrawWireFrame))
	{
		m_eRasterizerState = ERasterizerState::WireFrame;
	}
	else
	{
		m_eRasterizerState = ERasterizerState::CullCounterClockwise;
	}

	if (EFLAG_HAS(m_eFlagsRendering, EFlagsRendering::DrawMiniAxes))
	{
		DrawMiniAxes();
	}

	if (EFLAG_HAS(m_eFlagsRendering, EFlagsRendering::DrawPickingData))
	{
		DrawPickingRay();

		DrawPickedTriangle();
	}

	DrawObject3DLines();

	DrawSky(m_DeltaTimeF);

	if (EFLAG_HAS(m_eFlagsRendering, EFlagsRendering::DrawNormals))
	{
		UpdateCBSpace();
		m_GSNormal->Use();
	}

	// Opaque Object3Ds
	for (auto& Object3D : m_vObject3Ds)
	{
		if (Object3D->ComponentRender.bIsTransparent) continue;

		UpdateObject3D(Object3D.get());
		DrawObject3D(Object3D.get());

		if (EFLAG_HAS(m_eFlagsRendering, EFlagsRendering::DrawBoundingSphere))
		{
			DrawObject3DBoundingSphere(Object3D.get());
		}
	}

	// Transparent Object3Ds
	for (auto& Object3D : m_vObject3Ds)
	{
		if (!Object3D->ComponentRender.bIsTransparent) continue;

		UpdateObject3D(Object3D.get());
		DrawObject3D(Object3D.get());

		if (EFLAG_HAS(m_eFlagsRendering, EFlagsRendering::DrawBoundingSphere))
		{
			DrawObject3DBoundingSphere(Object3D.get());
		}
	}

	m_DeviceContext->GSSetShader(nullptr, nullptr, 0);

	DrawObject2Ds();
}

void CGame::UpdateObject3D(CObject3D* const PtrObject3D)
{
	if (!PtrObject3D) return;

	PtrObject3D->UpdateWorldMatrix();
	UpdateCBSpace(PtrObject3D->ComponentTransform.MatrixWorld);

	SetUniversalbUseLighiting();

	m_CBPSFlagsData.bUseLighting = EFLAG_HAS_NO(PtrObject3D->eFlagsRendering, CObject3D::EFlagsRendering::NoLighting);
	m_CBPSFlagsData.bUseTexture = EFLAG_HAS_NO(PtrObject3D->eFlagsRendering, CObject3D::EFlagsRendering::NoTexture);
	m_CBPSFlags->Update();

	assert(PtrObject3D->ComponentRender.PtrVS);
	assert(PtrObject3D->ComponentRender.PtrPS);
	CShader* VS{ PtrObject3D->ComponentRender.PtrVS };
	CShader* PS{ PtrObject3D->ComponentRender.PtrPS };
	if (EFLAG_HAS(PtrObject3D->eFlagsRendering, CObject3D::EFlagsRendering::UseRawVertexColor))
	{
		PS = m_PSVertexColor.get();
	}

	VS->Use();
	PS->Use();
}

void CGame::DrawObject3D(const CObject3D* const PtrObject3D, bool bIgnoreInstances, bool bIgnoreOwnTexture)
{
	if (!PtrObject3D) return;

	if (PtrObject3D->ShouldTessellate())
	{
		UpdateCBTessFactorData(PtrObject3D->GetTessFactorData());
		UpdateCBDisplacementData(PtrObject3D->GetDisplacementData());

		if (PtrObject3D->IsPatches())
		{
			m_VSNull->Use();

			m_HSQuadSphere->Use();
			m_DSQuadSphere->Use();

			m_PSTest->Use();
		}
		else
		{
			switch (PtrObject3D->TessellationType())
			{
			case CObject3D::ETessellationType::FractionalOdd:
				m_HSTriOdd->Use();
				break;
			case CObject3D::ETessellationType::FractionalEven:
				m_HSTriEven->Use();
				break;
			case CObject3D::ETessellationType::Integer:
				m_HSTriInteger->Use();
				break;
			default:
				break;
			}
			
			m_DSTri->Use();
		}
	}

	if (EFLAG_HAS(PtrObject3D->eFlagsRendering, CObject3D::EFlagsRendering::NoCulling))
	{
		m_DeviceContext->RSSetState(m_CommonStates->CullNone());
	}
	else
	{
		SetUniversalRSState();
	}

	PtrObject3D->Draw(bIgnoreOwnTexture, bIgnoreInstances);

	if (PtrObject3D->ShouldTessellate())
	{
		m_DeviceContext->HSSetShader(nullptr, nullptr, 0);
		m_DeviceContext->DSSetShader(nullptr, nullptr, 0);
	}
}

void CGame::DrawObject3DBoundingSphere(const CObject3D* const PtrObject3D)
{
	m_VSBase->Use();

	XMMATRIX Translation{ XMMatrixTranslationFromVector(PtrObject3D->ComponentTransform.Translation + 
		PtrObject3D->ComponentPhysics.BoundingSphere.CenterOffset) };
	XMMATRIX Scaling{ XMMatrixScaling(PtrObject3D->ComponentPhysics.BoundingSphere.Radius,
		PtrObject3D->ComponentPhysics.BoundingSphere.Radius, PtrObject3D->ComponentPhysics.BoundingSphere.Radius) };
	UpdateCBSpace(Scaling * Translation);

	m_DeviceContext->RSSetState(m_CommonStates->Wireframe());

	m_Object3DBoundingSphere->Draw();

	SetUniversalRSState();
}

void CGame::DrawObject3DLines()
{
	m_VSLine->Use();
	m_PSLine->Use();

	for (auto& Object3DLine : m_vObject3DLines)
	{
		if (Object3DLine)
		{
			if (!Object3DLine->bIsVisible) continue;

			Object3DLine->UpdateWorldMatrix();

			m_CBSpaceWVPData.World = XMMatrixTranspose(Object3DLine->ComponentTransform.MatrixWorld);
			m_CBSpaceWVPData.ViewProjection = XMMatrixTranspose(m_MatrixView * m_MatrixProjection);
			m_CBSpaceWVP->Update();

			Object3DLine->Draw();
		}
	}
}

void CGame::DrawObject2Ds()
{
	m_DeviceContext->OMSetDepthStencilState(m_CommonStates->DepthNone(), 0);
	m_DeviceContext->OMSetBlendState(m_CommonStates->NonPremultiplied(), nullptr, 0xFFFFFFFF);
	
	m_VSBase2D->Use();
	m_PSBase2D->Use();

	for (auto& Object2D : m_vObject2Ds)
	{
		if (!Object2D->IsVisible()) continue;

		UpdateCBSpace(Object2D->GetWorldMatrix());
		
		m_CBPS2DFlagsData.bUseTexture = (Object2D->HasTexture()) ? TRUE : FALSE;
		m_CBPS2DFlags->Update();

		Object2D->Draw();
	}

	m_DeviceContext->OMSetDepthStencilState(m_CommonStates->DepthDefault(), 0);
}

void CGame::DrawMiniAxes()
{
	m_DeviceContext->RSSetViewports(1, &m_vViewports[1]);

	for (auto& Object3D : m_vObject3DMiniAxes)
	{
		UpdateObject3D(Object3D.get());
		DrawObject3D(Object3D.get());

		Object3D->ComponentTransform.Translation = m_PtrCurrentCamera->GetEyePosition() + m_PtrCurrentCamera->GetForward();

		Object3D->UpdateWorldMatrix();
	}

	m_DeviceContext->RSSetViewports(1, &m_vViewports[0]);
}

void CGame::UpdatePickingRay()
{
	m_Object3DLinePickingRay->GetVertices().at(0).Position = m_PickingRayWorldSpaceOrigin;
	m_Object3DLinePickingRay->GetVertices().at(1).Position = m_PickingRayWorldSpaceOrigin + m_PickingRayWorldSpaceDirection * KPickingRayLength;
	m_Object3DLinePickingRay->UpdateVertexBuffer();
}

void CGame::DrawPickingRay()
{
	m_VSLine->Use();
	m_CBSpaceWVPData.World = XMMatrixTranspose(KMatrixIdentity);
	m_CBSpaceWVPData.ViewProjection = XMMatrixTranspose(m_MatrixView * m_MatrixProjection);
	m_CBSpaceWVP->Update();

	m_DeviceContext->GSSetShader(nullptr, nullptr, 0);
	
	m_PSLine->Use();

	m_Object3DLinePickingRay->Draw();
}

void CGame::DrawPickedTriangle()
{
	m_VSBase->Use();
	m_CBSpaceWVPData.World = XMMatrixTranspose(KMatrixIdentity);
	m_CBSpaceWVPData.ViewProjection = XMMatrixTranspose(m_MatrixView * m_MatrixProjection);
	m_CBSpaceWVP->Update();

	m_DeviceContext->GSSetShader(nullptr, nullptr, 0);
	
	m_PSVertexColor->Use();

	m_Object3DPickedTriangle->GetModel().vMeshes[0].vVertices[0].Position = m_PickedTriangleV0;
	m_Object3DPickedTriangle->GetModel().vMeshes[0].vVertices[1].Position = m_PickedTriangleV1;
	m_Object3DPickedTriangle->GetModel().vMeshes[0].vVertices[2].Position = m_PickedTriangleV2;
	m_Object3DPickedTriangle->UpdateMeshBuffer();

	m_Object3DPickedTriangle->Draw();
}

void CGame::DrawSky(float DeltaTime)
{
	// SkySphere
	{
		m_Object3DSkySphere->ComponentTransform.Translation = m_PtrCurrentCamera->GetEyePosition();

		UpdateObject3D(m_Object3DSkySphere.get());
		DrawObject3D(m_Object3DSkySphere.get(), true, true);
	}
}

void CGame::Draw3DGizmos()
{
	if (!IsAnyObject3DSelected()) return;
	
	switch (m_e3DGizmoMode)
	{
	case E3DGizmoMode::Translation:
		Draw3DGizmoTranslations(m_e3DGizmoSelectedAxis);
		break;
	case E3DGizmoMode::Rotation:
		Draw3DGizmoRotations(m_e3DGizmoSelectedAxis);
		break;
	case E3DGizmoMode::Scaling:
		Draw3DGizmoScalings(m_e3DGizmoSelectedAxis);
		break;
	default:
		break;
	}
}

void CGame::Draw3DGizmoTranslations(E3DGizmoAxis Axis)
{
	bool bHighlightX{ false };
	bool bHighlightY{ false };
	bool bHighlightZ{ false };

	if (IsGizmoHovered())
	{
		switch (Axis)
		{
		case E3DGizmoAxis::AxisX:
			bHighlightX = true;
			break;
		case E3DGizmoAxis::AxisY:
			bHighlightY = true;
			break;
		case E3DGizmoAxis::AxisZ:
			bHighlightZ = true;
			break;
		default:
			break;
		}
	}

	Draw3DGizmo(m_Object3D_3DGizmoTranslationX.get(), bHighlightX);
	Draw3DGizmo(m_Object3D_3DGizmoTranslationY.get(), bHighlightY);
	Draw3DGizmo(m_Object3D_3DGizmoTranslationZ.get(), bHighlightZ);
}

void CGame::Draw3DGizmoRotations(E3DGizmoAxis Axis)
{
	bool bHighlightX{ false };
	bool bHighlightY{ false };
	bool bHighlightZ{ false };

	if (IsGizmoHovered())
	{
		switch (Axis)
		{
		case E3DGizmoAxis::AxisX:
			bHighlightX = true;
			break;
		case E3DGizmoAxis::AxisY:
			bHighlightY = true;
			break;
		case E3DGizmoAxis::AxisZ:
			bHighlightZ = true;
			break;
		default:
			break;
		}
	}

	Draw3DGizmo(m_Object3D_3DGizmoRotationPitch.get(), bHighlightX);
	Draw3DGizmo(m_Object3D_3DGizmoRotationYaw.get(), bHighlightY);
	Draw3DGizmo(m_Object3D_3DGizmoRotationRoll.get(), bHighlightZ);
}

void CGame::Draw3DGizmoScalings(E3DGizmoAxis Axis)
{
	bool bHighlightX{ false };
	bool bHighlightY{ false };
	bool bHighlightZ{ false };

	if (IsGizmoHovered())
	{
		switch (Axis)
		{
		case E3DGizmoAxis::AxisX:
			bHighlightX = true;
			break;
		case E3DGizmoAxis::AxisY:
			bHighlightY = true;
			break;
		case E3DGizmoAxis::AxisZ:
			bHighlightZ = true;
			break;
		default:
			break;
		}
	}

	Draw3DGizmo(m_Object3D_3DGizmoScalingX.get(), bHighlightX);
	Draw3DGizmo(m_Object3D_3DGizmoScalingY.get(), bHighlightY);
	Draw3DGizmo(m_Object3D_3DGizmoScalingZ.get(), bHighlightZ);
}

void CGame::Draw3DGizmo(CObject3D* const Gizmo, bool bShouldHighlight)
{
	CShader* VS{ Gizmo->ComponentRender.PtrVS };
	CShader* PS{ Gizmo->ComponentRender.PtrPS };

	float Scalar{ XMVectorGetX(XMVector3Length(m_PtrCurrentCamera->GetEyePosition() - Gizmo->ComponentTransform.Translation)) * 0.1f };
	Scalar = pow(Scalar, 0.7f);

	Gizmo->ComponentTransform.Scaling = XMVectorSet(Scalar, Scalar, Scalar, 0.0f);
	Gizmo->UpdateWorldMatrix();
	UpdateCBSpace(Gizmo->ComponentTransform.MatrixWorld);
	VS->Use();

	m_CBGizmoColorFactorData.ColorFactor = (bShouldHighlight) ? XMVectorSet(2.0f, 2.0f, 2.0f, 0.95f) : XMVectorSet(0.75f, 0.75f, 0.75f, 0.75f);
	m_CBGizmoColorFactor->Update();
	PS->Use();

	Gizmo->Draw();
}

void CGame::DrawEditorGUI()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::PushFont(m_EditorGUIFont);

	DrawEditorGUIMenuBar();

	DrawEditorGUIPopupObjectAdder();

	DrawEditorGUIWindowPropertyEditor();

	DrawEditorGUIWindowSceneEditor();

	ImGui::PopFont();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void CGame::DrawEditorGUIMenuBar()
{
	if (ImGui::BeginMainMenuBar())
	{
		if (m_CapturedKeyboardState.LeftControl && m_CapturedKeyboardState.A) m_EditorGUIBools.bShowPopupObjectAdder = true;

		if (ImGui::BeginMenu(u8"창"))
		{
			ImGui::MenuItem(u8"속성 편집기", nullptr, &m_EditorGUIBools.bShowWindowPropertyEditor);
			ImGui::MenuItem(u8"장면 편집기", nullptr, &m_EditorGUIBools.bShowWindowSceneEditor);

			ImGui::EndMenu();
		}

		if (ImGui::MenuItem(u8"종료", "Alt+Q"))
		{
			Destroy();
			return;
		}

		ImGui::EndMainMenuBar();
	}
}

void CGame::DrawEditorGUIPopupObjectAdder()
{
	// ### 오브젝트 추가 윈도우 ###
	if (m_EditorGUIBools.bShowPopupObjectAdder) ImGui::OpenPopup(u8"오브젝트 추가기");
	ImGui::SetNextWindowPosCenter();
	if (ImGui::BeginPopup(u8"오브젝트 추가기", ImGuiWindowFlags_AlwaysAutoResize))
	{
		static char NewObejctName[KAssetNameMaxLength]{};
		static char ModelFileNameWithPath[MAX_PATH]{};
		static char ModelFileNameWithoutPath[MAX_PATH]{};
		static bool bIsModelRigged{ false };

		static constexpr float KIndentPerDepth{ 12.0f };
		static constexpr float KItemsOffetX{ 150.0f };
		static constexpr float KItemsWidth{ 150.0f };
		static const char* const KOptions[2]{ u8"3D 도형 (삼각형)", u8"2-패치 구 (제어점 1개)" };
		static int iSelectedOption{};

		static const char* const K3DPrimitiveTypes[10]{ u8"정사각형(XY)", u8"정사각형(XZ)", u8"정사각형(YZ)",
				u8"원", u8"정육면체", u8"각뿔", u8"각기둥", u8"구", u8"도넛(Torus)", u8"베지에 삼각형" };
		static int iSelected3DPrimitiveType{};
		static uint32_t SideCount{ KDefaultPrimitiveDetail };
		static uint32_t SegmentCount{ KDefaultPrimitiveDetail };
		static float RadiusFactor{ 0.0f };
		static float InnerRadius{ 0.5f };
		static float WidthScalar3D{ 1.0f };
		static float HeightScalar3D{ 1.0f };
		static float PixelWidth{ 50.0f };
		static float PixelHeight{ 50.0f };

		static XMFLOAT4 MaterialUniformColor{ 1.0f, 1.0f, 1.0f, 1.0f };

		bool bShowDialogLoad3DModel{};

		ImGui::SetItemDefaultFocus();
		ImGui::SetNextItemWidth(140);
		ImGui::InputText(u8"오브젝트 이름", NewObejctName, KAssetNameMaxLength);

		for (int iOption = 0; iOption < ARRAYSIZE(KOptions); ++iOption)
		{
			if (ImGui::Selectable(KOptions[iOption], (iSelectedOption == iOption)))
			{
				iSelectedOption = iOption;
			}

			if (iSelectedOption == iOption)
			{
				if (iSelectedOption == 0)
				{
					ImGui::Indent(KIndentPerDepth);

					for (int i3DPrimitiveType = 0; i3DPrimitiveType < ARRAYSIZE(K3DPrimitiveTypes); ++i3DPrimitiveType)
					{
						if (ImGui::Selectable(K3DPrimitiveTypes[i3DPrimitiveType], (iSelected3DPrimitiveType == i3DPrimitiveType)))
						{
							iSelected3DPrimitiveType = i3DPrimitiveType;
						}
						if (i3DPrimitiveType == iSelected3DPrimitiveType)
						{
							ImGui::Indent(KIndentPerDepth);

							ImGui::PushItemWidth(KItemsWidth);

							ImGui::AlignTextToFramePadding();
							ImGui::Text(u8"- 색상");
							ImGui::SameLine(KItemsOffetX);
							ImGui::ColorEdit3(u8"##- 색상", (float*)&MaterialUniformColor.x, ImGuiColorEditFlags_RGB);

							// Quasi-2D primitives scalars
							if (iSelected3DPrimitiveType >= 0 && iSelected3DPrimitiveType <= 3)
							{
								ImGui::AlignTextToFramePadding();
								ImGui::Text(u8"- 가로 크기");
								ImGui::SameLine(KItemsOffetX);
								ImGui::SliderFloat(u8"##- 가로 크기", &WidthScalar3D, 0.01f, 100.0f);

								ImGui::AlignTextToFramePadding();
								ImGui::Text(u8"- 세로 크기");
								ImGui::SameLine(KItemsOffetX);
								ImGui::SliderFloat(u8"##- 세로 크기", &HeightScalar3D, 0.01f, 100.0f);
							}

							// 3D primitives that require SideCount
							if (iSelected3DPrimitiveType == 3 || (iSelected3DPrimitiveType >= 5 && iSelected3DPrimitiveType <= 6) || iSelected3DPrimitiveType == 8)
							{
								if (iSelected3DPrimitiveType == 5)
								{
									// Cone
									ImGui::AlignTextToFramePadding();
									ImGui::Text(u8"- 반지름 인수");
									ImGui::SameLine(KItemsOffetX);
									ImGui::SliderFloat(u8"##- 반지름 인수", &RadiusFactor, 0.0f, 1.0f);
								}

								ImGui::AlignTextToFramePadding();
								ImGui::Text(u8"- 옆면 수");
								ImGui::SameLine(KItemsOffetX);
								ImGui::SliderInt(u8"##- 옆면 수", (int*)&SideCount, KMinPrimitiveDetail, KMaxPrimitiveDetail);
							}

							// 3D primitives that require SegmentCount
							if (iSelected3DPrimitiveType == 7 || iSelected3DPrimitiveType == 8)
							{
								if (iSelected3DPrimitiveType == 8)
								{
									// Torus
									ImGui::AlignTextToFramePadding();
									ImGui::Text(u8"- 띠 반지름");
									ImGui::SameLine(KItemsOffetX);
									ImGui::SliderFloat(u8"##- 띠 반지름", &InnerRadius, 0.0f, 1.0f);
								}

								ImGui::AlignTextToFramePadding();
								ImGui::Text(u8"- Segment 수");
								ImGui::SameLine(KItemsOffetX);
								ImGui::SliderInt(u8"##- Segment 수", (int*)&SegmentCount, KMinPrimitiveDetail, KMaxPrimitiveDetail);
							}

							ImGui::PopItemWidth();

							ImGui::Unindent(KIndentPerDepth);
						}
					}

					ImGui::Unindent(KIndentPerDepth);
				}
				else if (iSelectedOption == 1)
				{
					// 1 control point 2 patches
				}
			}
		}
		
		if (ImGui::Button(u8"결정") || m_CapturedKeyboardState.Enter)
		{
			if (strlen(NewObejctName) == 0)
			{
				strcpy_s(NewObejctName, ("primitive" + to_string(m_PrimitiveCreationCounter)).c_str());
			}

			bool IsObjectCreated{ true };
			if (iSelectedOption == 0)
			{
				InsertObject3D(NewObejctName);
				CObject3D* const Object3D{ GetObject3D(NewObejctName) };

				SMesh Mesh{};
				CMaterialData MaterialData{};
				MaterialData.SetUniformColor(XMFLOAT3(MaterialUniformColor.x, MaterialUniformColor.y, MaterialUniformColor.z));

				switch (iSelected3DPrimitiveType)
				{
				case 0:
					Mesh = GenerateSquareXYPlane();
					ScaleMesh(Mesh, XMVectorSet(WidthScalar3D, HeightScalar3D, 1.0f, 0));
					Object3D->ComponentPhysics.BoundingSphere.Radius = sqrt(2.0f);
					break;
				case 1:
					Mesh = GenerateSquareXZPlane();
					ScaleMesh(Mesh, XMVectorSet(WidthScalar3D, 1.0f, HeightScalar3D, 0));
					Object3D->ComponentPhysics.BoundingSphere.Radius = sqrt(2.0f);
					break;
				case 2:
					Mesh = GenerateSquareYZPlane();
					ScaleMesh(Mesh, XMVectorSet(1.0f, WidthScalar3D, HeightScalar3D, 0));
					Object3D->ComponentPhysics.BoundingSphere.Radius = sqrt(2.0f);
					break;
				case 3:
					Mesh = GenerateCircleXZPlane(SideCount);
					ScaleMesh(Mesh, XMVectorSet(WidthScalar3D, 1.0f, HeightScalar3D, 0));
					break;
				case 4:
					Mesh = GenerateCube();
					break;
				case 5:
					Mesh = GenerateCone(RadiusFactor, 1.0f, 1.0f, SideCount);
					Object3D->ComponentPhysics.BoundingSphere.CenterOffset = XMVectorSetY(Object3D->ComponentPhysics.BoundingSphere.CenterOffset, -0.5f);
					break;
				case 6:
					Mesh = GenerateCylinder(1.0f, 1.0f, SideCount);
					Object3D->ComponentPhysics.BoundingSphere.Radius = sqrt(1.5f);
					break;
				case 7:
					Mesh = GenerateSphere(SegmentCount);
					break;
				case 8:
					Mesh = GenerateTorus(InnerRadius, SideCount, SegmentCount);
					Object3D->ComponentPhysics.BoundingSphere.Radius += InnerRadius;
					break;
				case 9:
					Mesh = GenerateTriangle(
						XMVectorSet(0, 1.732f, 0, 1), XMVectorSet(+1.0f, 0, 0, 1), XMVectorSet(-1.0f, 0, 0, 1),
						XMVectorSet(1, 0, 0, 1), XMVectorSet(0, 1, 0, 1), XMVectorSet(0, 0, 1, 1));
					
					Mesh.vVertices[0].Normal = XMVector3Normalize(XMVectorSet(    0, +0.5f, -0.5f, 0));
					Mesh.vVertices[1].Normal = XMVector3Normalize(XMVectorSet(+0.5f, -0.5f, -0.5f, 0));
					Mesh.vVertices[2].Normal = XMVector3Normalize(XMVectorSet(-0.5f, -0.5f, -0.5f, 0));

					Object3D->ComponentRender.PtrPS = m_PSVertexColor.get();
					break;
				default:
					break;
				}

				Object3D->Create(Mesh, MaterialData);

				++m_PrimitiveCreationCounter;
			}
			else if (iSelectedOption == 1)
			{
				// 1 control point 2 patches (must tessellate)

				InsertObject3D(NewObejctName);
				CObject3D* const Object3D{ GetObject3D(NewObejctName) };

				Object3D->CreatePatches(1, 2);

				++m_PrimitiveCreationCounter;
			}

			if (IsObjectCreated)
			{
				m_EditorGUIBools.bShowPopupObjectAdder = false;
				memset(NewObejctName, 0, KAssetNameMaxLength);

				WidthScalar3D = 1.0f;
				HeightScalar3D = 1.0f;

				ImGui::CloseCurrentPopup();
			}
		}

		ImGui::SameLine();

		if (ImGui::Button(u8"취소"))
		{
			m_EditorGUIBools.bShowPopupObjectAdder = false;
			memset(ModelFileNameWithPath, 0, MAX_PATH);
			memset(ModelFileNameWithoutPath, 0, MAX_PATH);
			memset(NewObejctName, 0, KAssetNameMaxLength);
			ImGui::CloseCurrentPopup();
		}

		if (bShowDialogLoad3DModel)
		{
			static CFileDialog FileDialog{ GetWorkingDirectory() };
			if (FileDialog.OpenFileDialog("FBX 파일\0*.fbx\0SMOD 파일\0*.smod\0모든 파일\0*.*\0", "모델 불러오기"))
			{
				strcpy_s(ModelFileNameWithPath, FileDialog.GetRelativeFileName().c_str());
				strcpy_s(ModelFileNameWithoutPath, FileDialog.GetFileNameWithoutPath().c_str());
			}
		}

		ImGui::EndPopup();
	}
}

void CGame::DrawEditorGUIWindowPropertyEditor()
{
	// ### 속성 편집기 윈도우 ###
	if (m_EditorGUIBools.bShowWindowPropertyEditor)
	{
		static constexpr float KInitialWindowWidth{ 400 };
		ImGui::SetNextWindowPos(ImVec2(m_WindowSize.x - KInitialWindowWidth, 21), ImGuiCond_Appearing);
		ImGui::SetNextWindowSize(ImVec2(KInitialWindowWidth, 0), ImGuiCond_Appearing);
		ImGui::SetNextWindowSizeConstraints(
			ImVec2(m_WindowSize.x * 0.25f, m_WindowSize.y), ImVec2(m_WindowSize.x * 0.5f, m_WindowSize.y));

		if (ImGui::Begin(u8"속성 편집기", &m_EditorGUIBools.bShowWindowPropertyEditor, 
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysVerticalScrollbar))
		{
			float WindowWidth{ ImGui::GetWindowWidth() };

			if (ImGui::BeginTabBar(u8"탭바", ImGuiTabBarFlags_None))
			{
				// 오브젝트 탭
				if (ImGui::BeginTabItem(u8"오브젝트"))
				{
					static constexpr float KLabelsWidth{ 220 };
					static constexpr float KItemsMaxWidth{ 240 };
					float ItemsWidth{ WindowWidth - KLabelsWidth };
					ItemsWidth = min(ItemsWidth, KItemsMaxWidth);
					float ItemsOffsetX{ WindowWidth - ItemsWidth - 20 };

					if (IsAnyObject3DSelected())
					{
						ImGui::PushItemWidth(ItemsWidth);
						{
							CObject3D* const Object3D{ GetSelectedObject3D() };

							ImGui::AlignTextToFramePadding();
							ImGui::Text(u8"선택된 오브젝트:");
							ImGui::SameLine(ItemsOffsetX);
							ImGui::AlignTextToFramePadding();
							ImGui::Text(u8"<%s>", GetSelectedObject3DName().c_str());

							ImGui::Separator();

							ImGui::AlignTextToFramePadding();
							ImGui::Text(u8"위치");
							ImGui::SameLine(ItemsOffsetX);
							float Translation[3]{ XMVectorGetX(Object3D->ComponentTransform.Translation),
							XMVectorGetY(Object3D->ComponentTransform.Translation), XMVectorGetZ(Object3D->ComponentTransform.Translation) };
							if (ImGui::DragFloat3(u8"##위치", Translation, KTranslationDelta,
								KTranslationMinLimit, KTranslationMaxLimit, "%.2f"))
							{
								Object3D->ComponentTransform.Translation = XMVectorSet(Translation[0], Translation[1], Translation[2], 1.0f);
								Object3D->UpdateWorldMatrix();
							}

							ImGui::AlignTextToFramePadding();
							ImGui::Text(u8"회전");
							ImGui::SameLine(ItemsOffsetX);
							int PitchYawRoll360[3]{ (int)(Object3D->ComponentTransform.Pitch * KRotation2PITo360),
								(int)(Object3D->ComponentTransform.Yaw * KRotation2PITo360),
								(int)(Object3D->ComponentTransform.Roll * KRotation2PITo360) };
							if (ImGui::DragInt3(u8"##회전", PitchYawRoll360, KRotation360Unit,
								KRotation360MinLimit, KRotation360MaxLimit))
							{
								Object3D->ComponentTransform.Pitch = PitchYawRoll360[0] * KRotation360To2PI;
								Object3D->ComponentTransform.Yaw = PitchYawRoll360[1] * KRotation360To2PI;
								Object3D->ComponentTransform.Roll = PitchYawRoll360[2] * KRotation360To2PI;
								Object3D->UpdateWorldMatrix();
							}

							ImGui::AlignTextToFramePadding();
							ImGui::Text(u8"크기");
							ImGui::SameLine(ItemsOffsetX);
							float Scaling[3]{ XMVectorGetX(Object3D->ComponentTransform.Scaling),
								XMVectorGetY(Object3D->ComponentTransform.Scaling), XMVectorGetZ(Object3D->ComponentTransform.Scaling) };
							if (ImGui::DragFloat3(u8"##크기", Scaling, KScalingDelta,
								KScalingMinLimit, KScalingMaxLimit, "%.3f"))
							{
								Object3D->ComponentTransform.Scaling = XMVectorSet(Scaling[0], Scaling[1], Scaling[2], 0.0f);
								Object3D->UpdateWorldMatrix();
							}

							ImGui::Separator();

							ImGui::AlignTextToFramePadding();
							ImGui::Text(u8"오브젝트 BS 중심");
							ImGui::SameLine(ItemsOffsetX);
							float BSCenterOffset[3]{
								XMVectorGetX(Object3D->ComponentPhysics.BoundingSphere.CenterOffset),
								XMVectorGetY(Object3D->ComponentPhysics.BoundingSphere.CenterOffset),
								XMVectorGetZ(Object3D->ComponentPhysics.BoundingSphere.CenterOffset) };
							if (ImGui::DragFloat3(u8"##오브젝트 BS 중심", BSCenterOffset, KBSCenterOffsetDelta,
								KBSCenterOffsetMinLimit, KBSCenterOffsetMaxLimit, "%.2f"))
							{
								Object3D->ComponentPhysics.BoundingSphere.CenterOffset =
									XMVectorSet(BSCenterOffset[0], BSCenterOffset[1], BSCenterOffset[2], 1.0f);
							}

							ImGui::AlignTextToFramePadding();
							ImGui::Text(u8"오브젝트 BS 반지름 편중치");
							ImGui::SameLine(ItemsOffsetX);
							float BSRadiusBias{ Object3D->ComponentPhysics.BoundingSphere.RadiusBias };
							if (ImGui::DragFloat(u8"##오브젝트 BS반지름 편중치", &BSRadiusBias, KBSRadiusBiasDelta,
								KBSRadiusBiasMinLimit, KBSRadiusBiasMaxLimit, "%.2f"))
							{
								Object3D->ComponentPhysics.BoundingSphere.RadiusBias = BSRadiusBias;
							}

							ImGui::AlignTextToFramePadding();
							ImGui::Text(u8"오브젝트 BS 반지름 (자동)");
							ImGui::SameLine(ItemsOffsetX);
							float BSRadius{ Object3D->ComponentPhysics.BoundingSphere.Radius };
							ImGui::DragFloat(u8"##오브젝트 BS반지름 (자동)", &BSRadius, KBSRadiusDelta,
								KBSRadiusMinLimit, KBSRadiusMaxLimit, "%.2f");

							ImGui::Separator();

							if (Object3D->IsPatches())
							{
								ImGui::AlignTextToFramePadding();
								ImGui::Text(u8"제어점 개수");
								ImGui::SameLine(ItemsOffsetX);
								ImGui::Text(u8"%d", (int)Object3D->GetControlPointCountPerPatch());

								ImGui::AlignTextToFramePadding();
								ImGui::Text(u8"패치 개수");
								ImGui::SameLine(ItemsOffsetX);
								ImGui::Text(u8"%d", (int)Object3D->GetPatchCount());
							}
							else
							{
								ImGui::AlignTextToFramePadding();
								ImGui::Text(u8"정점 개수");
								ImGui::SameLine(ItemsOffsetX);
								int VertexCount{};
								for (const SMesh& Mesh : Object3D->GetModel().vMeshes)
								{
									VertexCount += (int)Mesh.vVertices.size();
								}
								ImGui::Text(u8"%d", VertexCount);

								ImGui::AlignTextToFramePadding();
								ImGui::Text(u8"삼각형 개수");
								ImGui::SameLine(ItemsOffsetX);
								int TriangleCount{};
								for (const SMesh& Mesh : Object3D->GetModel().vMeshes)
								{
									TriangleCount += (int)Mesh.vTriangles.size();
								}
								ImGui::Text(u8"%d", TriangleCount);
							}

							// Tessellation data
							ImGui::Separator();

							if (!Object3D->IsPatches())
							{
								bool bShouldTessellate{ Object3D->ShouldTessellate() };
								ImGui::AlignTextToFramePadding();
								ImGui::Text(u8"테셀레이션 사용 여부");
								ImGui::SameLine(ItemsOffsetX);
								if (ImGui::Checkbox(u8"##테셀레이션 사용 여부", &bShouldTessellate))
								{
									Object3D->ShouldTessellate(bShouldTessellate);
								}
							}
							
							ImGui::AlignTextToFramePadding();
							ImGui::Text(u8"테셀레이션 방식");

							int iTessellationType{ (int)Object3D->TessellationType() };
							ImGui::RadioButton(u8"Frational Odd", &iTessellationType, 0);
							ImGui::RadioButton(u8"Frational Even", &iTessellationType, 1);
							ImGui::RadioButton(u8"Integer", &iTessellationType, 2);
							Object3D->TessellationType((CObject3D::ETessellationType)iTessellationType);

							CObject3D::SCBTessFactorData TessFactorData{ Object3D->GetTessFactorData() };
							ImGui::AlignTextToFramePadding();
							ImGui::Text(u8"테셀레이션 변 계수");
							ImGui::SameLine(ItemsOffsetX);
							if (ImGui::SliderFloat(u8"##테셀레이션 변 계수", &TessFactorData.EdgeTessFactor, 0.0f, 64.0f, "%.2f"))
							{
								Object3D->SetTessFactorData(TessFactorData);
							}

							ImGui::AlignTextToFramePadding();
							ImGui::Text(u8"테셀레이션 내부 계수");
							ImGui::SameLine(ItemsOffsetX);
							if (ImGui::SliderFloat(u8"##테셀레이션 내부 계수", &TessFactorData.InsideTessFactor, 0.0f, 64.0f, "%.2f"))
							{
								Object3D->SetTessFactorData(TessFactorData);
							}

							CObject3D::SCBDisplacementData DisplacementData{ Object3D->GetDisplacementData() };
							ImGui::AlignTextToFramePadding();
							ImGui::Text(u8"변위 계수");
							ImGui::SameLine(ItemsOffsetX);
							if (ImGui::SliderFloat(u8"##변위 계수", &DisplacementData.DisplacementFactor, 0.0f, 1.0f, "%.2f"))
							{
								Object3D->SetDisplacementData(DisplacementData);
							}

							// Material data
							ImGui::Separator();

							ImGui::AlignTextToFramePadding();
							ImGui::Text(u8"오브젝트 재질");
							if (Object3D->GetMaterialCount() > 0)
							{
								static CMaterialData* capturedMaterialData{};
								static CMaterialTextureSet* capturedMaterialTextureSet{};
								static STextureData::EType ecapturedTextureType{};
								if (!ImGui::IsPopupOpen(u8"텍스처탐색기")) m_EditorGUIBools.bShowPopupMaterialTextureExplorer = false;

								for (size_t iMaterial = 0; iMaterial < Object3D->GetMaterialCount(); ++iMaterial)
								{
									CMaterialData& MaterialData{ Object3D->GetModel().vMaterialData[iMaterial] };
									CMaterialTextureSet* const MaterialTextureSet{ Object3D->GetMaterialTextureSet(iMaterial) };

									ImGui::PushID((int)iMaterial);

									if (DrawEditorGUIWindowPropertyEditor_MaterialData(MaterialData, MaterialTextureSet, ecapturedTextureType, ItemsOffsetX))
									{
										capturedMaterialData = &MaterialData;
										capturedMaterialTextureSet = MaterialTextureSet;
									}
	
									ImGui::PopID();
								}

								DrawEditorGUIPopupMaterialTextureExplorer(capturedMaterialData, capturedMaterialTextureSet, ecapturedTextureType);
								DrawEditorGUIPopupMaterialNameChanger(capturedMaterialData, true);
							}
						}
						ImGui::PopItemWidth();
					}
					else if (IsAnyObject2DSelected())
					{
						// Object2D
					}
					else if (IsAnyCameraSelected())
					{
						ImGui::PushItemWidth(ItemsWidth);
						{
							CCamera* const SelectedCamera{ GetSelectedCamera() };
							const XMVECTOR& KEyePosition{ SelectedCamera->GetEyePosition() };
							float EyePosition[3]{ XMVectorGetX(KEyePosition), XMVectorGetY(KEyePosition), XMVectorGetZ(KEyePosition) };
							float Pitch{ SelectedCamera->GetPitch() };
							float Yaw{ SelectedCamera->GetYaw() };

							ImGui::AlignTextToFramePadding();
							ImGui::Text(u8"현재 화면 카메라:");
							ImGui::SameLine(ItemsOffsetX);
							ImGui::AlignTextToFramePadding();
							CCamera* const CurrentCamera{ GetCurrentCamera() };
							ImGui::Text(u8"<%s>", CurrentCamera->GetName().c_str());

							if (m_PtrCurrentCamera != GetEditorCamera())
							{
								ImGui::SetCursorPosX(ItemsOffsetX);
								if (ImGui::Button(u8"에디터 카메라로 돌아가기", ImVec2(ItemsWidth, 0)))
								{
									m_PtrCurrentCamera = GetEditorCamera();
								}
							}

							ImGui::Separator();

							ImGui::AlignTextToFramePadding();
							ImGui::Text(u8"선택된 카메라:");
							ImGui::SameLine(ItemsOffsetX);
							ImGui::AlignTextToFramePadding();
							string Name{ GetSelectedCameraName() };
							ImGui::Text(u8"<%s>", GetSelectedCameraName().c_str());

							ImGui::AlignTextToFramePadding();
							ImGui::Text(u8"카메라 종류:");
							ImGui::SameLine(ItemsOffsetX);
							switch (SelectedCamera->GetType())
							{
							case CCamera::EType::FirstPerson:
								ImGui::AlignTextToFramePadding();
								ImGui::Text(u8"1인칭 카메라");
								break;
							case CCamera::EType::ThirdPerson:
								ImGui::AlignTextToFramePadding();
								ImGui::Text(u8"3인칭 카메라");
								break;
							case CCamera::EType::FreeLook:
								ImGui::AlignTextToFramePadding();
								ImGui::Text(u8"자유 시점 카메라");
								break;
							default:
								break;
							}

							ImGui::AlignTextToFramePadding();
							ImGui::Text(u8"위치");
							ImGui::SameLine(ItemsOffsetX);
							if (ImGui::DragFloat3(u8"##위치", EyePosition, 0.01f, -10000.0f, +10000.0f, "%.3f"))
							{
								SelectedCamera->SetEyePosition(XMVectorSet(EyePosition[0], EyePosition[1], EyePosition[2], 1.0f));
							}

							ImGui::AlignTextToFramePadding();
							ImGui::Text(u8"회전 Pitch");
							ImGui::SameLine(ItemsOffsetX);
							if (ImGui::DragFloat(u8"##회전 Pitch", &Pitch, 0.01f, -10000.0f, +10000.0f, "%.3f"))
							{
								SelectedCamera->SetPitch(Pitch);
							}

							ImGui::AlignTextToFramePadding();
							ImGui::Text(u8"회전 Yaw");
							ImGui::SameLine(ItemsOffsetX);
							if (ImGui::DragFloat(u8"##회전 Yaw", &Yaw, 0.01f, -10000.0f, +10000.0f, "%.3f"))
							{
								SelectedCamera->SetYaw(Yaw);
							}

							ImGui::AlignTextToFramePadding();
							ImGui::Text(u8"카메라 이동 속도");
							ImGui::SameLine(ItemsOffsetX);
							ImGui::SliderFloat(u8"##카메라 이동 속도", &m_CameraMovementFactor, 1.0f, 100.0f, "%.0f");

							if (m_PtrCurrentCamera != SelectedCamera)
							{
								ImGui::SetCursorPosX(ItemsOffsetX);
								if (ImGui::Button(u8"현재 화면 카메라로 지정", ImVec2(ItemsWidth, 0)))
								{
									m_PtrCurrentCamera = SelectedCamera;
								}
							}
						}
						ImGui::PopItemWidth();
					}
					else
					{
						ImGui::Text(u8"<먼저 오브젝트를 선택하세요.>");
					}

					ImGui::EndTabItem();
				}

				// 기타 탭
				if (ImGui::BeginTabItem(u8"기타"))
				{
					const XMVECTOR& KDirectionalLightDirection{ GetDirectionalLightDirection() };
					float DirectionalLightDirection[3]{ XMVectorGetX(KDirectionalLightDirection), XMVectorGetY(KDirectionalLightDirection),
						XMVectorGetZ(KDirectionalLightDirection) };

					static constexpr float KLabelsWidth{ 220 };
					static constexpr float KItemsMaxWidth{ 240 };
					float ItemsWidth{ WindowWidth - KLabelsWidth };
					ItemsWidth = min(ItemsWidth, KItemsMaxWidth);
					float ItemsOffsetX{ WindowWidth - ItemsWidth - 20 };
					ImGui::PushItemWidth(ItemsWidth);
					{
						if (ImGui::TreeNodeEx(u8"조명", ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen))
						{
							ImGui::AlignTextToFramePadding();
							ImGui::Text(u8"Directional Light 위치");
							ImGui::SameLine(ItemsOffsetX);
							if (ImGui::DragFloat3(u8"##Directional Light 위치", DirectionalLightDirection, 0.02f, -1.0f, +1.0f, "%.2f"))
							{
								SetDirectionalLightDirection(XMVectorSet(DirectionalLightDirection[0], DirectionalLightDirection[1],
									DirectionalLightDirection[2], 0.0f));
							}

							ImGui::AlignTextToFramePadding();
							ImGui::Text(u8"Directional Light 색상");
							ImGui::SameLine(ItemsOffsetX);
							XMFLOAT3 DirectionalLightColor{ GetDirectionalLightColor() };
							if (ImGui::ColorEdit3(u8"##Directional Light 색상 (HDR)", &DirectionalLightColor.x,
								ImGuiColorEditFlags_RGB | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR))
							{
								SetDirectionalLightColor(DirectionalLightColor);
							}

							ImGui::AlignTextToFramePadding();
							ImGui::Text(u8"Ambient Light 색상");
							ImGui::SameLine(ItemsOffsetX);
							XMFLOAT3 AmbientLightColor{ GetAmbientLightColor() };
							if (ImGui::ColorEdit3(u8"##Ambient Light 색상", &AmbientLightColor.x, ImGuiColorEditFlags_RGB))
							{
								SetAmbientlLight(AmbientLightColor, GetAmbientLightIntensity());
							}

							ImGui::AlignTextToFramePadding();
							ImGui::Text(u8"Ambient Light 강도");
							ImGui::SameLine(ItemsOffsetX);
							float AmbientLightIntensity{ GetAmbientLightIntensity() };
							if (ImGui::DragFloat(u8"##Ambient Light 강도", &AmbientLightIntensity, 0.02f, 0.0f, +1.0f, "%.2f"))
							{
								SetAmbientlLight(GetAmbientLightColor(), AmbientLightIntensity);
							}

							ImGui::AlignTextToFramePadding();
							ImGui::Text(u8"노출 (HDR)");
							ImGui::SameLine(ItemsOffsetX);
							float Exposure{ GetExposure() };
							if (ImGui::DragFloat(u8"##노출", &Exposure, 0.02f, 0.1f, +10.0f, "%.2f"))
							{
								SetExposure(Exposure);
							}

							ImGui::TreePop();
						}

						ImGui::Separator();

						if (ImGui::TreeNodeEx(u8"FPS", ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen))
						{
							ImGui::AlignTextToFramePadding();
							ImGui::Text(u8"Frames per second:");
							ImGui::SameLine(ItemsOffsetX);
							ImGui::AlignTextToFramePadding();
							ImGui::Text(u8"%d", m_FPS);

							ImGui::TreePop();
						}

						ImGui::Separator();
						ImGui::Separator();

						ImGui::AlignTextToFramePadding();
						ImGui::Text(u8"에디터 플래그");

						ImGui::AlignTextToFramePadding();
						ImGui::Text(u8"와이어 프레임");
						ImGui::SameLine(ItemsOffsetX);
						bool bDrawWireFrame{ EFLAG_HAS(m_eFlagsRendering, EFlagsRendering::DrawWireFrame) };
						if (ImGui::Checkbox(u8"##와이어 프레임", &bDrawWireFrame))
						{
							ToggleGameRenderingFlags(EFlagsRendering::DrawWireFrame);
						}

						ImGui::AlignTextToFramePadding();
						ImGui::Text(u8"법선 표시");
						ImGui::SameLine(ItemsOffsetX);
						bool bDrawNormals{ EFLAG_HAS(m_eFlagsRendering, EFlagsRendering::DrawNormals) };
						if (ImGui::Checkbox(u8"##법선 표시", &bDrawNormals))
						{
							ToggleGameRenderingFlags(EFlagsRendering::DrawNormals);
						}

						/*
						ImGui::AlignTextToFramePadding();
						ImGui::Text(u8"3D Gizmo 표시");
						ImGui::SameLine(ItemsOffsetX);
						bool bDrawBoundingSphere{ EFLAG_HAS(m_eFlagsRendering, EFlagsRendering::Use3DGizmos) };
						if (ImGui::Checkbox(u8"##3D Gizmo 표시", &bDrawBoundingSphere))
						{
							ToggleGameRenderingFlags(EFlagsRendering::Use3DGizmos);
						}
						*/

						ImGui::AlignTextToFramePadding();
						ImGui::Text(u8"화면 상단에 좌표축 표시");
						ImGui::SameLine(ItemsOffsetX);
						bool bDrawMiniAxes{ EFLAG_HAS(m_eFlagsRendering, EFlagsRendering::DrawMiniAxes) };
						if (ImGui::Checkbox(u8"##화면 상단에 좌표축 표시", &bDrawMiniAxes))
						{
							ToggleGameRenderingFlags(EFlagsRendering::DrawMiniAxes);
						}

						/*
						ImGui::AlignTextToFramePadding();
						ImGui::Text(u8"피킹 데이터 표시");
						ImGui::SameLine(ItemsOffsetX);
						bool bDrawPickingData{ EFLAG_HAS(m_eFlagsRendering, EFlagsRendering::DrawPickingData) };
						if (ImGui::Checkbox(u8"##피킹 데이터 표시", &bDrawPickingData))
						{
							ToggleGameRenderingFlags(EFlagsRendering::DrawPickingData);
						}
						*/
						
						ImGui::AlignTextToFramePadding();
						ImGui::Text(u8"Bounding Sphere 표시");
						ImGui::SameLine(ItemsOffsetX);
						bool bDrawBoundingSphere{ EFLAG_HAS(m_eFlagsRendering, EFlagsRendering::DrawBoundingSphere) };
						if (ImGui::Checkbox(u8"##Bounding Sphere 표시", &bDrawBoundingSphere))
						{
							ToggleGameRenderingFlags(EFlagsRendering::DrawBoundingSphere);
						}

						ImGui::Separator();
						ImGui::Separator();

						ImGui::AlignTextToFramePadding();
						ImGui::Text(u8"엔진 플래그");

						ImGui::AlignTextToFramePadding();
						ImGui::Text(u8"조명 적용");
						ImGui::SameLine(ItemsOffsetX);
						bool bUseLighting{ EFLAG_HAS(m_eFlagsRendering, EFlagsRendering::UseLighting) };
						if (ImGui::Checkbox(u8"##조명 적용", &bUseLighting))
						{
							ToggleGameRenderingFlags(EFlagsRendering::UseLighting);
						}

						ImGui::AlignTextToFramePadding();
						ImGui::Text(u8"물리 기반 렌더링 사용");
						ImGui::SameLine(ItemsOffsetX);
						bool bUsePhysicallyBasedRendering{ EFLAG_HAS(m_eFlagsRendering, EFlagsRendering::UsePhysicallyBasedRendering) };
						if (ImGui::Checkbox(u8"##물리 기반 렌더링 사용", &bUsePhysicallyBasedRendering))
						{
							ToggleGameRenderingFlags(EFlagsRendering::UsePhysicallyBasedRendering);
						}
					}
					ImGui::PopItemWidth();

					ImGui::EndTabItem();
				}

				ImGui::EndTabBar();
			}
		}

		ImGui::End();
	}
}

bool CGame::DrawEditorGUIWindowPropertyEditor_MaterialData(CMaterialData& MaterialData, CMaterialTextureSet* const TextureSet, 
	STextureData::EType& eSeletedTextureType, float ItemsOffsetX)
{
	bool Result{ false };
	bool bUsePhysicallyBasedRendering{ EFLAG_HAS(m_eFlagsRendering, EFlagsRendering::UsePhysicallyBasedRendering) };

	if (ImGui::TreeNodeEx(MaterialData.Name().c_str(), ImGuiTreeNodeFlags_SpanAvailWidth))
	{
		if (ImGui::Button(u8"재질 이름 변경"))
		{
			m_EditorGUIBools.bShowPopupMaterialNameChanger = true;
			Result = true;
		}

		if (bUsePhysicallyBasedRendering)
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text(u8"Base color 색상");
		}
		else
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text(u8"Diffuse 색상");
		}
		ImGui::SameLine(ItemsOffsetX);
		XMFLOAT3 DiffuseColor{ MaterialData.DiffuseColor() };
		if (ImGui::ColorEdit3(u8"##Diffuse 색상", &DiffuseColor.x, ImGuiColorEditFlags_RGB))
		{
			MaterialData.DiffuseColor(DiffuseColor);
		}

		if (!bUsePhysicallyBasedRendering)
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text(u8"Ambient 색상");
			ImGui::SameLine(ItemsOffsetX);
			XMFLOAT3 AmbientColor{ MaterialData.AmbientColor() };
			if (ImGui::ColorEdit3(u8"##Ambient 색상", &AmbientColor.x, ImGuiColorEditFlags_RGB))
			{
				MaterialData.AmbientColor(AmbientColor);
			}
		}

		if (!bUsePhysicallyBasedRendering)
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text(u8"Specular 색상");
			ImGui::SameLine(ItemsOffsetX);
			XMFLOAT3 SpecularColor{ MaterialData.SpecularColor() };
			if (ImGui::ColorEdit3(u8"##Specular 색상", &SpecularColor.x, ImGuiColorEditFlags_RGB))
			{
				MaterialData.SpecularColor(SpecularColor);
			}

			ImGui::AlignTextToFramePadding();
			ImGui::Text(u8"Specular 지수");
			ImGui::SameLine(ItemsOffsetX);
			float SpecularExponent{ MaterialData.SpecularExponent() };
			if (ImGui::DragFloat(u8"##Specular 지수", &SpecularExponent, 0.1f, CMaterialData::KSpecularMinExponent, CMaterialData::KSpecularMaxExponent, "%.1f"))
			{
				MaterialData.SpecularExponent(SpecularExponent);
			}
		}

		ImGui::AlignTextToFramePadding();
		ImGui::Text(u8"Specular 강도");
		ImGui::SameLine(ItemsOffsetX);
		float SpecularIntensity{ MaterialData.SpecularIntensity() };
		if (ImGui::DragFloat(u8"##Specular 강도", &SpecularIntensity, 0.01f, 0.0f, 1.0f, "%.2f"))
		{
			MaterialData.SpecularIntensity(SpecularIntensity);
		}

		if (bUsePhysicallyBasedRendering)
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text(u8"Roughness");
			ImGui::SameLine(ItemsOffsetX);
			float Roughness{ MaterialData.Roughness() };
			if (ImGui::DragFloat(u8"##Roughness", &Roughness, 0.01f, 0.0f, 1.0f, "%.2f"))
			{
				MaterialData.Roughness(Roughness);
			}

			ImGui::AlignTextToFramePadding();
			ImGui::Text(u8"Metalness");
			ImGui::SameLine(ItemsOffsetX);
			float Metalness{ MaterialData.Metalness() };
			if (ImGui::DragFloat(u8"##Metalness", &Metalness, 0.01f, 0.0f, 1.0f, "%.2f"))
			{
				MaterialData.Metalness(Metalness);
			}
		}

		ImGui::Separator();

		ImGui::AlignTextToFramePadding();
		ImGui::Text(u8"텍스처");

		static const ImVec2 KTextureSmallViewSize{ 60.0f, 60.0f };
		ImGui::PushID(0);
		if (bUsePhysicallyBasedRendering)
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text(u8"Base color");
		}
		else
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text(u8"Diffuse");
		}
		ImGui::SameLine(ItemsOffsetX);
		if (ImGui::ImageButton((TextureSet) ? TextureSet->GetTextureSRV(STextureData::EType::DiffuseTexture) : nullptr, KTextureSmallViewSize))
		{
			eSeletedTextureType = STextureData::EType::DiffuseTexture;
			m_EditorGUIBools.bShowPopupMaterialTextureExplorer = true;
			Result = true;
		}
		ImGui::PopID();

		ImGui::PushID(1);
		ImGui::AlignTextToFramePadding();
		ImGui::Text(u8"Normal");
		ImGui::SameLine(ItemsOffsetX);
		if (ImGui::ImageButton((TextureSet) ? TextureSet->GetTextureSRV(STextureData::EType::NormalTexture) : nullptr, KTextureSmallViewSize))
		{
			eSeletedTextureType = STextureData::EType::NormalTexture;
			m_EditorGUIBools.bShowPopupMaterialTextureExplorer = true;
			Result = true;
		}
		ImGui::PopID();

		ImGui::PushID(2);
		ImGui::AlignTextToFramePadding();
		ImGui::Text(u8"Opacity");
		ImGui::SameLine(ItemsOffsetX);
		if (ImGui::ImageButton((TextureSet) ? TextureSet->GetTextureSRV(STextureData::EType::OpacityTexture) : nullptr, KTextureSmallViewSize))
		{
			eSeletedTextureType = STextureData::EType::OpacityTexture;
			m_EditorGUIBools.bShowPopupMaterialTextureExplorer = true;
			Result = true;
		}
		ImGui::PopID();

		ImGui::PushID(3);
		ImGui::AlignTextToFramePadding();
		ImGui::Text(u8"Specular Intensity");
		ImGui::SameLine(ItemsOffsetX);
		if (ImGui::ImageButton((TextureSet) ? TextureSet->GetTextureSRV(STextureData::EType::SpecularIntensityTexture) : nullptr, KTextureSmallViewSize))
		{
			eSeletedTextureType = STextureData::EType::SpecularIntensityTexture;
			m_EditorGUIBools.bShowPopupMaterialTextureExplorer = true;
			Result = true;
		}
		ImGui::PopID();

		if (bUsePhysicallyBasedRendering)
		{
			ImGui::PushID(4);
			ImGui::AlignTextToFramePadding();
			ImGui::Text(u8"Roughness");
			ImGui::SameLine(ItemsOffsetX);
			if (ImGui::ImageButton((TextureSet) ? TextureSet->GetTextureSRV(STextureData::EType::RoughnessTexture) : nullptr, KTextureSmallViewSize))
			{
				eSeletedTextureType = STextureData::EType::RoughnessTexture;
				m_EditorGUIBools.bShowPopupMaterialTextureExplorer = true;
				Result = true;
			}
			ImGui::PopID();

			ImGui::PushID(5);
			ImGui::AlignTextToFramePadding();
			ImGui::Text(u8"Metalness");
			ImGui::SameLine(ItemsOffsetX);
			if (ImGui::ImageButton((TextureSet) ? TextureSet->GetTextureSRV(STextureData::EType::MetalnessTexture) : nullptr, KTextureSmallViewSize))
			{
				eSeletedTextureType = STextureData::EType::MetalnessTexture;
				m_EditorGUIBools.bShowPopupMaterialTextureExplorer = true;
				Result = true;
			}
			ImGui::PopID();

			ImGui::PushID(6);
			ImGui::AlignTextToFramePadding();
			ImGui::Text(u8"Ambient Occlusion");
			ImGui::SameLine(ItemsOffsetX);
			if (ImGui::ImageButton((TextureSet) ? TextureSet->GetTextureSRV(STextureData::EType::AmbientOcclusionTexture) : nullptr, KTextureSmallViewSize))
			{
				eSeletedTextureType = STextureData::EType::AmbientOcclusionTexture;
				m_EditorGUIBools.bShowPopupMaterialTextureExplorer = true;
				Result = true;
			}
			ImGui::PopID();
		}

		ImGui::PushID(7);
		ImGui::AlignTextToFramePadding();
		ImGui::Text(u8"Displacement");
		ImGui::SameLine(ItemsOffsetX);
		if (ImGui::ImageButton((TextureSet) ? TextureSet->GetTextureSRV(STextureData::EType::DisplacementTexture) : nullptr, KTextureSmallViewSize))
		{
			eSeletedTextureType = STextureData::EType::DisplacementTexture;
			m_EditorGUIBools.bShowPopupMaterialTextureExplorer = true;
			Result = true;
		}
		ImGui::PopID();

		ImGui::TreePop();
	}

	return Result;
}

void CGame::DrawEditorGUIPopupMaterialNameChanger(CMaterialData*& capturedMaterialData, bool bIsEditorMaterial)
{
	static char OldName[KAssetNameMaxLength]{};
	static char NewName[KAssetNameMaxLength]{};

	// ### 재질 이름 변경 윈도우 ###
	if (m_EditorGUIBools.bShowPopupMaterialNameChanger) ImGui::OpenPopup(u8"재질 이름 변경");

	ImGui::SetNextWindowSize(ImVec2(240, 100), ImGuiCond_Always);
	if (ImGui::BeginPopupModal(u8"재질 이름 변경", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
	{
		ImGui::SetNextItemWidth(160);
		ImGui::InputText(u8"새 이름", NewName, KAssetNameMaxLength, ImGuiInputTextFlags_EnterReturnsTrue);

		ImGui::Separator();

		if (ImGui::Button(u8"결정") || ImGui::IsKeyDown(VK_RETURN))
		{
			strcpy_s(OldName, capturedMaterialData->Name().c_str());

			if (bIsEditorMaterial)
			{
				if (ChangeMaterialName(OldName, NewName))
				{
					ImGui::CloseCurrentPopup();
					m_EditorGUIBools.bShowPopupMaterialNameChanger = false;
					capturedMaterialData = nullptr;
				}
			}
			else
			{
				// TODO: 이름 충돌 검사
				// 에디터 재질이 아니면.. 이름 충돌해도 괜찮을까?

				capturedMaterialData->Name(NewName);
			}
		}

		ImGui::SameLine();

		if (ImGui::Button(u8"닫기") || ImGui::IsKeyDown(VK_ESCAPE))
		{
			ImGui::CloseCurrentPopup();
			m_EditorGUIBools.bShowPopupMaterialNameChanger = false;
			capturedMaterialData = nullptr;
		}

		ImGui::EndPopup();
	}
}

void CGame::DrawEditorGUIPopupMaterialTextureExplorer(CMaterialData* const capturedMaterialData, CMaterialTextureSet* const capturedMaterialTextureSet,
	STextureData::EType eSelectedTextureType)
{
	// ### 텍스처 탐색기 윈도우 ###
	if (m_EditorGUIBools.bShowPopupMaterialTextureExplorer) ImGui::OpenPopup(u8"텍스처탐색기");
	if (ImGui::BeginPopup(u8"텍스처탐색기", ImGuiWindowFlags_AlwaysAutoResize))
	{
		ID3D11ShaderResourceView* SRV{};
		if (capturedMaterialTextureSet) SRV = capturedMaterialTextureSet->GetTextureSRV(eSelectedTextureType);

		if (ImGui::Button(u8"파일에서 텍스처 불러오기"))
		{
			static CFileDialog FileDialog{ GetWorkingDirectory() };
			if (FileDialog.OpenFileDialog(KTextureDialogFilter, KTextureDialogTitle))
			{
				capturedMaterialData->SetTextureFileName(eSelectedTextureType, FileDialog.GetRelativeFileName());
				capturedMaterialTextureSet->CreateTexture(eSelectedTextureType, *capturedMaterialData);
			}
		}

		ImGui::SameLine();

		if (ImGui::Button(u8"텍스처 해제하기"))
		{
			capturedMaterialData->ClearTextureData(eSelectedTextureType);
			capturedMaterialTextureSet->DestroyTexture(eSelectedTextureType);
		}

		ImGui::Image(SRV, ImVec2(600, 600));

		ImGui::EndPopup();
	}
}

void CGame::DrawEditorGUIWindowSceneEditor()
{
	// ### 장면 편집기 윈도우 ###
	if (m_EditorGUIBools.bShowWindowSceneEditor)
	{
		const auto& mapObject3D{ GetObject3DMap() };
		const auto& mapObject2D{ GetObject2DMap() };
		const auto& mapCamera{ GetCameraMap() };

		ImGui::SetNextWindowPos(ImVec2(0, 122), ImGuiCond_Appearing);
		ImGui::SetNextWindowSizeConstraints(ImVec2(200, 60), ImVec2(300, 300));
		if (ImGui::Begin(u8"장면 편집기", &m_EditorGUIBools.bShowWindowSceneEditor, ImGuiWindowFlags_AlwaysAutoResize))
		{
			// 오브젝트 추가
			if (ImGui::Button(u8"오브젝트 추가"))
			{
				m_EditorGUIBools.bShowPopupObjectAdder = true;
			}

			// 오브젝트 제거
			if (ImGui::Button(u8"오브젝트 제거"))
			{
				if (IsAnyObject3DSelected())
				{
					string Name{ GetSelectedObject3DName() };
					DeleteObject3D(Name);
				}
				if (IsAnyObject2DSelected())
				{
					string Name{ GetSelectedObject2DName() };
					DeleteObject2D(Name);
				}
				if (IsAnyCameraSelected())
				{
					string Name{ GetSelectedCameraName() };
					DeleteCamera(Name);
				}
			}

			ImGui::Separator();

			ImGui::Text(u8"오브젝트");
			ImGui::Separator();

			if (ImGui::TreeNodeEx(u8"3D 오브젝트", ImGuiTreeNodeFlags_DefaultOpen))
			{
				// 3D 오브젝트 목록
				int iObject3DPair{};
				for (const auto& Object3DPair : mapObject3D)
				{
					CObject3D* const Object3D{ GetObject3D(Object3DPair.first) };
					bool bIsThisObject3DSelected{ false };
					if (IsAnyObject3DSelected())
					{
						if (GetSelectedObject3DName() == Object3DPair.first) bIsThisObject3DSelected = true;
					}

					ImGuiTreeNodeFlags Flags{ ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Leaf };
					if (bIsThisObject3DSelected) Flags |= ImGuiTreeNodeFlags_Selected;

					ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());
					bool bIsNodeOpen{ ImGui::TreeNodeEx(Object3DPair.first.c_str(), Flags) };
					if (ImGui::IsItemClicked())
					{
						DeselectAll();

						SelectObject3D(Object3DPair.first);
					}
					if (bIsNodeOpen) ImGui::TreePop();
					ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());

					++iObject3DPair;
				}

				ImGui::TreePop();
			}
			
			if (ImGui::TreeNodeEx(u8"카메라", ImGuiTreeNodeFlags_DefaultOpen))
			{
				// 카메라 목록
				int iCameraPair{};
				for (const auto& CameraPair : mapCamera)
				{
					CCamera* const Camera{ GetCamera(CameraPair.first) };
					bool bIsThisCameraSelected{ false };
					if (IsAnyCameraSelected())
					{
						if (GetSelectedCameraName() == CameraPair.first) bIsThisCameraSelected = true;
					}

					ImGuiTreeNodeFlags Flags{ ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Leaf };
					if (bIsThisCameraSelected) Flags |= ImGuiTreeNodeFlags_Selected;

					ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());
					bool bIsNodeOpen{ ImGui::TreeNodeEx(CameraPair.first.c_str(), Flags) };
					if (ImGui::IsItemClicked())
					{
						DeselectAll();

						SelectCamera(CameraPair.first);
					}
					if (bIsNodeOpen) ImGui::TreePop();
					ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());
				}

				ImGui::TreePop();
			}
		}
		ImGui::End();
	}
}

void CGame::EndRendering()
{
	if (m_IsDestroyed) return;

	// Pass-through drawing
	DrawScreenQuadToSceen(m_PSScreenQuad.get(), false);

	m_DeviceContext->OMSetRenderTargets(1, m_DeviceRTV.GetAddressOf(), m_DepthStencilView.Get());
	m_DeviceContext->ClearDepthStencilView(m_DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	if (EFLAG_HAS(m_eFlagsRendering, EFlagsRendering::Use3DGizmos))
	{
		Draw3DGizmos();
	}

	DrawEditorGUI();

	m_SwapChain->Present(0, 0);
}

void CGame::DrawScreenQuadToSceen(CShader* const PixelShader, bool bShouldClearDeviceRTV)
{
	m_DeviceContext->RSSetState(m_CommonStates->CullNone());

	// Deferred texture to screen
	m_DeviceContext->OMSetRenderTargets(1, m_DeviceRTV.GetAddressOf(), m_DepthStencilView.Get());
	if (bShouldClearDeviceRTV) m_DeviceContext->ClearRenderTargetView(m_DeviceRTV.Get(), Colors::Transparent);
	//m_DeviceContext->ClearDepthStencilView(m_DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	m_VSScreenQuad->Use();
	PixelShader->Use();

	ID3D11SamplerState* const PointSampler{ m_CommonStates->PointWrap() };
	m_DeviceContext->PSSetSamplers(0, 1, &PointSampler);
	m_DeviceContext->PSSetShaderResources(0, 1, m_ScreenQuadSRV.GetAddressOf());

	// Draw full-screen quad vertices
	m_DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_DeviceContext->IASetVertexBuffers(0, 1, m_ScreenQuadVertexBuffer.GetAddressOf(), &m_ScreenQuadVertexBufferStride, &m_ScreenQuadVertexBufferOffset);
	m_DeviceContext->Draw(6, 0);

	SetUniversalRSState();
}

Keyboard::State CGame::GetKeyState() const
{
	return m_Keyboard->GetState();
}

Mouse::State CGame::GetMouseState() const
{ 
	Mouse::State ResultState{ m_Mouse->GetState() };
	
	m_Mouse->ResetScrollWheelValue();

	return ResultState;
}

const XMFLOAT2& CGame::GetWindowSize() const
{
	return m_WindowSize;
}