project "Toast"
	kind "StaticLib"
	language "C++"
	toolset "v143"
	cppdialect "C++17"
	staticruntime "off"
	characterset "MBCS"

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "tpch.h"
	pchsource "src/tpch.cpp"

	files
	{
		"src/**.h",
		"src/**.cpp",

		"vendor/ImGuizmo/ImGuizmo.h",
		"vendor/ImGuizmo/ImGuizmo.cpp"
	}

	defines
	{
		"_CRT_SECURE_NO_WARNINGS"
	}

	includedirs
	{
		"src",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.directxtk}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.yaml_cpp}",
		"%{IncludeDir.ImGuizmo}",
		"%{IncludeDir.mono}",
		"%{IncludeDir.cgltf}",
		"%{IncludeDir.directxtex}",
		"%{IncludeDir.msdf_atlas_gen}",
		"%{IncludeDir.msdfgen}",
	}

	links
	{
		"ImGui",
		"DirectXTK",
		"d3d11.lib",
		"dxgi.lib",
		"dxguid.lib",
		"yaml-cpp",
		"msdf-atlas-gen",
		"%{Library.directxtex}",
		"%{Library.mono}",
		"%{Library.WinSock}",
		"%{Library.WinMM}",
		"%{Library.WinVersion}",
		"%{Library.BCrypt}",
	}

	filter "files:vendor/ImGuizmo/**.cpp"
		flags { "NoPCH" }

	filter "system:windows"
		systemversion "latest"

		defines
		{
			"TOAST_PLATFORM_WINDOWS",
			"TOAST_BUILD_DLL"
		}

	filter "configurations:Debug"
		defines "TOAST_DEBUG"
		runtime "Debug"
		symbols "on"

		libdirs
		{
			"%{LibraryDir.directxtk}/Debug-windows-x86_64/DirectXTK"
		}

	filter "configurations:Release"
		defines "TOAST_RELEASE"
		runtime "Release"
		optimize "on"

		libdirs
		{
			"%{LibraryDir.directxtk}/Release-windows-x86_64/DirectXTK"
		}

	filter "configurations:Dist"
		defines "TOAST_DIST"
		runtime "Release"
		optimize "on"

		libdirs
		{
			"%{LibraryDir.directxtk}/Release-windows-x86_64/DirectXTK"
		}