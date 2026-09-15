// Compatibility renames. These are exposed with the "godot_" prefix
// to work around two distinct Adreno bugs:
// 1. Some Adreno devices expose ES310 functions in ES300 shaders.
//    Internally, we must use the "godot_" prefix, but user shaders
//    will be mapped automatically.
// 2. Adreno 3XX devices have poor implementations of the other packing
//    functions, so we just use our own there to keep it simple.

#ifdef USE_HALF2FLOAT
// Floating point pack/unpack functions are part of the GLSL ES 300 specification used by web and mobile.
// It appears to be safe to expose these on mobile, but when running through ANGLE this appears to break.
uint float2half(uint f) {
	uint e = f & uint(0x7f800000);
	if (e <= uint(0x38000000)) {
		return uint(0);
	} else {
		return ((f >> uint(16)) & uint(0x8000)) |
				(((e - uint(0x38000000)) >> uint(13)) & uint(0x7c00)) |
				((f >> uint(13)) & uint(0x03ff));
	}
}

uint half2float(uint h) {
	uint h_e = h & uint(0x7c00);
	return ((h & uint(0x8000)) << uint(16)) | uint((h_e >> uint(10)) != uint(0)) * (((h_e + uint(0x1c000)) << uint(13)) | ((h & uint(0x03ff)) << uint(13)));
}

uint godot_packHalf2x16(vec2 v) {
	return float2half(floatBitsToUint(v.x)) | float2half(floatBitsToUint(v.y)) << uint(16);
}

vec2 godot_unpackHalf2x16(uint v) {
	return vec2(uintBitsToFloat(half2float(v & uint(0xffff))),
			uintBitsToFloat(half2float(v >> uint(16))));
}

uint godot_packUnorm2x16(vec2 v) {
	uvec2 uv = uvec2(round(clamp(v, vec2(0.0), vec2(1.0)) * 65535.0));
	return uv.x | uv.y << uint(16);
}

vec2 godot_unpackUnorm2x16(uint p) {
	return vec2(float(p & uint(0xffff)), float(p >> uint(16))) * 0.000015259021; // 1.0 / 65535.0 optimization
}

uint godot_packSnorm2x16(vec2 v) {
	uvec2 uv = uvec2(round(clamp(v, vec2(-1.0), vec2(1.0)) * 32767.0) + 32767.0);
	return uv.x | uv.y << uint(16);
}

vec2 godot_unpackSnorm2x16(uint p) {
	vec2 v = vec2(float(p & uint(0xffff)), float(p >> uint(16)));
	return clamp((v - 32767.0) * vec2(0.00003051851), vec2(-1.0), vec2(1.0));
}

#define packHalf2x16 godot_packHalf2x16
#define unpackHalf2x16 godot_unpackHalf2x16
#define packUnorm2x16 godot_packUnorm2x16
#define unpackUnorm2x16 godot_unpackUnorm2x16
#define packSnorm2x16 godot_packSnorm2x16
#define unpackSnorm2x16 godot_unpackSnorm2x16

#endif // USE_HALF2FLOAT

// Always expose these as they are ES310 functions and not available in ES300 or GLSL 330.
#ifndef USE_GLES2_ES2
// (ES 2.0 has no uint/bit ops; canvas unpacks on the CPU side instead.)

uint godot_packUnorm4x8(vec4 v) {
	uvec4 uv = uvec4(round(clamp(v, vec4(0.0), vec4(1.0)) * 255.0));
	return uv.x | (uv.y << uint(8)) | (uv.z << uint(16)) | (uv.w << uint(24));
}

vec4 godot_unpackUnorm4x8(uint p) {
	return vec4(float(p & uint(0xff)), float((p >> uint(8)) & uint(0xff)), float((p >> uint(16)) & uint(0xff)), float(p >> uint(24))) * 0.00392156862; // 1.0 / 255.0
}

uint godot_packSnorm4x8(vec4 v) {
	uvec4 uv = uvec4(round(clamp(v, vec4(-1.0), vec4(1.0)) * 127.0) + 127.0);
	return uv.x | uv.y << uint(8) | uv.z << uint(16) | uv.w << uint(24);
}

vec4 godot_unpackSnorm4x8(uint p) {
	vec4 v = vec4(float(p & uint(0xff)), float((p >> uint(8)) & uint(0xff)), float((p >> uint(16)) & uint(0xff)), float(p >> uint(24)));
	return clamp((v - vec4(127.0)) * vec4(0.00787401574), vec4(-1.0), vec4(1.0));
}

#define packUnorm4x8 godot_packUnorm4x8
#define unpackUnorm4x8 godot_unpackUnorm4x8
#define packSnorm4x8 godot_packSnorm4x8
#define unpackSnorm4x8 godot_unpackSnorm4x8
#endif // !USE_GLES2_ES2

#ifdef USE_GLES2_ES2
// ES 2.0 has no uint/bit ops. Layer/bake tests below match the integer bit
// tests of the 300 es path (operands are small non-negative ints).
bool mask_overlaps_mask(int a, int b) {
	float fa = float(a);
	float fb = float(b);
	for (int k = 0; k < 20; k++) {
		if (mod(floor(fa / exp2(float(k))), 2.0) > 0.5 && mod(floor(fb / exp2(float(k))), 2.0) > 0.5) {
			return true;
		}
	}
	return false;
}
#define MASK_OVERLAP(a, b) mask_overlaps_mask(a, b)
#define BAKE_STATIC_SET(v) (mod(floor(float(v) / 2.0), 2.0) > 0.5)
#define BAKE_DYNAMIC_SET(v) (mod(floor(float(v) / 4.0), 2.0) > 0.5)
#define LIGHT_ENABLED_SET(v) (mod(float(v), 2.0) > 0.5)
#else
#define MASK_OVERLAP(a, b) bool((a) & (b))
#define BAKE_STATIC_SET(v) bool((v) & DIRECTIONAL_LIGHT_BAKE_STATIC)
#define BAKE_DYNAMIC_SET(v) bool((v) & DIRECTIONAL_LIGHT_BAKE_DYNAMIC)
#define LIGHT_ENABLED_SET(v) bool((v) & DIRECTIONAL_LIGHT_ENABLED)
#endif // USE_GLES2_ES2

#ifdef USE_GLES2_ES2
// ES 2.0 has no transpose()/inverse(): adjugate over determinant.
// Returns transpose(inverse(m)) (the normal matrix); columns are the
// adjugate rows, i.e. the transposed cofactor matrix over the determinant.
mat3 godot_transpose_inverse(mat3 m) {
	vec3 c0 = m[0];
	vec3 c1 = m[1];
	vec3 c2 = m[2];
	vec3 r0 = vec3(c1.y * c2.z - c1.z * c2.y, c1.z * c2.x - c1.x * c2.z, c1.x * c2.y - c1.y * c2.x);
	vec3 r1 = vec3(c2.y * c0.z - c2.z * c0.y, c2.z * c0.x - c2.x * c0.z, c2.x * c0.y - c2.y * c0.x);
	vec3 r2 = vec3(c0.y * c1.z - c0.z * c1.y, c0.z * c1.x - c0.x * c1.z, c0.x * c1.y - c0.y * c1.x);
	float det = dot(c0, r0);
	det = det == 0.0 ? 1.0 : det;
	return mat3(r0, r1, r2) / det;
}
#endif // USE_GLES2_ES2
