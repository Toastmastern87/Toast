#pragma once

namespace Toast {

	typedef enum class BindFlagCode
	{
		SHADER_RESOURCE = 0x8L,
		RENDER_TARGET = 0x20L,
		DEPTH_STENCIL = 0x40L,

	} BindFlag;
	inline BindFlag operator|(BindFlag a, BindFlag b) { return (BindFlag)((uint32_t)a | (uint32_t)b); };

	typedef enum class PrimitiveTopology
	{
		UNDEFINED = 0,
		POINTLIST = 1,
		LINELIST = 2,
		LINESTRIP = 3,
		TRIANGLELIST = 4,
		TRIANGLESTRIP = 5,
		TRIANGLELIST_ADJ = 12

	} Topology;

	typedef enum class TextureFormat
	{
		None = 0,

		// Color
		R32G32B32A32_FLOAT = 2,
		R16G16B16A16_FLOAT = 10,
		R8G8B8A8_UNORM = 28,
		R32_TYPELESS = 39,
		D32_FLOAT = 40,
		R32_FLOAT = 41,
		R32_SINT = 43,

		// Depth/stencil
		D24_UNORM_S8_UINT = 45,

		// Default
		Depth = D24_UNORM_S8_UINT
	};
}

#define TOAST_BIND_SHADER_RESOURCE			::Toast::BindFlag::SHADER_RESOURCE
#define TOAST_BIND_RENDER_TARGET			::Toast::BindFlag::RENDER_TARGET
#define TOAST_BIND_DEPTH_STENCIL			::Toast::BindFlag::DEPTH_STENCIL