/* clang-format off */
#[modes]
mode_default =

#[specializations]

USE_MULTIVIEW = false
USE_GLOW = false
USE_GLOW_ADDITIVE = false
USE_GLOW_SOFTLIGHT = false
USE_GLOW_REPLACE = false
USE_GLOW_MIX = false
USE_FXAA = false
USE_LUMINANCE_MULTIPLIER = false
USE_BCS = false
USE_COLOR_CORRECTION = false
USE_1D_LUT = false
USE_SSAO_ABYSS = false
USE_SSAO_LOW = false
USE_SSAO_MED = false
USE_SSAO_HIGH = false
USE_SSAO_MEGA = false

#[vertex]
layout(location = 0) in vec2 vertex_attrib;

/* clang-format on */

out vec2 uv_interp;

void main() {
	uv_interp = vertex_attrib * 0.5 + 0.5;
	gl_Position = vec4(vertex_attrib, 1.0, 1.0);
}

/* clang-format off */
#[fragment]
/* clang-format on */

// If we reach this code, we always tonemap.
#define APPLY_TONEMAPPING

#include "../tonemap_inc.glsl"

#ifdef USE_MULTIVIEW
uniform sampler2DArray source_color; // texunit:0
#else
uniform sampler2D source_color; // texunit:0
#endif // USE_MULTIVIEW

uniform float view;
uniform float luminance_multiplier;

#ifdef USE_GLOW
uniform sampler2D glow_color; // texunit:1
uniform sampler2D glow_color1; // texunit:4
uniform sampler2D glow_color2; // texunit:5
uniform sampler2D glow_color3; // texunit:6
uniform float glow_intensity;
uniform float srgb_white;
uniform vec4 glow_weights;
#endif // USE_GLOW

#if defined(USE_GLOW) || defined(USE_FXAA)
uniform vec2 pixel_size;
#endif

#ifdef USE_FXAA
// FXAA 3.11 console version, ported from Godot 3 GLES2 tonemap.glsl.
// Applied before glow to preserve the "bleed" effect of glow.
// Plain function (not a #define): #-lines skip the ES2 translator,
// so textureLod must stay in translatable code.
vec4 fxaa_sample(vec2 uv) {
#ifdef USE_MULTIVIEW
	return textureLod(source_color, vec3(uv, view), 0.0);
#else
	return textureLod(source_color, uv, 0.0);
#endif
}
#define FXAA_SOURCE_SAMPLE(m_uv) fxaa_sample(m_uv)

vec4 apply_fxaa(vec4 color, vec2 uv_interp, vec2 pixel_size) {
	const float FXAA_REDUCE_MIN = (1.0 / 128.0);
	const float FXAA_REDUCE_MUL = (1.0 / 8.0);
	const float FXAA_SPAN_MAX = 8.0;
	const vec3 luma = vec3(0.299, 0.587, 0.114);

	// Godot 3 style DISABLE_ALPHA: the 3D internal buffer carries no
	// meaningful alpha (it reads back as 0 in the linear HDR path, which
	// would collapse luma and zero the output), so FXAA works opaque.
	color.a = 1.0;

	vec4 rgbNW = FXAA_SOURCE_SAMPLE(uv_interp + vec2(-0.5, -0.5) * pixel_size);
	vec4 rgbNE = FXAA_SOURCE_SAMPLE(uv_interp + vec2(0.5, -0.5) * pixel_size);
	vec4 rgbSW = FXAA_SOURCE_SAMPLE(uv_interp + vec2(-0.5, 0.5) * pixel_size);
	vec4 rgbSE = FXAA_SOURCE_SAMPLE(uv_interp + vec2(0.5, 0.5) * pixel_size);
	vec3 rgbM = color.rgb;

	float lumaNW = dot(rgbNW.rgb, luma);
	float lumaNE = dot(rgbNE.rgb, luma);
	float lumaSW = dot(rgbSW.rgb, luma);
	float lumaSE = dot(rgbSE.rgb, luma);
	float lumaM = dot(rgbM, luma);

	float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
	float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

	vec2 dir;
	dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
	dir.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));

	float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) *
					(0.25 * FXAA_REDUCE_MUL),
			FXAA_REDUCE_MIN);

	float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
	dir = min(vec2(FXAA_SPAN_MAX, FXAA_SPAN_MAX),
				  max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
						 dir * rcpDirMin)) *
			pixel_size;

	vec4 rgbA = 0.5 * (FXAA_SOURCE_SAMPLE(uv_interp + dir * (1.0 / 3.0 - 0.5)) + FXAA_SOURCE_SAMPLE(uv_interp + dir * (2.0 / 3.0 - 0.5)));
	vec4 rgbB = rgbA * 0.5 + 0.25 * (FXAA_SOURCE_SAMPLE(uv_interp + dir * -0.5) + FXAA_SOURCE_SAMPLE(uv_interp + dir * 0.5));

	float lumaB = dot(rgbB.rgb, luma);
	vec4 color_output = ((lumaB < lumaMin) || (lumaB > lumaMax)) ? rgbA : rgbB;
	return vec4(color_output.rgb, 1.0);
}
#endif // USE_FXAA

#ifdef USE_GLOW
// 8-tap tent over one glow stage (dual-filtering upsample kernel,
// same as the old single-stage composite).
vec4 glow_tent(sampler2D tex, vec2 uv, vec2 ps) {
	vec2 half_pixel = ps * 0.5;

	vec4 color = textureLod(tex, uv + vec2(-half_pixel.x * 2.0, 0.0), 0.0);
	color += textureLod(tex, uv + vec2(-half_pixel.x, half_pixel.y), 0.0) * 2.0;
	color += textureLod(tex, uv + vec2(0.0, half_pixel.y * 2.0), 0.0);
	color += textureLod(tex, uv + vec2(half_pixel.x, half_pixel.y), 0.0) * 2.0;
	color += textureLod(tex, uv + vec2(half_pixel.x * 2.0, 0.0), 0.0);
	color += textureLod(tex, uv + vec2(half_pixel.x, -half_pixel.y), 0.0) * 2.0;
	color += textureLod(tex, uv + vec2(0.0, -half_pixel.y * 2.0), 0.0);
	color += textureLod(tex, uv + vec2(-half_pixel.x, -half_pixel.y), 0.0) * 2.0;

	return color / 12.0;
}

// Godot 3 style multiband composite: each pyramid stage holds one blur
// band (filter/downsample only, no upsample merge), weighted by the
// environment glow levels. Stage k is 2^(k+1) times smaller than the
// source, hence the scaled pixel sizes.
vec4 get_glow_color(vec2 uv) {
	vec4 color = glow_tent(glow_color, uv, pixel_size) * glow_weights.x;
	color += glow_tent(glow_color1, uv, pixel_size * 2.0) * glow_weights.y;
	color += glow_tent(glow_color2, uv, pixel_size * 4.0) * glow_weights.z;
	color += glow_tent(glow_color3, uv, pixel_size * 8.0) * glow_weights.w;

#ifdef USE_LUMINANCE_MULTIPLIER
	color = color / luminance_multiplier;
#endif

	return color;
}
#endif // USE_GLOW

#ifdef USE_COLOR_CORRECTION
#ifdef USE_1D_LUT
uniform sampler2D source_color_correction; //texunit:2

vec3 apply_color_correction(vec3 color) {
	color.r = texture(source_color_correction, vec2(color.r, 0.0f)).r;
	color.g = texture(source_color_correction, vec2(color.g, 0.0f)).g;
	color.b = texture(source_color_correction, vec2(color.b, 0.0f)).b;
	return color;
}
#else
uniform sampler3D source_color_correction; //texunit:2

vec3 apply_color_correction(vec3 color) {
	return textureLod(source_color_correction, color, 0.0).rgb;
}
#endif // USE_1D_LUT
#endif // USE_COLOR_CORRECTION

#if defined(USE_SSAO_ABYSS) || defined(USE_SSAO_LOW) || defined(USE_SSAO_MED) || defined(USE_SSAO_HIGH) || defined(USE_SSAO_MEGA)
#define USE_SOME_SSAO
uniform float ssao_intensity;
uniform float ssao_radius_frac;
uniform vec2 ssao_prn_UV;
#ifdef USE_MULTIVIEW
// VR will have 2 depth buffers.
uniform sampler2DArray depth_buffer_array; // texunit:3
#else
uniform sampler2D depth_buffer; // texunit:3
#endif
#if defined(USE_SSAO_ABYSS)
// Use the tiny 2-sample version.
#include "../s4ao_micro_inc.glsl"
#elif defined(USE_SSAO_HIGH) || defined(USE_SSAO_MEGA)
// Use the rings version for the higher qualities.
#include "../s4ao_mega_inc.glsl"
#else
// Use the more generic NxN grid version.
#include "../s4ao_inc.glsl"
#endif
#endif

in vec2 uv_interp;

layout(location = 0) out vec4 frag_color;

void main() {
#ifdef USE_MULTIVIEW
	vec4 color = texture(source_color, vec3(uv_interp, view));
#else
	vec4 color = texture(source_color, uv_interp);
#endif

#ifdef USE_FXAA
	// FXAA must be performed before glow to preserve the "bleed" effect of glow.
	// Note: this runs on the raw source (before the luminance_multiplier
	// divide below) so rgbM matches the re-sampled neighbors; scaling a
	// divided rgbM against raw samples would darken the output by lum.
	color = apply_fxaa(color, uv_interp, pixel_size);
#endif

#ifdef USE_LUMINANCE_MULTIPLIER
	color = color / luminance_multiplier;
#endif

#ifdef USE_GLOW
	// Glow blending is performed before srgb_to_linear because
	// the glow texture was created from a nonlinear sRGB-encoded
	// scene, so it only makes sense to add this glow to an equally
	// nonlinear sRGB-encoded scene.

	vec4 glow = get_glow_color(uv_interp) * glow_intensity;

#if defined(USE_GLOW_ADDITIVE)
	// Godot 3 style additive (its default when no blend mode is selected).
	color.rgb += glow.rgb;
#elif defined(USE_GLOW_SOFTLIGHT)
	// Godot 3 style softlight.
	vec3 soft_glow = glow.rgb * vec3(0.5) + vec3(0.5);

	color.r = (soft_glow.r <= 0.5) ? (color.r - (1.0 - 2.0 * soft_glow.r) * color.r * (1.0 - color.r)) : (((soft_glow.r > 0.5) && (color.r <= 0.25)) ? (color.r + (2.0 * soft_glow.r - 1.0) * (4.0 * color.r * (4.0 * color.r + 1.0) * (color.r - 1.0) + 7.0 * color.r)) : (color.r + (2.0 * soft_glow.r - 1.0) * (sqrt(color.r) - color.r)));
	color.g = (soft_glow.g <= 0.5) ? (color.g - (1.0 - 2.0 * soft_glow.g) * color.g * (1.0 - color.g)) : (((soft_glow.g > 0.5) && (color.g <= 0.25)) ? (color.g + (2.0 * soft_glow.g - 1.0) * (4.0 * color.g * (4.0 * color.g + 1.0) * (color.g - 1.0) + 7.0 * color.g)) : (color.g + (2.0 * soft_glow.g - 1.0) * (sqrt(color.g) - color.g)));
	color.b = (soft_glow.b <= 0.5) ? (color.b - (1.0 - 2.0 * soft_glow.b) * color.b * (1.0 - color.b)) : (((soft_glow.b > 0.5) && (color.b <= 0.25)) ? (color.b + (2.0 * soft_glow.b - 1.0) * (4.0 * color.b * (4.0 * color.b + 1.0) * (color.b - 1.0) + 7.0 * color.b)) : (color.b + (2.0 * soft_glow.b - 1.0) * (sqrt(color.b) - color.b)));
#elif defined(USE_GLOW_REPLACE)
	color.rgb = glow.rgb;
#elif defined(USE_GLOW_MIX)
	// Same semantics as the Forward/Mobile renderer MIX mode.
	color.rgb = color.rgb * (1.0 - glow_intensity) + glow.rgb;
#else
	// Glow always uses the screen blend mode in the Compatibility renderer:

	// Glow cannot be above 1.0 after normalizing and should be non-negative
	// to produce expected results. It is possible that glow can be negative
	// if negative lights were used in the scene.
	// We clamp to srgb_white because glow will be normalized to this range.
	// Note: srgb_white cannot be smaller than the maximum output value (1.0).
	glow.rgb = clamp(glow.rgb, 0.0, srgb_white);

	// Normalize to srgb_white range.
	//glow.rgb /= srgb_white;
	//color.rgb /= srgb_white;
	//color.rgb = (color.rgb + glow.rgb) - (color.rgb * glow.rgb);
	// Expand back to original range.
	//color.rgb *= srgb_white;

	// The following is a mathematically simplified version of the above.
	color.rgb = color.rgb + glow.rgb - (color.rgb * glow.rgb / srgb_white);
#endif // glow blend mode
#endif // USE_GLOW

	color.rgb = srgb_to_linear(color.rgb);

#if defined(USE_SOME_SSAO)
	// Putting SSAO after the conversion to linear color, though it might be better before the glow.
	color.rgb *= s4ao(uv_interp); // The USE_SSAO_X controls the number of samples.
#endif

	color.rgb = apply_tonemapping(color.rgb);

#ifdef USE_BCS
	// Apply brightness:
	// Apply to relative luminance. This ensures that the hue and saturation of
	// colors is not affected by the adjustment, but requires the multiplication
	// to be performed on linear-encoded values.
	color.rgb = color.rgb * brightness;

	color.rgb = linear_to_srgb(color.rgb);

	// Apply contrast:
	// By applying contrast to RGB values that are perceptually uniform (nonlinear),
	// the darkest values are not hard-clipped as badly, which produces a
	// higher quality contrast adjustment and maintains compatibility with
	// existing projects.
	color.rgb = mix(vec3(0.5), color.rgb, contrast);

	// Apply saturation:
	// By applying saturation adjustment to nonlinear sRGB-encoded values with
	// even weights the preceived brightness of blues are affected, but this
	// maintains compatibility with existing projects.
	color.rgb = mix(vec3(dot(vec3(1.0), color.rgb) * (1.0 / 3.0)), color.rgb, saturation);
#else
	color.rgb = linear_to_srgb(color.rgb);
#endif // USE_BCS

#ifdef USE_COLOR_CORRECTION
	color.rgb = apply_color_correction(color.rgb);
#endif

	frag_color = color;

#ifdef USE_GLES2_ES2
	// Single ES2 render target (frag_color is a plain global there).
	gl_FragColor = frag_color;
#endif
}
