#pragma once

namespace Toast {

	typedef enum class FormatCode
	{
		R32G32B32A32_FLOAT = 2,
		D24_UNORM_S8_UINT = 45

	} Format;

	typedef enum class BindFlagCode
	{
		SHADER_RESOURCE = 0x8L,
		RENDER_TARGET = 0x20L,
		DEPTH_STENCIL = 0x40L,

	} BindFlag;
	inline BindFlag operator|(BindFlag a, BindFlag b) { return (BindFlag)((UINT)a | (UINT)b); };
}

#define TOAST_FORMAT_R32G32B32A32_FLOAT		::Toast::Format::R32G32B32A32_FLOAT
#define TOAST_FORMAT_D24_UNORM_S8_UINT		::Toast::Format::D24_UNORM_S8_UINT

#define TOAST_BIND_SHADER_RESOURCE			::Toast::BindFlag::SHADER_RESOURCE
#define TOAST_BIND_RENDER_TARGET			::Toast::BindFlag::RENDER_TARGET
#define TOAST_BIND_DEPTH_STENCIL			::Toast::BindFlag::DEPTH_STENCIL