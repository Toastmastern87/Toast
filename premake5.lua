workspace "Toast"
	architecture "x64"
	startproject "Mars"

	configurations
	{
		"Debug",
		"Release",
		"Dist"	
	}

	flags
	{
		"MultiProcessorCompile"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

IncludeDir = {}
IncludeDir["ImGui"] = "Toast/vendor/imgui"
IncludeDir["directxtk"] = "Toast/vendor/directxtk/Inc" 

LibraryDir = {}
LibraryDir["directxtk"] = "Toast/vendor/directxtk/Bin/Desktop_2019/x64"

group "Dependencies"
	include "Toast/vendor/imgui"

group ""

project "Toast"
	location "Toast"
	kind "StaticLib"
	language "C++"
	characterset "MBCS"
	cppdialect "C++17"
	staticruntime "on"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "tpch.h"
	pchsource "Toast/src/tpch.cpp"

	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp"
	}

	defines
	{
		"_CRT_SECURE_NO_WARNINGS"
	}

	includedirs
	{
		"%{prj.name}/src",
		"%{prj.name}/vendor/spdlog/include",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.directxtk}"
	}

	libdirs
    {
		"%{LibraryDir.directxtk}/Debug"
    }

	links
	{
		"ImGui",
		"d3d11.lib",
		"dxgi.lib",
		"dxguid.lib",
		"DirectXTK.lib"
	}

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

	filter "configurations:Release"
		defines "TOAST_RELEASE"
		runtime "Release"
		optimize "on"

	filter "configurations:Dist"
		defines "TOAST_DIST"
		runtime "Release"
		optimize "on"

project "Mars"
	location "Mars"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "on"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp"
	}

	includedirs
	{
		"Toast/vendor/spdlog/include",
		"Toast/src",
		"Toast/vendor"
	}

	links
	{
		"Toast"
	}

	filter "system:windows"
		systemversion "latest"

		defines
		{
			"TOAST_PLATFORM_WINDOWS"
		}

	filter "configurations:Debug"
		defines "TOAST_DEBUG"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		defines "TOAST_RELEASE"
		runtime "Release"
		optimize "on"

	filter "configurations:Dist"
		defines "TOAST_DIST"
		runtime "Release"
		optimize "on"