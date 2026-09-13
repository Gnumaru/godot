#ifdef USE_GLES2_ES2
// ES 2.0 has no uint/bit ops: plain int literals, tests via mod/floor.
#define MAX_LIGHTS_PER_ITEM 16

#define M_PI 3.14159265359

#define SDF_MAX_LENGTH 16384.0

#define INSTANCE_FLAGS_LIGHT_COUNT_SHIFT 0 // 4 bits.

#define INSTANCE_FLAGS_CLIP_RECT_UV 16
#define INSTANCE_FLAGS_TRANSPOSE_RECT 32
#define INSTANCE_FLAGS_USE_MSDF 64
#define INSTANCE_FLAGS_USE_LCD 128

#define INSTANCE_FLAGS_NINEPATCH_DRAW_CENTER 256
#define INSTANCE_FLAGS_NINEPATCH_H_MODE_SHIFT 9
#define INSTANCE_FLAGS_NINEPATCH_V_MODE_SHIFT 11

#define INSTANCE_FLAGS_SHADOW_MASKED_SHIFT 13 // 16 bits.
#define INSTANCE_FLAGS_SHADOW_MASKED 8192

// 1 means enabled, 2+ means trails in use
#define BATCH_FLAGS_INSTANCING_MASK 127
#define BATCH_FLAGS_INSTANCING_HAS_COLORS_SHIFT 7
#define BATCH_FLAGS_INSTANCING_HAS_COLORS 128
#define BATCH_FLAGS_INSTANCING_HAS_CUSTOM_DATA_SHIFT 8
#define BATCH_FLAGS_INSTANCING_HAS_CUSTOM_DATA 256

#define BATCH_FLAGS_DEFAULT_NORMAL_MAP_USED 512
#define BATCH_FLAGS_DEFAULT_SPECULAR_MAP_USED 1024
#else
#define MAX_LIGHTS_PER_ITEM 16

#define M_PI 3.14159265359

#define SDF_MAX_LENGTH 16384.0

#define INSTANCE_FLAGS_LIGHT_COUNT_SHIFT 0 // 4 bits.

#define INSTANCE_FLAGS_CLIP_RECT_UV (1 << 4)
#define INSTANCE_FLAGS_TRANSPOSE_RECT (1 << 5)
#define INSTANCE_FLAGS_USE_MSDF (1 << 6)
#define INSTANCE_FLAGS_USE_LCD (1 << 7)

#define INSTANCE_FLAGS_NINEPATCH_DRAW_CENTER (1 << 8)
#define INSTANCE_FLAGS_NINEPATCH_H_MODE_SHIFT 9
#define INSTANCE_FLAGS_NINEPATCH_V_MODE_SHIFT 11

#define INSTANCE_FLAGS_SHADOW_MASKED_SHIFT 13 // 16 bits.
#define INSTANCE_FLAGS_SHADOW_MASKED (1 << INSTANCE_FLAGS_SHADOW_MASKED_SHIFT)

// 1 means enabled, 2+ means trails in use
#define BATCH_FLAGS_INSTANCING_MASK 0x7F
#define BATCH_FLAGS_INSTANCING_HAS_COLORS_SHIFT 7
#define BATCH_FLAGS_INSTANCING_HAS_COLORS (1 << BATCH_FLAGS_INSTANCING_HAS_COLORS_SHIFT)
#define BATCH_FLAGS_INSTANCING_HAS_CUSTOM_DATA_SHIFT 8
#define BATCH_FLAGS_INSTANCING_HAS_CUSTOM_DATA (1 << BATCH_FLAGS_INSTANCING_HAS_CUSTOM_DATA_SHIFT)

#define BATCH_FLAGS_DEFAULT_NORMAL_MAP_USED (1 << 9)
#define BATCH_FLAGS_DEFAULT_SPECULAR_MAP_USED (1 << 10)
#endif

// Bit tests without uint/bit ops on ES 2.0 (flags arrive as exact floats).
// Usages stay unconditional; only the macros differ per target.
#ifdef USE_GLES2_ES2
#define FLAG_TEST(v, b) (mod(floor(float(v) / float(b)), 2.0) > 0.5)
#define FLAG_LOW4(v) int(mod(float(v), 16.0))
#define FLAG_FIELD(v, d) int(mod(floor(float(v) / float(d)), 4.0))
#else
#define FLAG_TEST(v, b) bool((v) & (b))
#define FLAG_LOW4(v) ((v) & 0xF)
#define FLAG_FIELD(v, d) ((v) / (d) & 0x3)
#endif

#ifdef USE_GLES2_ES2
// ES 2.0 has no unpackHalf2x16; primitive colors arrive as two 8-bit channels
// packed into one float (R * 256 + G), exact below 2^24.
vec2 unpack_rg_ba(float pair) {
	return vec2(floor(pair / 256.0) / 255.0, mod(pair, 256.0) / 255.0);
}
#endif

// GLES2 simplification (low-end 3D): the global table was a UBO (std140);
// converted to a plain uniform array, uploaded once per program per frame from
// GlobalShaderUniforms::buffer_values. Only declared when user code references
// globals, so default programs stay small. Shader usage unchanged.
#ifdef CANVAS_GLOBALS_USED
uniform vec4 global_shader_uniforms[MAX_GLOBAL_SHADER_UNIFORMS];
#endif

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

uniform int directional_light_count;
uniform highp float tex_to_sdf;

#ifndef DISABLE_LIGHTING
#ifdef USE_GLES2_ES2
#define LIGHT_FLAGS_BLEND_MASK 196608
#define LIGHT_FLAGS_BLEND_MODE_ADD 0
#define LIGHT_FLAGS_BLEND_MODE_SUB 65536
#define LIGHT_FLAGS_BLEND_MODE_MIX 131072
#define LIGHT_FLAGS_BLEND_MODE_MASK 196608
#define LIGHT_FLAGS_HAS_SHADOW 1048576
#define LIGHT_FLAGS_FILTER_SHIFT 22
#define LIGHT_FLAGS_FILTER_MASK 12582912
#define LIGHT_FLAGS_SHADOW_NEAREST 0
#define LIGHT_FLAGS_SHADOW_PCF5 4194304
#define LIGHT_FLAGS_SHADOW_PCF13 8388608
#else
#define LIGHT_FLAGS_BLEND_MASK (3 << 16)
#define LIGHT_FLAGS_BLEND_MODE_ADD (0 << 16)
#define LIGHT_FLAGS_BLEND_MODE_SUB (1 << 16)
#define LIGHT_FLAGS_BLEND_MODE_MIX (2 << 16)
#define LIGHT_FLAGS_BLEND_MODE_MASK (3 << 16)
#define LIGHT_FLAGS_HAS_SHADOW (1 << 20)
#define LIGHT_FLAGS_FILTER_SHIFT 22
#define LIGHT_FLAGS_FILTER_MASK (3 << 22)
#define LIGHT_FLAGS_SHADOW_NEAREST (0 << 22)
#define LIGHT_FLAGS_SHADOW_PCF5 (1 << 22)
#define LIGHT_FLAGS_SHADOW_PCF13 (2 << 22)
#endif

struct Light {
	// GLES2 simplification: mat2x4 does not exist in ES 2.0; two vec4 rows
	// hold the same floats (same mat4 construction at use sites, both paths).
	vec4 texture_matrix[2]; //light to texture coordinate matrix (transposed)
	vec4 shadow_matrix[2]; //light to shadow coordinate matrix (transposed)
	vec4 color;

	vec4 shadow_color; // unpacked (C++ provides bytes as floats, both paths)
	int flags; //index to light texture (bit ops on 300es, mod/floor on ES2)
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
