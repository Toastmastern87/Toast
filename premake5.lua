workspace "Toast"
	architecture "x64"
	startproject "Mars"

	configurations
	{
		"Debug",
		"Release",
		"Dist"	
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

IncludeDir = {}
IncludeDir["ImGui"] = "Toast/vendor/imgui"

group "Dependencies"
	include "Toast/vendor/imgui"

group ""

project "Toast"
	location "Toast"
	kind "SharedLib"
	language "C++"
	staticruntime "off"
	characterset "MBCS"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "tpch.h"
	pchsource "Toast/src/tpch.cpp"

	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp"
	}

	includedirs
	{
		"%{prj.name}/src",
		"%{prj.name}/vendor/spdlog/include",
		"%{IncludeDir.ImGui}"
	}

	links
	{
		"ImGui",
		"d3d11.lib"
	}

	filter "system:windows"
		cppdialect "C++17"
		systemversion "latest"

		defines
		{
			"TOAST_PLATFORM_WINDOWS",
			"TOAST_BUILD_DLL"
		}

		postbuildcommands
		{
			("{COPY} %{cfg.buildtarget.relpath} \"../bin/" .. outputdir .. "/Mars/\"")
		}

	filter "configurations:Debug"
		defines "TOAST_DEBUG"
		runtime "Debug"
		symbols "On"

	filter "configurations:Release"
		defines "TOAST_RELEASE"
		runtime "Release"
		optimize "On"

	filter "configurations:Dist"
		defines "TOAST_DIST"
		runtime "Release"
		optimize "On"

project "Mars"
	location "Mars"
	kind "ConsoleApp"
	language "C++"
	staticruntime "off"

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
		"Toast/src"
	}

	links
	{
		"Toast"
	}

	filter "system:windows"
		cppdialect "C++17"
		systemversion "latest"

		defines
		{
			"TOAST_PLATFORM_WINDOWS"
		}

	filter "configurations:Debug"
		defines "TOAST_DEBUG"
		runtime "Debug"
		symbols "On"

	filter "configurations:Release"
		defines "TOAST_RELEASE"
		runtime "Release"
		optimize "On"

	filter "configurations:Dist"
		defines "TOAST_DIST"
		runtime "Release"
		optimize "On"