/* clang-format off */

#[modes]

mode_default =

#[specializations]

USE_OUTPUT1 = true
USE_OUTPUT2 = true
USE_OUTPUT3 = true

#[vertex]

layout(location = 0) in vec2 vertex_attrib;

uniform vec2 offset;
uniform vec2 size;

out vec2 uv;

void main() {
	uv = vertex_attrib * 0.5 + 0.5;
	// This math scales the Vertex Attribute Quad to match the Rect the user passed in, based on Offset & Size
	gl_Position = vec4( (offset * 2.0 - 1.0) + (size * (vertex_attrib + 1.0)), 1.0, 1.0);
}

#[fragment]

uniform sampler2D source0; // texunit:0

uniform sampler2D source1; // texunit:-1

uniform sampler2D source2; // texunit:-2

uniform sampler2D source3; // texunit:-3

#ifdef USE_GLES2_ES2
#define OUTPUT0_SRGB 1
#define OUTPUT1_SRGB 2
#define OUTPUT2_SRGB 4
#define OUTPUT3_SRGB 8
#else
#define OUTPUT0_SRGB uint(1)
#define OUTPUT1_SRGB uint(2)
#define OUTPUT2_SRGB uint(4)
#define OUTPUT3_SRGB uint(8)
#endif

#ifdef USE_GLES2_ES2
uniform int convert_to_srgb;
#else
uniform uint convert_to_srgb;
#endif
uniform vec4 modulate;
uniform float time;

in vec2 uv;

#ifdef USE_GLES2_ES2
// Single ES2 render target writes gl_FragColor (see below).
#else
layout (location = 0) out vec4 out_color0;
#endif

#ifdef USE_OUTPUT1
layout (location = 1) out vec4 out_color1;
#endif

#ifdef USE_OUTPUT2
layout (location = 2) out vec4 out_color2;
#endif

#ifdef USE_OUTPUT3
layout (location = 3) out vec4 out_color3;
#endif

// GLES2 simplification: material UBO as plain uniforms (same access, no block).
#ifdef MATERIAL_UNIFORMS_USED
#MATERIAL_UNIFORMS
#endif

// GLES2 simplification: global table as plain uniform array (same access).
#ifdef TEXBLIT_GLOBALS_USED
uniform vec4 global_shader_uniforms[MAX_GLOBAL_SHADER_UNIFORMS];
#endif

#GLOBALS

vec3 linear_to_srgb(vec3 color) {
	// If going to srgb, clamp from 0 to 1.
	color = clamp(color, vec3(0.0), vec3(1.0));
	const vec3 a = vec3(0.055f);
#ifdef USE_GLES2_ES2
	// ES 2.0 mix() has no bvec overload.
	return mix((vec3(1.0f) + a) * pow(color.rgb, vec3(1.0f / 2.4f)) - a, 12.92f * color.rgb, vec3(lessThan(color.rgb, vec3(0.0031308f))));
#else
	return mix((vec3(1.0f) + a) * pow(color.rgb, vec3(1.0f / 2.4f)) - a, 12.92f * color.rgb, lessThan(color.rgb, vec3(0.0031308f)));
#endif
}

void main() {
	// Handles the case where user code uses extra outputs, but extra output targets were not bound
	vec4 color0 = vec4(0.0, 0.0, 0.0, 1.0);
	vec4 color1 = vec4(0.0, 0.0, 0.0, 1.0);
	vec4 color2 = vec4(0.0, 0.0, 0.0, 1.0);
	vec4 color3 = vec4(0.0, 0.0, 0.0, 1.0);

#CODE : BLIT

	// Discards extra outputs if extra output targets were not bound.
	// ES 2.0 has a single target: convert first (never read gl_FragColor),
	// then write once.
#ifdef USE_GLES2_ES2
	if (mod(float(convert_to_srgb / OUTPUT0_SRGB), 2.0) > 0.5) {
		color0.rgb = linear_to_srgb(color0.rgb); // Regular linear -> SRGB conversion.
	}
	gl_FragColor = color0;
#else
	out_color0 = color0;

#ifdef USE_OUTPUT1
	out_color1 = color1;
#endif
#ifdef USE_OUTPUT2
	out_color2 = color2;
#endif
#ifdef USE_OUTPUT3
	out_color3 = color3;
#endif

	if (bool(convert_to_srgb & OUTPUT0_SRGB)) {
		out_color0.rgb = linear_to_srgb(out_color0.rgb); // Regular linear -> SRGB conversion.
	}
#ifdef USE_OUTPUT1
	if (bool(convert_to_srgb & OUTPUT1_SRGB)) {
		out_color1.rgb = linear_to_srgb(out_color1.rgb);
	}
#endif
#ifdef USE_OUTPUT2
	if (bool(convert_to_srgb & OUTPUT2_SRGB)) {
		out_color2.rgb = linear_to_srgb(out_color2.rgb);
	}
#endif
#ifdef USE_OUTPUT3
	if (bool(convert_to_srgb & OUTPUT3_SRGB)) {
		out_color3.rgb = linear_to_srgb(out_color3.rgb);
	}
#endif
#endif
}
