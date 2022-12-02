project "Mars"
	location "Mars"
	kind "SharedLib"
	language "C#"

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	files 
	{
		"Source/**.cs", 
	}

	links
	{
		"Toast-ScriptCore"
	}