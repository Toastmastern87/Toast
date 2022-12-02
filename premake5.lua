include "Dependencies.lua"

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

group "Dependencies"
	include "Toast/vendor/imgui"
	include "Toast/vendor/directxtk"
	include "Toast/vendor/yaml-cpp"
group "Dependencies/msdf"
	include "Toast/vendor/msdf-atlas-gen"
group ""

group "Core"
	include "Toast"
	include "Toast-ScriptCore"
group ""

group "Tools"
	include "Toaster"
group ""

group "Misc"
	include "Mars"
group ""