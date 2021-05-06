#pragma once

//For use by game using the Toast engine

#include "Toast/Core/Base.h"

#include "Toast/Core/Application.h"
#include "Toast/Core/Layer.h"
#include "Toast/Core/Log.h"

#include "Toast/Core/Timestep.h"

#include "Toast/Core/Input.h"
#include "Toast/Core/KeyCodes.h"
#include "Toast/Core/MouseCodes.h"

#include "Toast/ImGui/ImGuiLayer.h"

#include "Toast/Scene/Scene.h"
#include "Toast/Scene/Entity.h"
#include "Toast/Scene/Components.h"
#include "Toast/Scene/SceneSerializer.h"

//-------Renderer------------------------
#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/Renderer2D.h"
#include "Toast/Renderer/RenderCommand.h"

#include "Toast/Renderer/Buffer.h"
#include "Toast/Renderer/Shader.h"
#include "Toast/Renderer/Framebuffer.h"
#include "Toast/Renderer/Texture.h"
#include "Toast/Renderer/Mesh.h"
#include "Toast/Renderer/Primitives.h"
#include "Toast/Renderer/Material.h"

#include "Toast/Renderer/OrthographicCamera.h"
#include "Toast/Renderer/EditorCamera.h"
//---------------------------------------

//-------Scripting-----------------------
#include "Toast/Script/ScriptEngine.h"
#include "Toast/Script/ScriptEngineRegistry.h"
#include "Toast/Script/ScriptWrappers.h"
//---------------------------------------