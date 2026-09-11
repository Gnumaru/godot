#define MAX_LIGHTS_PER_ITEM uint(16)

#define M_PI 3.14159265359

#define SDF_MAX_LENGTH 16384.0

#define INSTANCE_FLAGS_LIGHT_COUNT_SHIFT 0 // 4 bits.

#define INSTANCE_FLAGS_CLIP_RECT_UV uint(1 << 4)
#define INSTANCE_FLAGS_TRANSPOSE_RECT uint(1 << 5)
#define INSTANCE_FLAGS_USE_MSDF uint(1 << 6)
#define INSTANCE_FLAGS_USE_LCD uint(1 << 7)

#define INSTANCE_FLAGS_NINEPATCH_DRAW_CENTER uint(1 << 8)
#define INSTANCE_FLAGS_NINEPATCH_H_MODE_SHIFT 9
#define INSTANCE_FLAGS_NINEPATCH_V_MODE_SHIFT 11

#define INSTANCE_FLAGS_SHADOW_MASKED_SHIFT 13u // 16 bits.
#define INSTANCE_FLAGS_SHADOW_MASKED uint(1 << INSTANCE_FLAGS_SHADOW_MASKED_SHIFT)

// 1 means enabled, 2+ means trails in use
#define BATCH_FLAGS_INSTANCING_MASK uint(0x7F)
#define BATCH_FLAGS_INSTANCING_HAS_COLORS_SHIFT 7
#define BATCH_FLAGS_INSTANCING_HAS_COLORS uint(1 << BATCH_FLAGS_INSTANCING_HAS_COLORS_SHIFT)
#define BATCH_FLAGS_INSTANCING_HAS_CUSTOM_DATA_SHIFT 8
#define BATCH_FLAGS_INSTANCING_HAS_CUSTOM_DATA uint(1 << BATCH_FLAGS_INSTANCING_HAS_CUSTOM_DATA_SHIFT)

#define BATCH_FLAGS_DEFAULT_NORMAL_MAP_USED uint(1 << 9)
#define BATCH_FLAGS_DEFAULT_SPECULAR_MAP_USED uint(1 << 10)

layout(std140) uniform GlobalShaderUniformData { //ubo:1
	vec4 global_shader_uniforms[MAX_GLOBAL_SHADER_UNIFORMS];
};

// GLES2 simplification (low-end 3D): CanvasData was a UBO (std140);
// converted to plain variables, one by one, like Godot 3 / ansraer fork gles2.
// GlobalShaderUniformData stays a UBO for now (256-entry array, needs restructuring).
uniform highp mat4 canvas_transform;
uniform highp mat4 screen_transform;
uniform highp mat4 canvas_normal_transform;
uniform vec4 canvas_modulation;
uniform vec2 screen_pixel_size;
uniform highp float time;
uniform bool use_pixel_snap;

uniform vec4 sdf_to_tex;
uniform vec2 screen_to_sdf;
uniform vec2 sdf_to_screen;

uniform uint directional_light_count;
uniform highp float tex_to_sdf;

#ifndef DISABLE_LIGHTING
#define LIGHT_FLAGS_BLEND_MASK uint(3 << 16)
#define LIGHT_FLAGS_BLEND_MODE_ADD uint(0 << 16)
#define LIGHT_FLAGS_BLEND_MODE_SUB uint(1 << 16)
#define LIGHT_FLAGS_BLEND_MODE_MIX uint(2 << 16)
#define LIGHT_FLAGS_BLEND_MODE_MASK uint(3 << 16)
#define LIGHT_FLAGS_HAS_SHADOW uint(1 << 20)
#define LIGHT_FLAGS_FILTER_SHIFT 22
#define LIGHT_FLAGS_FILTER_MASK uint(3 << 22)
#define LIGHT_FLAGS_SHADOW_NEAREST uint(0 << 22)
#define LIGHT_FLAGS_SHADOW_PCF5 uint(1 << 22)
#define LIGHT_FLAGS_SHADOW_PCF13 uint(2 << 22)

struct Light {
	mat2x4 texture_matrix; //light to texture coordinate matrix (transposed)
	mat2x4 shadow_matrix; //light to shadow coordinate matrix (transposed)
	vec4 color;

	uint shadow_color; // packed
	uint flags; //index to light texture
	float shadow_pixel_size;
	float height;

	vec2 position;
	float shadow_zfar_inv;
	float shadow_y_ofs;

	vec4 atlas_rect;
};

// GLES2 simplification (low-end 3D): LightData was a UBO (std140);
// converted to a plain uniform array. Combined with the 16-light render cap,
// it fits real GLES2 uniform limits. Shader usage unchanged.
uniform Light light_array[MAX_LIGHTS];
#endif // DISABLE_LIGHTING
