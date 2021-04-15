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
}

#define TOAST_BIND_SHADER_RESOURCE			::Toast::BindFlag::SHADER_RESOURCE
#define TOAST_BIND_RENDER_TARGET			::Toast::BindFlag::RENDER_TARGET
#define TOAST_BIND_DEPTH_STENCIL			::Toast::BindFlag::DEPTH_STENCIL