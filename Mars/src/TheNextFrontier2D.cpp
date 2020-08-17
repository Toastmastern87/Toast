#include "TheNextFrontier2D.h"

#include <imgui/imgui.h>

#include <chrono>

template<typename Fn>
class Timer
{
public:
	Timer(const char* name, Fn&& func)
		: mName(name), mFunc(func), mStopped(false)
	{
		mStartTimepoint = std::chrono::high_resolution_clock::now();
	}

	~Timer()
	{
		if (!mStopped)
			Stop();
	}

	void Stop() 
	{
		auto endTimepoint = std::chrono::high_resolution_clock::now();

		long long start = std::chrono::time_point_cast<std::chrono::microseconds>(mStartTimepoint).time_since_epoch().count();
		long long end = std::chrono::time_point_cast<std::chrono::microseconds>(endTimepoint).time_since_epoch().count();

		mStopped = true;

		float duration = (end - start) * 0.001f;

		mFunc({ mName, duration });
	}
private:
	const char* mName;
	Fn mFunc;
	std::chrono::time_point<std::chrono::steady_clock> mStartTimepoint;
	bool mStopped;
};

#define PROFILE_SCOPE(name) Timer timer##__LINE__(name, [&](ProfileResult profileResult) { mProfileResults.push_back(profileResult); })

TheNextFrontier2D::TheNextFrontier2D()
	: Layer("TheNextFrontier2D"), mCameraController(1280.0f / 720.0f, true)
{
}

void TheNextFrontier2D::OnAttach()
{
	mCheckerboardTexture = Toast::Texture2D::Create("assets/textures/Checkerboard.png");
}

void TheNextFrontier2D::OnDetach()
{
}

void TheNextFrontier2D::OnUpdate(Toast::Timestep ts)
{
	PROFILE_SCOPE("TheNextFrontier2D::OnUpdate");

	// Update
	{
		PROFILE_SCOPE("CameraController::OnUpdate");
		mCameraController.OnUpdate(ts);
	}

	// Render
	{
		PROFILE_SCOPE("Renderer Prep");
		const float clearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };

		Toast::RenderCommand::SetRenderTargets();
		Toast::RenderCommand::Clear(clearColor);
	}

	{
		PROFILE_SCOPE("Renderer Draw");
		Toast::Renderer2D::BeginScene(mCameraController.GetCamera());
		Toast::Renderer2D::DrawQuad(DirectX::XMFLOAT2(-1.0f, 0.0f), DirectX::XMFLOAT2(0.8f, 0.8f), DirectX::XMFLOAT4(0.8f, 0.2f, 0.3f, 1.0f));
		Toast::Renderer2D::DrawQuad(DirectX::XMFLOAT2(0.5f, -0.5f), DirectX::XMFLOAT2(0.5f, 0.75f), DirectX::XMFLOAT4(0.2f, 0.3f, 0.8f, 1.0f));
		Toast::Renderer2D::DrawQuad(DirectX::XMFLOAT3(0.0f, 0.0f, 0.1f), DirectX::XMFLOAT2(10.0f, 10.0f), mCheckerboardTexture);
		Toast::Renderer2D::EndScene();
	}
}

void TheNextFrontier2D::OnImGuiRender()
{
	ImGui::Begin("Settings");
	ImGui::ColorEdit4("Square Color", mSquareColor);

	for (auto& result : mProfileResults) 
	{
		char label[50];
		strcpy_s(label, "%.3fms ");
		strcat_s(label, result.Name);
		ImGui::Text(label, result.Time);
	}

	mProfileResults.clear();

	ImGui::End();
}

void TheNextFrontier2D::OnEvent(Toast::Event& e)
{
	mCameraController.OnEvent(e);
}