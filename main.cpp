#include <chrono>
#include "Core/Game.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_impl_dx11.h"

using std::chrono::steady_clock;

static ImVec2 operator+(const ImVec2& a, const ImVec2& b)
{
	return ImVec2(a.x + b.x, a.y + b.y);
}

IMGUI_IMPL_API LRESULT  ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProc(_In_ HWND hWnd, _In_ UINT Msg, _In_ WPARAM wParam, _In_ LPARAM lParam);

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
	constexpr float KClearColor[4]{ 0.2f, 0.6f, 0.9f, 1.0f };

	char WorkingDirectory[MAX_PATH]{};
	GetCurrentDirectoryA(MAX_PATH, WorkingDirectory);
	
	CGame Game{ hInstance, XMFLOAT2(800, 600) };
	Game.CreateWin32(WndProc, TEXT("Game"), L"Asset\\dotumche_10_korean.spritefont", true);
	
	Game.SetAmbientlLight(XMFLOAT3(1, 1, 1), 0.2f);
	Game.SetDirectionalLight(XMVectorSet(0, 1, 0, 0), XMVectorSet(1, 1, 1, 1));

	Game.SetGameRenderingFlags(EFlagsGameRendering::UseLighting | EFlagsGameRendering::DrawMiniAxes | 
		EFlagsGameRendering::UseTerrainSelector | EFlagsGameRendering::DrawTerrainMaskingTexture | EFlagsGameRendering::TessellateTerrain);

	CCamera* MainCamera{ Game.AddCamera(SCameraData(ECameraType::FreeLook, XMVectorSet(0, 0, 0, 0), XMVectorSet(0, 0, 1, 0))) };
	MainCamera->SetPosition(XMVectorSet(0, 1, -2, 1));

	CGameObject3D* goTest{ Game.AddGameObject3D("Test") };
	{
		goTest->ComponentRender.PtrObject3D = Game.AddObject3D();
		XMVECTOR V0{  0, +1, 0, 1 };
		XMVECTOR V1{ +1, -1, 0, 1 };
		XMVECTOR V2{ -1, -1, 0, 1 };
		XMVECTOR C0{ 1, 0, 0, 1 };
		XMVECTOR C1{ 0, 1, 0, 1 };
		XMVECTOR C2{ 0, 0, 1, 1 };
		
		SModel Model{};
		SMesh Mesh{ GenerateTerrainBase(XMFLOAT2(10, 10)) };
		AverageNormals(Mesh);
		CalculateTangents(Mesh);
		AverageTangents(Mesh);
		CMaterial Material{};
		Material.SetbShouldGenerateAutoMipMap(true);
		Material.SetDiffuseTextureFileName("Asset\\ground.png");
		Material.SetNormalTextureFileName("Asset\\ground_normal.png");
		Material.SetDisplacementTextureFileName("Asset\\ground_displacement.png");
		Model.vMeshes.emplace_back(Mesh);
		Model.vMaterials.emplace_back(Material);
		goTest->ComponentRender.PtrObject3D->Create(Model);
		goTest->ComponentRender.PtrObject3D->ShouldTessellate(true);
	}

	CMaterial MaterialDefaultGround{};
	{
		MaterialDefaultGround.SetName("DefaultGround");
		MaterialDefaultGround.SetbShouldGenerateAutoMipMap(true);
		MaterialDefaultGround.SetDiffuseTextureFileName("Asset\\ground.png");
		MaterialDefaultGround.SetNormalTextureFileName("Asset\\ground_normal.png");
		Game.AddMaterial(MaterialDefaultGround);
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(Game.GethWnd());
	ImGui_ImplDX11_Init(Game.GetDevicePtr(), Game.GetDeviceContextPtr());

	ImGuiIO& igIO{ ImGui::GetIO() };
	igIO.Fonts->AddFontDefault();
	ImFont* igFont{ igIO.Fonts->AddFontFromFileTTF("Asset/D2Coding.ttf", 16.0f, nullptr, igIO.Fonts->GetGlyphRangesKorean()) };
	
	while (true)
	{
		static MSG Msg{};
		static char KeyDown{};
		static bool bLeftButton{ false };
		static bool bRightButton{ false };

		if (PeekMessage(&Msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (Msg.message == WM_LBUTTONDOWN) bLeftButton = true;
			if (Msg.message == WM_RBUTTONDOWN) bRightButton = true;
			if (Msg.message == WM_MOUSEMOVE)
			{
				if (Msg.wParam == MK_LBUTTON) bLeftButton = true;
				if (Msg.wParam == MK_RBUTTON) bRightButton = true;
			}
			if (Msg.message == WM_KEYDOWN) KeyDown = (char)Msg.wParam;
			if (Msg.message == WM_QUIT) break;

			TranslateMessage(&Msg);
			DispatchMessage(&Msg);
		}
		else
		{
			static steady_clock Clock{};
			long long TimeNow{ Clock.now().time_since_epoch().count() };
			static long long TimePrev{ TimeNow };
			float DeltaTimeF{ (TimeNow - TimePrev) * 0.000'000'001f };

			Game.BeginRendering(Colors::CornflowerBlue);

			// Keyboard input
			const Keyboard::State& KeyState{ Game.GetKeyState() };
			if (KeyState.Escape)
			{
				Game.Destroy();
			}
			if (KeyState.W)
			{
				MainCamera->Move(ECameraMovementDirection::Forward, DeltaTimeF * 1.0f);
			}
			if (KeyState.S)
			{
				MainCamera->Move(ECameraMovementDirection::Backward, DeltaTimeF * 1.0f);
			}
			if (KeyState.A)
			{
				MainCamera->Move(ECameraMovementDirection::Leftward, DeltaTimeF * 1.0f);
			}
			if (KeyState.D)
			{
				MainCamera->Move(ECameraMovementDirection::Rightward, DeltaTimeF * 1.0f);
			}
			if (KeyState.D1)
			{
				Game.Set3DGizmoMode(E3DGizmoMode::Translation);
			}
			if (KeyState.D2)
			{
				Game.Set3DGizmoMode(E3DGizmoMode::Rotation);
			}
			if (KeyState.D3)
			{
				Game.Set3DGizmoMode(E3DGizmoMode::Scaling);
			}

			if (KeyDown == VK_F1)
			{
				Game.ToggleGameRenderingFlags(EFlagsGameRendering::DrawWireFrame);
			}
			if (KeyDown == VK_F2)
			{
				Game.ToggleGameRenderingFlags(EFlagsGameRendering::DrawNormals);
			}
			if (KeyDown == VK_F3)
			{
				Game.ToggleGameRenderingFlags(EFlagsGameRendering::DrawMiniAxes);
			}

			// Mouse input
			const Mouse::State& MouseState{ Game.GetMouseState() };
			static int PrevMouseX{ MouseState.x };
			static int PrevMouseY{ MouseState.y };
			if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
			{
				if (MouseState.leftButton)
				{
					Game.Pick();
				}
				if (MouseState.x != PrevMouseX || MouseState.y != PrevMouseY)
				{
					if (MouseState.middleButton)
					{
						MainCamera->Rotate(MouseState.x - PrevMouseX, MouseState.y - PrevMouseY, 0.01f);
					}

					PrevMouseX = MouseState.x;
					PrevMouseY = MouseState.y;
				}
			}

			Game.Animate();
			Game.Draw(DeltaTimeF);

			ImGui_ImplDX11_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			ImGui::PushFont(igFont);

			{
				bool bShowTessellationEditor{ true };
				if (ImGui::Begin(u8"Å×¼¿·¹ÀÌ¼Ç(Tessellation)", &bShowTessellationEditor, ImGuiWindowFlags_AlwaysAutoResize))
				{
					float TessFactor{ Game.GetTessFactor() };
					if (ImGui::DragFloat(u8"Tess factor", &TessFactor, 0.1f, 0.0f, 64.0f, "%.1f"))
					{
						Game.SetTessFactor(TessFactor);
					}

					ImGui::Separator();

					static bool bRadio[3]{ true, false, false };
					if (ImGui::RadioButton(u8"Fractional Odd", bRadio[0]))
					{
						Game.SetTessellationMode(ETessellationMode::FractionalOdd);
						bRadio[0] = true;
						bRadio[1] = bRadio[2] = false;
					}
					if (ImGui::RadioButton(u8"Fractional Even", bRadio[1]))
					{
						Game.SetTessellationMode(ETessellationMode::FractionalEven);
						bRadio[1] = true;
						bRadio[0] = bRadio[2] = false;
					}
					if (ImGui::RadioButton(u8"Integer", bRadio[2]))
					{
						Game.SetTessellationMode(ETessellationMode::Integer);
						bRadio[2] = true;
						bRadio[0] = bRadio[1] = false;
					}
				}
				ImGui::End();
			}

			ImGui::PopFont();

			ImGui::Render();
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

			Game.EndRendering();

			KeyDown = 0;
			bLeftButton = false;
			bRightButton = false;
			TimePrev = TimeNow;
		}
	}

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	return 0;
}

LRESULT CALLBACK WndProc(_In_ HWND hWnd, _In_ UINT Msg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, Msg, wParam, lParam))
		return 0;

	switch (Msg)
	{
	case WM_ACTIVATEAPP:
		Keyboard::ProcessMessage(Msg, wParam, lParam);
		break;
	case WM_INPUT:
	case WM_MOUSEMOVE:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MOUSEWHEEL:
	case WM_XBUTTONDOWN:
	case WM_XBUTTONUP:
	case WM_MOUSEHOVER:
		Mouse::ProcessMessage(Msg, wParam, lParam);
		break;
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYUP:
		Keyboard::ProcessMessage(Msg, wParam, lParam);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, Msg, wParam, lParam);
	}
	return 0;
}