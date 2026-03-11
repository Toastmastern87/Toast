-- Toast Dependencies

IncludeDir = {}
IncludeDir["ImGui"] = "%{wks.location}/Toast/vendor/imgui"
IncludeDir["entt"] = "%{wks.location}/Toast/vendor/entt/include" 
IncludeDir["yaml_cpp"] = "%{wks.location}/Toast/vendor/yaml-cpp/include" 
IncludeDir["ImGuizmo"] = "%{wks.location}/Toast/vendor/ImGuizmo"
IncludeDir["mono"] = "%{wks.location}/Toast/vendor/mono/include"
IncludeDir["cgltf"] = "%{wks.location}/Toast/vendor/cgltf/include"
IncludeDir["directxtex"] = "%{wks.location}/Toast/vendor/directxtex/include"
IncludeDir["msdf_atlas_gen"] = "%{wks.location}/Toast/vendor/msdf-atlas-gen/msdf-atlas-gen"
IncludeDir["msdfgen"] = "%{wks.location}/Toast/vendor/msdf-atlas-gen/msdfgen"
IncludeDir["filewatch"] = "%{wks.location}/Toast/vendor/filewatch"
IncludeDir["robin_hood"] = "%{wks.location}/Toast/vendor/robinhood/include" 

LibraryDir = {}
LibraryDir["mono"] = "%{wks.location}/Toast/vendor/mono/lib/%{cfg.buildcfg}"

Library = {}
Library["mono"] = "%{LibraryDir.mono}/libmono-static-sgen.lib"

-- Windows
Library["WinSock"] = "Ws2_32.lib"
Library["WinMM"] = "Winmm.lib"
Library["WinVersion"] = "Version.lib"
Library["BCrypt"] = "Bcrypt.lib"