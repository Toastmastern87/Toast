workspace "Toast"
	architecture "x64"
	startproject "Toaster"

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
IncludeDir["entt"] = "Toast/vendor/entt/include" 
IncludeDir["yaml_cpp"] = "Toast/vendor/yaml-cpp/include" 
IncludeDir["ImGuizmo"] = "Toast/vendor/ImGuizmo"
IncludeDir["mono"] = "Toast/vendor/mono/include"
IncludeDir["cgltf"] = "Toast/vendor/cgltf/include"
IncludeDir["directxtex"] = "Toast/vendor/directxtex/include"
IncludeDir["msdf_atlas_gen"] = "Toast/vendor/msdf-atlas-gen/msdf-atlas-gen"
IncludeDir["msdfgen"] = "Toast/vendor/msdf-atlas-gen/msdfgen"

LibraryDir = {}
LibraryDir["mono"] = "vendor/mono/lib/Debug/mono-2.0-sgen.lib"
LibraryDir["directxtex"] = "vendor/directxtex/lib/DirectXTex.lib"

group "Dependencies"
	include "Toast/vendor/imgui"
	include "Toast/vendor/directxtk"
	include "Toast/vendor/yaml-cpp"
group "Dependencies/msdf"
	include "Toast/vendor/msdf-atlas-gen"
group ""

project "Toast"
	location "Toast"
	kind "StaticLib"
	language "C++"
	characterset "MBCS"
	toolset "v143"
	cppdialect "C++17"
	staticruntime "on"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "tpch.h"
	pchsource "Toast/src/tpch.cpp"

	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",

		"%{prj.name}/vendor/ImGuizmo/ImGuizmo.h",
		"%{prj.name}/vendor/ImGuizmo/ImGuizmo.cpp"
	}

	defines
	{
		"_CRT_SECURE_NO_WARNINGS"
	}

	includedirs
	{
		"%{prj.name}/src",
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
		"%{LibraryDir.directxtex}",
		"%{LibraryDir.mono}"
	}

	filter "files:Toast/vendor/ImGuizmo/**.cpp"
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

project "Toast-ScriptCore"
	location "Toast-ScriptCore"
	kind "SharedLib"
	language "C#"

	targetdir ("../Toast/Toaster/assets/scripts")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files 
	{
		"%{prj.name}/src/**.cs", 
	}
group ""

project "Toaster"
	location "Toaster"
	kind "ConsoleApp"
	language "C++"
	toolset "v143"
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
		"Toast/vendor",
		"%{IncludeDir.entt}",
		"%{IncludeDir.ImGuizmo}"
	}

	links
	{
		"Toast"
	}

--	postbuildcommands 
--	{
--		'{COPY} "%{cfg.targetdir}/assets" "../Toaster/assets"'
--	}

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

		postbuildcommands 
		{
			'{COPY} "../Toast/vendor/mono/bin/Debug/mono-2.0-sgen.dll" "%{cfg.targetdir}"'
		}


	filter "configurations:Release"
		defines "TOAST_RELEASE"
		runtime "Release"
		optimize "on"

		postbuildcommands 
		{
			'{COPY} "../Toast/vendor/mono/bin/Debug/mono-2.0-sgen.dll" "%{cfg.targetdir}"'
		}

	filter "configurations:Dist"
		defines "TOAST_DIST"
		runtime "Release"
		optimize "on"

		postbuildcommands 
		{
			'{COPY} "../Toast/vendor/mono/bin/Debug/mono-2.0-sgen.dll" "%{cfg.targetdir}"'
		}
project "Mars"
	location "Mars"
	kind "SharedLib"
	language "C#"

	targetdir ("../Toast/Toaster/assets/scripts")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files 
	{
		"%{prj.name}/src/**.cs", 
	}

	links
	{
		"Toast-ScriptCore"
	}