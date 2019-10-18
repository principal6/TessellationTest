#include <chrono>
#include "Core/Game.h"

using std::chrono::steady_clock;

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

	Game.SetGameRenderingFlags(EFlagsGameRendering::UseLighting | EFlagsGameRendering::DrawMiniAxes);

	CCamera* MainCamera{ Game.AddCamera(SCameraData(ECameraType::FreeLook, XMVectorSet(0, 0, 0, 0), XMVectorSet(0, 0, +1.0f, 0))) };

	CGameObject3D* goTri{ Game.AddGameObject3D("Tri") };
	{
		SMesh Mesh{ GenerateTriangle(XMVectorSet(0, 1, 4, 1), XMVectorSet(1, -1, 4, 1), XMVectorSet(-1, -1, 4, 1),
			XMVectorSet(1, 0, 0, 1), XMVectorSet(0, 1, 0, 1), XMVectorSet(0, 0, 1, 1)) };
		CalculateFaceNormals(Mesh);
		CalculateVertexNormalsFromFaceNormals(Mesh);

		goTri->ComponentRender.PtrObject3D = Game.AddObject3D();
		goTri->ComponentRender.PtrObject3D->Create(Mesh);
		goTri->ComponentRender.PtrObject3D->SetbUseTessellation(true);

		goTri->ComponentRender.PtrPS = Game.GetBaseShader(EBaseShader::PSVertexColor);
	}

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

			// Keyboard input
			const Keyboard::State& KeyState{ Game.GetKeyState() };
			if (KeyState.Escape)
			{
				Game.Destroy();
			}
			if (KeyState.W)
			{
				MainCamera->MoveCamera(ECameraMovementDirection::Forward, DeltaTimeF * 10.0f);
			}
			if (KeyState.S)
			{
				MainCamera->MoveCamera(ECameraMovementDirection::Backward, DeltaTimeF * 10.0f);
			}
			if (KeyState.A)
			{
				MainCamera->MoveCamera(ECameraMovementDirection::Leftward, DeltaTimeF * 10.0f);
			}
			if (KeyState.D)
			{
				MainCamera->MoveCamera(ECameraMovementDirection::Rightward, DeltaTimeF * 10.0f);
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
			if (MouseState.leftButton)
			{
				Game.Pick();
			}
			if (MouseState.x != PrevMouseX || MouseState.y != PrevMouseY)
			{
				if (MouseState.middleButton)
				{
					MainCamera->RotateCamera(MouseState.x - PrevMouseX, MouseState.y - PrevMouseY, 0.01f);
				}

				PrevMouseX = MouseState.x;
				PrevMouseY = MouseState.y;
			}

			Game.BeginRendering(Colors::CornflowerBlue);

			Game.Animate();
			Game.Draw(DeltaTimeF);

			Game.EndRendering();

			KeyDown = 0;
			bLeftButton = false;
			bRightButton = false;
			TimePrev = TimeNow;
		}
	}

	return 0;
}

LRESULT CALLBACK WndProc(_In_ HWND hWnd, _In_ UINT Msg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
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