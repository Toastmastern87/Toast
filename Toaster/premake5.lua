project "Toaster"
	kind "ConsoleApp"
	language "C++"
	toolset "v143"
	cppdialect "C++17"
	staticruntime "off"

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"src/**.h",
		"src/**.cpp"
	}

	includedirs
	{
		"%{wks.location}/Toast/vendor/spdlog/include",
		"%{wks.location}/Toast/src",
		"%{wks.location}/Toast/vendor",
		"%{IncludeDir.entt}",
		"%{IncludeDir.ImGuizmo}",
		"%{IncludeDir.filewatch}"
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