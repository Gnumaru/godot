/**************************************************************************/
/*  rasterizer_scene_gles2.h                                              */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#ifdef GLES2_ENABLED

#include "core/math/projection.h"
#include "core/templates/paged_allocator.h"
#include "core/templates/rid_owner.h"
#include "core/templates/self_list.h"
#include "drivers/gles2/storage/light_storage.h"
#include "drivers/gles2/storage/material_storage.h"
#include "servers/rendering/renderer_scene_render.h"
#include "servers/rendering/rendering_server_enums.h"
#include "servers/rendering/rendering_server_types.h"

class RenderSceneBuffersGLES2;

enum RenderListTypeGLES2 {
	RENDER_LIST_OPAQUE_GLES2, //used for opaque objects
	RENDER_LIST_ALPHA_GLES2, //used for transparent objects
	RENDER_LIST_SECONDARY_GLES2, //used for shadows and other objects
	RENDER_LIST_MAX_GLES2
};

enum PassModeGLES2 {
	PASS_MODE_COLOR_GLES2,
	PASS_MODE_COLOR_TRANSPARENT_GLES2,
	PASS_MODE_SHADOW_GLES2,
	PASS_MODE_DEPTH_GLES2,
	PASS_MODE_MATERIAL_GLES2,
	PASS_MODE_MOTION_VECTORS_GLES2,
};

// GLES2 simplification (low-end 3D): 3D light/shadow UBOs are plain uniform
// arrays with small caps (mirrors the canvas light cap). Extra scene lights
// beyond these caps are ignored.
enum SceneLightCapGLES2 {
	MAX_OMNI_LIGHTS_GLES2 = 8,
	MAX_SPOT_LIGHTS_GLES2 = 8,
	MAX_AREA_LIGHTS_GLES2 = 4,
	MAX_POSITIONAL_SHADOWS_GLES2 = 16,
	LIGHT_DATA_MEMBER_COUNT_GLES2 = 13, // position, inv_radius, direction, size, color, attenuation, cone_attenuation, cone_angle, specular_amount, shadow_opacity, bake_mode, area_width, area_height
	DIRECTIONAL_LIGHT_MEMBER_COUNT_GLES2 = 8, // direction, energy, color, size, enabled_bake_mode, shadow_opacity, specular, mask
	POSITIONAL_SHADOW_MEMBER_COUNT_GLES2 = 4, // shadow_matrix, light_position, shadow_normal_bias, shadow_atlas_pixel_size
	DIRECTIONAL_SHADOW_MEMBER_COUNT_GLES2 = 10, // direction, shadow_atlas_pixel_size, shadow_normal_bias, shadow_split_offsets, shadow_matrix1-4, fade_from, fade_to
};

// These should share as much as possible with SkyUniform Location
enum SceneUniformLocationGLES2 {
	SCENE_TONEMAP_UNIFORM_LOCATION_GLES2,
	SCENE_GLOBALS_UNIFORM_LOCATION_GLES2,
	SCENE_DATA_UNIFORM_LOCATION_GLES2,
	SCENE_MATERIAL_UNIFORM_LOCATION_GLES2,
	SCENE_EMPTY1_GLES2, // Unused, put here to avoid conflicts with SKY_DIRECTIONAL_LIGHT_UNIFORM_LOCATION_GLES2.
	SCENE_OMNILIGHT_UNIFORM_LOCATION_GLES2,
	SCENE_SPOTLIGHT_UNIFORM_LOCATION_GLES2,
	SCENE_AREALIGHT_UNIFORM_LOCATION_GLES2,
	SCENE_DIRECTIONAL_LIGHT_UNIFORM_LOCATION_GLES2,
	SCENE_MULTIVIEW_UNIFORM_LOCATION_GLES2,
	SCENE_POSITIONAL_SHADOW_UNIFORM_LOCATION_GLES2,
	SCENE_DIRECTIONAL_SHADOW_UNIFORM_LOCATION_GLES2,
	SCENE_EMPTY2_GLES2, // Unused, put here to avoid conflicts with SKY_MULTIVIEW_UNIFORM_LOCATION_GLES2.
	SCENE_PREV_DATA_UNIFORM_LOCATION_GLES2,
	SCENE_PREV_MULTIVIEW_UNIFORM_LOCATION_GLES2,
};

enum SkyUniformLocationGLES2 {
	SKY_TONEMAP_UNIFORM_LOCATION_GLES2,
	SKY_GLOBALS_UNIFORM_LOCATION_GLES2,
	SKY_EMPTY1_GLES2, // Unused, put here to avoid conflicts with SCENE_DATA_UNIFORM_LOCATION_GLES2.
	SKY_MATERIAL_UNIFORM_LOCATION_GLES2,
	SKY_DIRECTIONAL_LIGHT_UNIFORM_LOCATION_GLES2,
	SKY_EMPTY2_GLES2, // Unused, put here to avoid conflicts with SCENE_OMNILIGHT_UNIFORM_LOCATION_GLES2.
	SKY_EMPTY3_GLES2, // Unused, put here to avoid conflicts with SCENE_SPOTLIGHT_UNIFORM_LOCATION_GLES2.
	SKY_EMPTY4_GLES2, // Unused, put here to avoid conflicts with SCENE_AREALIGHT_UNIFORM_LOCATION_GLES2.
	SKY_EMPTY5_GLES2, // Unused, put here to avoid conflicts with SCENE_DIRECTIONAL_LIGHT_UNIFORM_LOCATION_GLES2.
	SKY_EMPTY6_GLES2, // Unused, put here to avoid conflicts with SCENE_MULTIVIEW_UNIFORM_LOCATION_GLES2.
	SKY_EMPTY7_GLES2, // Unused, put here to avoid conflicts with SCENE_POSITIONAL_SHADOW_UNIFORM_LOCATION_GLES2.
	SKY_EMPTY8_GLES2, // Unused, put here to avoid conflicts with SCENE_DIRECTIONAL_SHADOW_UNIFORM_LOCATION_GLES2.
	SKY_MULTIVIEW_UNIFORM_LOCATION_GLES2,
	SKY_EMPTY9_GLES2, // Unused, put here to avoid conflicts with SCENE_PREV_DATA_UNIFORM_LOCATION_GLES2.
	SKY_EMPTY10_GLES2, // Unused, put here to avoid conflicts with SCENE_PREV_MULTIVIEW_UNIFORM_LOCATION_GLES2.
};

struct RenderDataGLES2 {
	Ref<RenderSceneBuffersGLES2> render_buffers;
	bool transparent_bg = false;
	Rect2i render_region;

	Transform3D cam_transform;
	Transform3D inv_cam_transform;
	Projection cam_projection;
	bool cam_orthogonal = false;
	uint32_t camera_visible_layers = 0xFFFFFFFF;

	// For billboards to cast correct shadows.
	Transform3D main_cam_transform;

	// For stereo rendering
	uint32_t view_count = 1;
	Vector3 view_eye_offset[RendererSceneRender::MAX_RENDER_VIEWS];
	Projection view_projection[RendererSceneRender::MAX_RENDER_VIEWS];

	float z_near = 0.0;
	float z_far = 0.0;

	const PagedArray<RenderGeometryInstance *> *instances = nullptr;
	const PagedArray<RID> *lights = nullptr;
	const PagedArray<RID> *reflection_probes = nullptr;
	RID environment;
	RID camera_attributes;
	RID shadow_atlas;
	RID reflection_probe;
	int reflection_probe_pass = 0;

	float lod_distance_multiplier = 0.0;
	float screen_mesh_lod_threshold = 0.0;

	uint32_t directional_light_count = 0;
	uint32_t directional_shadow_count = 0;

	uint32_t spot_light_count = 0;
	uint32_t omni_light_count = 0;
	uint32_t area_light_count = 0;

	float luminance_multiplier = 1.0;

	RenderingServerTypes::RenderInfo *render_info = nullptr;

	/* Shadow data */
	const RendererSceneRender::RenderShadowData *render_shadows = nullptr;
	int render_shadow_count = 0;
};

class RasterizerCanvasGLES2;

class RasterizerSceneGLES2 : public RendererSceneRender {
private:
	static RasterizerSceneGLES2 *singleton;
	RSE::ViewportDebugDraw debug_draw = RSE::VIEWPORT_DEBUG_DRAW_DISABLED;
	uint64_t scene_pass = 0;

	template <typename T>
	struct InstanceSort {
		float depth;
		T *instance = nullptr;
		bool operator<(const InstanceSort &p_sort) const {
			return depth < p_sort.depth;
		}
	};

	struct SceneGlobals {
		RID shader_default_version;
		RID default_material;
		RID default_shader;
		RID overdraw_material;
		RID overdraw_shader;
	} scene_globals;

	GLES2::SceneMaterialData *default_material_data_ptr = nullptr;
	GLES2::SceneMaterialData *overdraw_material_data_ptr = nullptr;

	struct LTC {
		RID lut1_texture;
		RID lut2_texture;
	} ltc;

	/* LIGHT INSTANCE */

	struct LightData {
		float position[3];
		float inv_radius;

		float direction[3]; // Only used by SpotLight
		float size;

		float color[3];
		float attenuation;

		float inv_spot_attenuation;
		float cos_spot_angle;
		float specular_amount;
		float shadow_opacity;

		float pad[3];
		uint32_t bake_mode;

		float area_width[4]; // 4th is padding
		float area_height[4];
	};
	static_assert(sizeof(LightData) % 16 == 0, "LightData size must be a multiple of 16 bytes");

	struct DirectionalLightData {
		float direction[3];
		float energy;

		float color[3];
		float size;

		uint32_t enabled : 1; // For use by SkyShaders
		uint32_t bake_mode : 2;
		float shadow_opacity;
		float specular;
		uint32_t mask;
	};
	static_assert(sizeof(DirectionalLightData) % 16 == 0, "DirectionalLightData size must be a multiple of 16 bytes");

	struct ShadowData {
		float shadow_matrix[16];

		float light_position[3];
		float shadow_normal_bias;

		float pad[3];
		float shadow_atlas_pixel_size;
	};
	static_assert(sizeof(ShadowData) % 16 == 0, "ShadowData size must be a multiple of 16 bytes");

	struct DirectionalShadowData {
		float direction[3];
		float shadow_atlas_pixel_size;
		float shadow_normal_bias[4];
		float shadow_split_offsets[4];
		float shadow_matrices[4][16];
		float fade_from;
		float fade_to;
		uint32_t blend_splits; // Not exposed to the shader.
		uint32_t pad;
	};
	static_assert(sizeof(DirectionalShadowData) % 16 == 0, "DirectionalShadowData size must be a multiple of 16 bytes");

	class GeometryInstanceGLES2;

	// Cached data for drawing surfaces
	struct GeometryInstanceSurface {
		enum {
			FLAG_PASS_DEPTH = 1,
			FLAG_PASS_OPAQUE = 2,
			FLAG_PASS_ALPHA = 4,
			FLAG_PASS_SHADOW = 8,
			FLAG_USES_SHARED_SHADOW_MATERIAL = 128,
			FLAG_USES_SCREEN_TEXTURE = 2048,
			FLAG_USES_DEPTH_TEXTURE = 4096,
			FLAG_USES_NORMAL_TEXTURE = 8192,
			FLAG_USES_DOUBLE_SIDED_SHADOWS = 16384,
			FLAG_USES_STENCIL = 32768,
		};

		union {
			struct {
				uint64_t sort_key1;
				uint64_t sort_key2;
			};
			struct {
				uint64_t lod_index : 8;
				uint64_t surface_index : 8;
				uint64_t geometry_id : 32;
				uint64_t material_id_low : 16;

				uint64_t material_id_hi : 16;
				uint64_t shader_id : 32;
				uint64_t uses_softshadow : 1;
				uint64_t uses_projector : 1;
				uint64_t uses_forward_gi : 1;
				uint64_t uses_lightmap : 1;
				uint64_t depth_layer : 4;
				uint64_t priority : 8;
			};
		} sort;

		RSE::PrimitiveType primitive = RSE::PRIMITIVE_MAX;
		uint32_t flags = 0;
		uint32_t surface_index = 0;
		uint32_t lod_index = 0;
		uint32_t index_count = 0;
		int32_t light_pass_index = -1;
		bool finished_base_pass = false;

		void *surface = nullptr;
		GLES2::SceneShaderData *shader = nullptr;
		GLES2::SceneMaterialData *material = nullptr;

		void *surface_shadow = nullptr;
		GLES2::SceneShaderData *shader_shadow = nullptr;
		GLES2::SceneMaterialData *material_shadow = nullptr;

		GeometryInstanceSurface *next = nullptr;
		GeometryInstanceGLES2 *owner = nullptr;
	};

	struct GeometryInstanceLightmapSH {
		Color sh[9];
	};

	class GeometryInstanceGLES2 : public RenderGeometryInstanceBase {
	public:
		//used during rendering
		bool store_transform_cache = true;

		// Used for generating motion vectors.
		Transform3D prev_transform;
		bool is_prev_transform_stored = false;

		int32_t instance_count = 0;

		bool can_sdfgi = false;
		bool using_projectors = false;
		bool using_softshadows = false;

		struct LightPass {
			int32_t light_id = -1; // Position in the light uniform buffer.
			int32_t shadow_id = -1; // Position in the shadow uniform buffer.
			RID light_instance_rid;
			bool is_omni = false;
		};

		LocalVector<LightPass> light_passes;

		uint32_t paired_omni_light_count = 0;
		uint32_t paired_spot_light_count = 0;
		uint32_t paired_area_light_count = 0;
		LocalVector<RID> paired_omni_lights;
		LocalVector<RID> paired_spot_lights;
		LocalVector<RID> paired_area_lights;
		LocalVector<uint32_t> omni_light_gl_cache;
		LocalVector<uint32_t> spot_light_gl_cache;
		LocalVector<uint32_t> area_light_gl_cache;

		LocalVector<RID> paired_reflection_probes;
		LocalVector<RID> reflection_probe_rid_cache;
		LocalVector<Transform3D> reflection_probes_local_transform_cache;

		RID lightmap_instance;
		Rect2 lightmap_uv_scale;
		uint32_t lightmap_slice_index;
		GeometryInstanceLightmapSH *lightmap_sh = nullptr;

		// Used during setup.
		GeometryInstanceSurface *surface_caches = nullptr;
		SelfList<GeometryInstanceGLES2> dirty_list_element;

		GeometryInstanceGLES2() :
				dirty_list_element(this) {}

		virtual void _mark_dirty() override;
		virtual void set_use_lightmap(RID p_lightmap_instance, const Rect2 &p_lightmap_uv_scale, int p_lightmap_slice_index) override;
		virtual void set_lightmap_capture(const Color *p_sh9) override;

		virtual void clear_light_instances() override;
		virtual void pair_light_instance(const RID p_light_instance, RSE::LightType light_type, uint32_t placement_idx) override;
		virtual void pair_reflection_probe_instances(const RID *p_reflection_probe_instances, uint32_t p_reflection_probe_instance_count) override;
		virtual void pair_decal_instances(const RID *p_decal_instances, uint32_t p_decal_instance_count) override {}
		virtual void pair_voxel_gi_instances(const RID *p_voxel_gi_instances, uint32_t p_voxel_gi_instance_count) override {}

		virtual void set_softshadow_projector_pairing(bool p_softshadow, bool p_projector) override {}
	};

	virtual uint32_t get_max_lights_total() override;
	virtual uint32_t get_max_lights_per_mesh() override;

	enum {
		INSTANCE_DATA_FLAGS_DYNAMIC = 1 << 3,
		INSTANCE_DATA_FLAGS_NON_UNIFORM_SCALE = 1 << 4,
		INSTANCE_DATA_FLAG_USE_GI_BUFFERS = 1 << 5,
		INSTANCE_DATA_FLAG_USE_LIGHTMAP_CAPTURE = 1 << 7,
		INSTANCE_DATA_FLAG_USE_LIGHTMAP = 1 << 8,
		INSTANCE_DATA_FLAG_USE_SH_LIGHTMAP = 1 << 9,
		INSTANCE_DATA_FLAG_USE_VOXEL_GI = 1 << 10,
		INSTANCE_DATA_FLAG_PARTICLES = 1 << 11,
		INSTANCE_DATA_FLAG_MULTIMESH = 1 << 12,
		INSTANCE_DATA_FLAG_MULTIMESH_FORMAT_2D = 1 << 13,
		INSTANCE_DATA_FLAG_MULTIMESH_HAS_COLOR = 1 << 14,
		INSTANCE_DATA_FLAG_MULTIMESH_HAS_CUSTOM_DATA = 1 << 15,
	};

	static void _geometry_instance_dependency_changed(Dependency::DependencyChangedNotification p_notification, DependencyTracker *p_tracker);
	static void _geometry_instance_dependency_deleted(const RID &p_dependency, DependencyTracker *p_tracker);

	SelfList<GeometryInstanceGLES2>::List geometry_instance_dirty_list;

	// Use PagedAllocator instead of RID to maximize performance
	PagedAllocator<GeometryInstanceGLES2> geometry_instance_alloc;
	PagedAllocator<GeometryInstanceSurface> geometry_instance_surface_alloc;

	void _geometry_instance_add_surface_with_material(GeometryInstanceGLES2 *ginstance, uint32_t p_surface, GLES2::SceneMaterialData *p_material, uint32_t p_material_id, uint32_t p_shader_id, RID p_mesh);
	void _geometry_instance_add_surface_with_material_chain(GeometryInstanceGLES2 *ginstance, uint32_t p_surface, GLES2::SceneMaterialData *p_material, RID p_mat_src, RID p_mesh);
	void _geometry_instance_add_surface(GeometryInstanceGLES2 *ginstance, uint32_t p_surface, RID p_material, RID p_mesh);
	void _geometry_instance_update(RenderGeometryInstance *p_geometry_instance);
	void _update_dirty_geometry_instances();

	struct SceneState {
		struct UBO {
			float projection_matrix[16];
			float inv_projection_matrix[16];
			float inv_view_matrix[16];
			float view_matrix[16];

			float main_cam_inv_view_matrix[16];

			float viewport_size[2];
			float screen_pixel_size[2];

			float ambient_light_color_energy[4];

			float ambient_color_sky_mix;
			uint32_t directional_shadow_count;
			float emissive_exposure_normalization;
			uint32_t use_ambient_light = 0;

			uint32_t use_ambient_cubemap = 0;
			uint32_t use_reflection_cubemap = 0;
			float fog_aerial_perspective;
			float time;

			float radiance_inverse_xform[12];

			uint32_t directional_light_count;
			float z_far;
			float z_near;
			float IBL_exposure_normalization;

			uint32_t fog_enabled;
			uint32_t fog_mode;
			float fog_density;
			float fog_height;

			float fog_height_density;
			float fog_depth_curve;
			float fog_sun_scatter;
			float fog_depth_begin;

			float fog_light_color[3];
			float fog_depth_end;

			float shadow_bias;
			float luminance_multiplier;
			uint32_t camera_visible_layers;
			bool pancake_shadows;
		};
		static_assert(sizeof(UBO) % 16 == 0, "Scene UBO size must be a multiple of 16 bytes");
		static_assert(sizeof(UBO) < 16384, "Scene UBO size must be 16384 bytes or smaller");

		struct MultiviewUBO {
			float projection_matrix_view[RendererSceneRender::MAX_RENDER_VIEWS][16];
			float inv_projection_matrix_view[RendererSceneRender::MAX_RENDER_VIEWS][16];
			float eye_offset[RendererSceneRender::MAX_RENDER_VIEWS][4];
		};
		static_assert(sizeof(MultiviewUBO) % 16 == 0, "Multiview UBO size must be a multiple of 16 bytes");
		static_assert(sizeof(MultiviewUBO) < 16384, "MultiviewUBO size must be 16384 bytes or smaller");

		struct TonemapUBO {
			float exposure = 1.0;
			int32_t tonemapper = 0;
			int32_t pad = 0;
			int32_t pad2 = 0;
			float tonemapper_params[4] = { 0.0, 0.0, 0.0, 0.0 };
			float brightness = 1.0;
			float contrast = 1.0;
			float saturation = 1.0;
			int32_t pad3 = 0;
		};
		static_assert(sizeof(TonemapUBO) % 16 == 0, "Tonemap UBO size must be a multiple of 16 bytes");

		UBO data;
		UBO prev_data;
		MultiviewUBO multiview_data;
		MultiviewUBO prev_multiview_data;
		// GLES2 simplification: persisted for plain-uniform upload (no UBO).
		// Initialized from the environment in post-processing, like the old buffer.
		TonemapUBO tonemap_data;

		// GLES2 simplification: per-program location caches for plain uniforms
		// (no UBOs). Each entry also tracks the last uploaded frame.
		struct ProgramUniforms {
			// SceneDataBlock members (dotted: "scene_data_block.data.*").
			GLint projection_matrix = -1;
			GLint inv_projection_matrix = -1;
			GLint inv_view_matrix = -1;
			GLint view_matrix = -1;
			GLint main_cam_inv_view_matrix = -1;
			GLint viewport_size = -1;
			GLint screen_pixel_size = -1;
			GLint ambient_light_color_energy = -1;
			GLint ambient_color_sky_mix = -1;
			GLint directional_shadow_count = -1;
			GLint emissive_exposure_normalization = -1;
			GLint use_ambient_light = -1;
			GLint use_ambient_cubemap = -1;
			GLint use_reflection_cubemap = -1;
			GLint fog_aerial_perspective = -1;
			GLint time = -1;
			GLint radiance_inverse_xform = -1;
			GLint directional_light_count = -1;
			GLint z_far = -1;
			GLint z_near = -1;
			GLint ibl_exposure_normalization = -1;
			GLint fog_enabled = -1;
			GLint fog_mode = -1;
			GLint fog_density = -1;
			GLint fog_height = -1;
			GLint fog_height_density = -1;
			GLint fog_depth_curve = -1;
			GLint fog_sun_scatter = -1;
			GLint fog_depth_begin = -1;
			GLint fog_light_color = -1;
			GLint fog_depth_end = -1;
			GLint shadow_bias = -1;
			GLint luminance_multiplier = -1;
			GLint camera_visible_layers = -1;
			GLint pancake_shadows = -1;
			// PrevSceneDataBlock (same members, prev_ prefix in shader).
			GLint prev_projection_matrix = -1;
			GLint prev_inv_projection_matrix = -1;
			GLint prev_inv_view_matrix = -1;
			GLint prev_view_matrix = -1;
			GLint prev_main_cam_inv_view_matrix = -1;
			GLint prev_viewport_size = -1;
			GLint prev_screen_pixel_size = -1;
			GLint prev_ambient_light_color_energy = -1;
			GLint prev_ambient_color_sky_mix = -1;
			GLint prev_directional_shadow_count = -1;
			GLint prev_emissive_exposure_normalization = -1;
			GLint prev_use_ambient_light = -1;
			GLint prev_use_ambient_cubemap = -1;
			GLint prev_use_reflection_cubemap = -1;
			GLint prev_fog_aerial_perspective = -1;
			GLint prev_time = -1;
			GLint prev_radiance_inverse_xform = -1;
			GLint prev_directional_light_count = -1;
			GLint prev_z_far = -1;
			GLint prev_z_near = -1;
			GLint prev_ibl_exposure_normalization = -1;
			GLint prev_fog_enabled = -1;
			GLint prev_fog_mode = -1;
			GLint prev_fog_density = -1;
			GLint prev_fog_height = -1;
			GLint prev_fog_height_density = -1;
			GLint prev_fog_depth_curve = -1;
			GLint prev_fog_sun_scatter = -1;
			GLint prev_fog_depth_begin = -1;
			GLint prev_fog_light_color = -1;
			GLint prev_fog_depth_end = -1;
			GLint prev_shadow_bias = -1;
			GLint prev_luminance_multiplier = -1;
			GLint prev_camera_visible_layers = -1;
			GLint prev_pancake_shadows = -1;
			// MultiviewDataBlock (+prev) and TonemapData members.
			GLint mv_projection_matrix_view = -1;
			GLint mv_inv_projection_matrix_view = -1;
			GLint mv_eye_offset = -1;
			GLint prev_mv_projection_matrix_view = -1;
			GLint prev_mv_inv_projection_matrix_view = -1;
			GLint prev_mv_eye_offset = -1;
			GLint tonemap_exposure = -1;
			GLint tonemap_tonemapper = -1;
			GLint tonemap_tonemapper_params = -1;
			GLint tonemap_brightness = -1;
			GLint tonemap_contrast = -1;
			GLint tonemap_saturation = -1;
			// Global table (conditional).
			GLint global_table = -1;
			// Sky directional array bases (queried per member below).
			GLint sky_dir_energy = -1;
			GLint sky_dir_color = -1;
			GLint sky_dir_enabled = -1;
			GLint sky_dir_shadow_opacity = -1;
			GLint sky_dir_specular = -1;
			GLint sky_dir_mask = -1;
			// Sky multiview block uses direct members (no .data nesting).
			GLint sky_mv_projection_matrix_view = -1;
			GLint sky_mv_inv_projection_matrix_view = -1;
			GLint sky_mv_eye_offset = -1;
			// 3D light/shadow plain arrays, indexed [light][member]. Member
			// order mirrors the GLSL structs (see _ensure_scene_light_uniforms).
			GLint omni_lights[MAX_OMNI_LIGHTS_GLES2][LIGHT_DATA_MEMBER_COUNT_GLES2];
			GLint spot_lights[MAX_SPOT_LIGHTS_GLES2][LIGHT_DATA_MEMBER_COUNT_GLES2];
			GLint area_lights[MAX_AREA_LIGHTS_GLES2][LIGHT_DATA_MEMBER_COUNT_GLES2];
			GLint directional_lights[MAX_DIRECTIONAL_LIGHTS][DIRECTIONAL_LIGHT_MEMBER_COUNT_GLES2];
			GLint positional_shadows[MAX_POSITIONAL_SHADOWS_GLES2][POSITIONAL_SHADOW_MEMBER_COUNT_GLES2];
			GLint directional_shadows[MAX_DIRECTIONAL_LIGHTS][DIRECTIONAL_SHADOW_MEMBER_COUNT_GLES2];
			uint64_t frame = 0;
			uint64_t light_frame = 0;
		};
		HashMap<GLuint, ProgramUniforms> program_uniforms;

		int prev_data_state = 0; // 0 = Motion vectors not used, 1 = use data (first frame only), 2 = use previous data

		bool used_depth_prepass = false;

		GLES2::SceneShaderData::BlendMode current_blend_mode = GLES2::SceneShaderData::BLEND_MODE_MIX;
		RSE::CullMode cull_mode = RSE::CULL_MODE_BACK;
		GLenum current_depth_function = GL_GEQUAL;

		bool current_blend_enabled = false;
		bool current_depth_draw_enabled = false;
		bool current_depth_test_enabled = false;
		bool current_scissor_test_enabled = false;

		void reset_gl_state() {
			glDisable(GL_BLEND);
			current_blend_enabled = false;

			glDisable(GL_SCISSOR_TEST);
			current_scissor_test_enabled = false;

			glCullFace(GL_BACK);
			glEnable(GL_CULL_FACE);
			cull_mode = RSE::CULL_MODE_BACK;

			glDepthMask(GL_FALSE);
			current_depth_draw_enabled = false;
			glDisable(GL_DEPTH_TEST);
			current_depth_test_enabled = false;

			glDepthFunc(GL_GEQUAL);
			current_depth_function = GL_GEQUAL;

			glDisable(GL_STENCIL_TEST);
			current_stencil_test_enabled = false;
			glStencilMask(255);
			current_stencil_write_mask = 255;
			glStencilFunc(GL_ALWAYS, 0, 255);
			current_stencil_compare = GL_ALWAYS;
			current_stencil_reference = 0;
			current_stencil_compare_mask = 255;
		}

		void set_gl_cull_mode(RSE::CullMode p_mode) {
			if (cull_mode != p_mode) {
				if (p_mode == RSE::CULL_MODE_DISABLED) {
					glDisable(GL_CULL_FACE);
				} else {
					if (cull_mode == RSE::CULL_MODE_DISABLED) {
						// Last time was disabled, so enable and set proper face.
						glEnable(GL_CULL_FACE);
					}
					glCullFace(p_mode == RSE::CULL_MODE_FRONT ? GL_FRONT : GL_BACK);
				}
				cull_mode = p_mode;
			}
		}

		void enable_gl_blend(bool p_enabled) {
			if (current_blend_enabled != p_enabled) {
				if (p_enabled) {
					glEnable(GL_BLEND);
				} else {
					glDisable(GL_BLEND);
				}
				current_blend_enabled = p_enabled;
			}
		}

		void enable_gl_scissor_test(bool p_enabled) {
			if (current_scissor_test_enabled != p_enabled) {
				if (p_enabled) {
					glEnable(GL_SCISSOR_TEST);
				} else {
					glDisable(GL_SCISSOR_TEST);
				}
				current_scissor_test_enabled = p_enabled;
			}
		}

		void enable_gl_depth_draw(bool p_enabled) {
			if (current_depth_draw_enabled != p_enabled) {
				glDepthMask(p_enabled ? GL_TRUE : GL_FALSE);
				current_depth_draw_enabled = p_enabled;
			}
		}

		void enable_gl_depth_test(bool p_enabled) {
			if (current_depth_test_enabled != p_enabled) {
				if (p_enabled) {
					glEnable(GL_DEPTH_TEST);
				} else {
					glDisable(GL_DEPTH_TEST);
				}
				current_depth_test_enabled = p_enabled;
			}
		}

		void set_gl_depth_func(GLenum p_depth_func) {
			if (current_depth_function != p_depth_func) {
				glDepthFunc(p_depth_func);
				current_depth_function = p_depth_func;
			}
		}

		void enable_gl_stencil_test(bool p_enabled) {
			if (current_stencil_test_enabled != p_enabled) {
				if (p_enabled) {
					glEnable(GL_STENCIL_TEST);
				} else {
					glDisable(GL_STENCIL_TEST);
				}
				current_stencil_test_enabled = p_enabled;
			}
		}

		void set_gl_stencil_func(GLenum p_compare, GLint p_reference, GLenum p_compare_mask) {
			if (current_stencil_compare != p_compare || current_stencil_reference != p_reference || current_stencil_compare_mask != p_compare_mask) {
				glStencilFunc(p_compare, p_reference, p_compare_mask);
				current_stencil_compare = p_compare;
				current_stencil_reference = p_reference;
				current_stencil_compare_mask = p_compare_mask;
			}
		}

		void set_gl_stencil_write_mask(GLuint p_mask) {
			if (current_stencil_write_mask != p_mask) {
				glStencilMask(p_mask);
				current_stencil_write_mask = p_mask;
			}
		}

		void set_gl_stencil_op(GLenum p_op_fail, GLenum p_op_dpfail, GLenum p_op_dppass) {
			if (current_stencil_op_fail != p_op_fail || current_stencil_op_dpfail != p_op_dpfail || current_stencil_op_dppass != p_op_dppass) {
				glStencilOp(p_op_fail, p_op_dpfail, p_op_dppass);
				current_stencil_op_fail = p_op_fail;
				current_stencil_op_dpfail = p_op_dpfail;
				current_stencil_op_dppass = p_op_dppass;
			}
		}

		GLenum current_stencil_compare = GL_ALWAYS;
		GLuint current_stencil_compare_mask = 255;
		GLuint current_stencil_write_mask = 255;
		GLint current_stencil_reference = 0;
		GLenum current_stencil_op_fail = GL_KEEP;
		GLenum current_stencil_op_dpfail = GL_KEEP;
		GLenum current_stencil_op_dppass = GL_KEEP;
		bool current_stencil_test_enabled = false;

		bool texscreen_copied = false;
		bool used_screen_texture = false;
		bool used_normal_texture = false;
		bool used_depth_texture = false;
		bool used_opaque_stencil = false;

		LightData *omni_lights = nullptr;
		LightData *spot_lights = nullptr;
		LightData *area_lights = nullptr;
		ShadowData *positional_shadows = nullptr;

		InstanceSort<GLES2::LightInstance> *omni_light_sort;
		InstanceSort<GLES2::LightInstance> *spot_light_sort;
		InstanceSort<GLES2::LightInstance> *area_light_sort;
		// GLES2 simplification: no light/shadow GL buffers (plain uniforms).
		uint32_t omni_light_count = 0;
		uint32_t spot_light_count = 0;
		uint32_t area_light_count = 0;
		uint32_t directional_light_count = 0;
		uint32_t directional_shadow_count = 0;
		uint32_t positional_shadow_count = 0;
		RSE::ShadowQuality positional_shadow_quality = RSE::ShadowQuality::SHADOW_QUALITY_SOFT_LOW;

		DirectionalLightData *directional_lights = nullptr;
		DirectionalShadowData *directional_shadows = nullptr;
		RSE::ShadowQuality directional_shadow_quality = RSE::ShadowQuality::SHADOW_QUALITY_SOFT_LOW;
	} scene_state;

	struct RenderListParameters {
		GeometryInstanceSurface **elements = nullptr;
		int element_count = 0;
		bool reverse_cull = false;
		uint64_t spec_constant_base_flags = 0;
		bool force_wireframe = false;
		Vector2 uv_offset = Vector2(0, 0);

		RenderListParameters(GeometryInstanceSurface **p_elements, int p_element_count, bool p_reverse_cull, uint64_t p_spec_constant_base_flags, bool p_force_wireframe = false, Vector2 p_uv_offset = Vector2()) {
			elements = p_elements;
			element_count = p_element_count;
			reverse_cull = p_reverse_cull;
			spec_constant_base_flags = p_spec_constant_base_flags;
			force_wireframe = p_force_wireframe;
			uv_offset = p_uv_offset;
		}
	};

	struct RenderList {
		LocalVector<GeometryInstanceSurface *> elements;

		void clear() {
			elements.clear();
		}

		//should eventually be replaced by radix

		struct SortByKey {
			_FORCE_INLINE_ bool operator()(const GeometryInstanceSurface *A, const GeometryInstanceSurface *B) const {
				return (A->sort.sort_key2 == B->sort.sort_key2) ? (A->sort.sort_key1 < B->sort.sort_key1) : (A->sort.sort_key2 < B->sort.sort_key2);
			}
		};

		void sort_by_key() {
			SortArray<GeometryInstanceSurface *, SortByKey> sorter;
			sorter.sort(elements.ptr(), elements.size());
		}

		void sort_by_key_range(uint32_t p_from, uint32_t p_size) {
			SortArray<GeometryInstanceSurface *, SortByKey> sorter;
			sorter.sort(elements.ptr() + p_from, p_size);
		}

		struct SortByDepth {
			_FORCE_INLINE_ bool operator()(const GeometryInstanceSurface *A, const GeometryInstanceSurface *B) const {
				return (A->owner->depth < B->owner->depth);
			}
		};

		void sort_by_depth() { //used for shadows

			SortArray<GeometryInstanceSurface *, SortByDepth> sorter;
			sorter.sort(elements.ptr(), elements.size());
		}

		struct SortByReverseDepthAndPriority {
			_FORCE_INLINE_ bool operator()(const GeometryInstanceSurface *A, const GeometryInstanceSurface *B) const {
				return (A->sort.priority == B->sort.priority) ? (A->owner->depth > B->owner->depth) : (A->sort.priority < B->sort.priority);
			}
		};

		void sort_by_reverse_depth_and_priority() { //used for alpha

			SortArray<GeometryInstanceSurface *, SortByReverseDepthAndPriority> sorter;
			sorter.sort(elements.ptr(), elements.size());
		}

		_FORCE_INLINE_ void add_element(GeometryInstanceSurface *p_element) {
			elements.push_back(p_element);
		}
	};

	RenderList render_list[RENDER_LIST_MAX_GLES2];

	// GLES2 simplification: 3D state uploads as plain uniforms (no UBOs), once
	// per program per frame. Locations are cached in program_uniforms.
	void _ensure_scene_program_uniforms(GLuint p_program, SceneState::ProgramUniforms &r_cache);
	void _set_scene_state_uniforms();
	void _ensure_scene_light_uniforms(GLuint p_program, SceneState::ProgramUniforms &r_cache);
	static void _upload_scene_light_data(const GLint *p_locations, const LightData &p_light);
	void _set_scene_light_uniforms();
	void _set_sky_uniforms();

	void _setup_lights(const RenderDataGLES2 *p_render_data, bool p_using_shadows, uint32_t &r_directional_light_count, uint32_t &r_omni_light_count, uint32_t &r_spot_light_count, uint32_t &r_area_light_count, uint32_t &r_directional_shadow_count);
	void _setup_environment(const RenderDataGLES2 *p_render_data, bool p_no_fog, const Size2i &p_screen_size, bool p_flip_y, const Color &p_default_bg_color, bool p_pancake_shadows, float p_shadow_bias = 0.0);
	void _fill_render_list(RenderListTypeGLES2 p_render_list, const RenderDataGLES2 *p_render_data, PassModeGLES2 p_pass_mode, bool p_append = false);
	void _render_shadows(const RenderDataGLES2 *p_render_data, const Size2i &p_viewport_size = Size2i(1, 1));
	void _render_shadow_pass(RID p_light, RID p_shadow_atlas, int p_pass, const PagedArray<RenderGeometryInstance *> &p_instances, float p_lod_distance_multiplier = 0, float p_screen_mesh_lod_threshold = 0.0, RenderingServerTypes::RenderInfo *p_render_info = nullptr, const Size2i &p_viewport_size = Size2i(1, 1), const Transform3D &p_main_cam_transform = Transform3D());
	void _render_post_processing(const RenderDataGLES2 *p_render_data);

	template <PassModeGLES2 p_pass_mode>
	_FORCE_INLINE_ void _render_list_template(RenderListParameters *p_params, const RenderDataGLES2 *p_render_data, uint32_t p_from_element, uint32_t p_to_element, bool p_alpha_pass = false);

	_FORCE_INLINE_ uint32_t _indices_to_primitives(RSE::PrimitiveType p_primitive, uint32_t p_indices) const;

protected:
	double time;
	double time_step = 0;

	bool screen_space_roughness_limiter = false;
	float screen_space_roughness_limiter_amount = 0.25;
	float screen_space_roughness_limiter_limit = 0.18;

	void _render_buffers_debug_draw(Ref<RenderSceneBuffersGLES2> p_render_buffers, RID p_shadow_atlas, GLuint p_fbo);

	/* Camera Attributes */

	struct CameraAttributes {
		float exposure_multiplier = 1.0;
		float exposure_normalization = 1.0;
	};

	bool use_physical_light_units = false;
	mutable RID_Owner<CameraAttributes, true> camera_attributes_owner;

	/* Environment */

	RSE::EnvironmentSSAOQuality ssao_quality = RSE::ENV_SSAO_QUALITY_MEDIUM;
	bool ssao_half_size = false;
	float ssao_adaptive_target = 0.5;
	int ssao_blur_passes = 2;
	float ssao_fadeout_from = 50.0;
	float ssao_fadeout_to = 300.0;

	bool glow_bicubic_upscale = false;

	bool lightmap_bicubic_upscale = false;

	/* Sky */

	struct SkyGlobals {
		float fog_aerial_perspective = 0.0;
		Color fog_light_color;
		float fog_sun_scatter = 0.0;
		bool fog_enabled = false;
		float fog_density = 0.0;
		float z_far = 0.0;
		uint32_t directional_light_count = 0;

		DirectionalLightData *directional_lights = nullptr;
		DirectionalLightData *last_frame_directional_lights = nullptr;
		uint32_t last_frame_directional_light_count = 0;
		// GLES2 simplification: no directional light UBO (plain uniforms).

		RID shader_default_version;
		RID default_material;
		RID default_shader;
		RID fog_material;
		RID fog_shader;
		GLuint screen_triangle = 0;
		GLuint screen_triangle_array = 0;
		uint32_t max_directional_lights = 4;
		uint32_t roughness_layers = 8;
	} sky_globals;

	struct Sky {
		// Screen Buffers
		GLuint half_res_pass = 0;
		GLuint half_res_framebuffer = 0;
		GLuint quarter_res_pass = 0;
		GLuint quarter_res_framebuffer = 0;
		Size2i screen_size = Size2i(0, 0);

		// Radiance Cubemap
		GLuint radiance = 0;
		GLuint radiance_framebuffer = 0;
		GLuint raw_radiance = 0;

		RID material;

		int radiance_size = 256;
		int mipmap_count = 1;

		RSE::SkyMode mode = RSE::SKY_MODE_AUTOMATIC;
		RSE::SkyMode internal_mode = RSE::SKY_MODE_INCREMENTAL; // When using SKY_MODE_AUTOMATIC, this is the mode used internally.

		//ReflectionData reflection;
		bool reflection_dirty = false;
		bool dirty = false;
		int processing_layer = 0;
		Sky *dirty_list = nullptr;
		float baked_exposure = 1.0;

		//State to track when radiance cubemap needs updating
		GLES2::SkyMaterialData *prev_material = nullptr;
		Vector3 prev_position = Vector3(0.0, 0.0, 0.0);
		float prev_time = 0.0f;
	};

	Sky *dirty_sky_list = nullptr;
	mutable RID_Owner<Sky, true> sky_owner;

	void _setup_sky(const RenderDataGLES2 *p_render_data, const PagedArray<RID> &p_lights, const Projection &p_projection, const Transform3D &p_transform, const Size2i p_screen_size);
	void _invalidate_sky(Sky *p_sky);
	void _update_dirty_skys();
	void _update_sky_radiance(RID p_env, const Projection &p_projection, const Transform3D &p_transform, float p_sky_energy_multiplier);
	void _draw_sky(RID p_env, const Projection &p_projection, const Transform3D &p_transform, float p_sky_energy_multiplier, float p_luminance_multiplier, bool p_use_multiview, bool p_flip_y, bool p_apply_color_adjustments_in_post);
	void _free_sky_data(Sky *p_sky);

	// Needed for a single argument calls (material and uv2).
	PagedArrayPool<RenderGeometryInstance *> cull_argument_pool;
	PagedArray<RenderGeometryInstance *> cull_argument;

public:
	static RasterizerSceneGLES2 *get_singleton() { return singleton; }

	RasterizerCanvasGLES2 *canvas = nullptr;

	RenderGeometryInstance *geometry_instance_create(RID p_base) override;
	void geometry_instance_free(RenderGeometryInstance *p_geometry_instance) override;

	uint32_t geometry_instance_get_pair_mask() override;

	/* PIPELINES */

	virtual void mesh_generate_pipelines(RID p_mesh, bool p_background_compilation) override {}
	virtual uint32_t get_pipeline_compilations(RSE::PipelineSource p_source) override { return 0; }

	/* SDFGI UPDATE */

	void sdfgi_update(const Ref<RenderSceneBuffers> &p_render_buffers, RID p_environment, const Vector3 &p_world_position) override {}
	int sdfgi_get_pending_region_count(const Ref<RenderSceneBuffers> &p_render_buffers) const override {
		return 0;
	}
	AABB sdfgi_get_pending_region_bounds(const Ref<RenderSceneBuffers> &p_render_buffers, int p_region) const override {
		return AABB();
	}
	uint32_t sdfgi_get_pending_region_cascade(const Ref<RenderSceneBuffers> &p_render_buffers, int p_region) const override {
		return 0;
	}

	/* SKY API */

	RID sky_allocate() override;
	void sky_initialize(RID p_rid) override;
	void sky_set_radiance_size(RID p_sky, int p_radiance_size) override;
	void sky_set_mode(RID p_sky, RSE::SkyMode p_mode) override;
	void sky_set_material(RID p_sky, RID p_material) override;
	Ref<Image> sky_bake_panorama(RID p_sky, float p_energy, bool p_bake_irradiance, const Size2i &p_size) override;
	float sky_get_baked_exposure(RID p_sky) const;

	/* ENVIRONMENT API */

	void environment_glow_set_use_bicubic_upscale(bool p_enable) override;

	void environment_set_ssr_half_size(bool p_half_size) override;
	void environment_set_ssr_roughness_quality(RSE::EnvironmentSSRRoughnessQuality p_quality) override;

	void environment_set_ssao_quality(RSE::EnvironmentSSAOQuality p_quality, bool p_half_size, float p_adaptive_target, int p_blur_passes, float p_fadeout_from, float p_fadeout_to) override;

	void environment_set_ssil_quality(RSE::EnvironmentSSILQuality p_quality, bool p_half_size, float p_adaptive_target, int p_blur_passes, float p_fadeout_from, float p_fadeout_to) override;

	void environment_set_sdfgi_ray_count(RSE::EnvironmentSDFGIRayCount p_ray_count) override;
	void environment_set_sdfgi_frames_to_converge(RSE::EnvironmentSDFGIFramesToConverge p_frames) override;
	void environment_set_sdfgi_frames_to_update_light(RSE::EnvironmentSDFGIFramesToUpdateLight p_update) override;

	void environment_set_volumetric_fog_volume_size(int p_size, int p_depth) override;
	void environment_set_volumetric_fog_filter_active(bool p_enable) override;

	Ref<Image> environment_bake_panorama(RID p_env, bool p_bake_irradiance, const Size2i &p_size) override;

	_FORCE_INLINE_ bool is_using_physical_light_units() {
		return use_physical_light_units;
	}

	void positional_soft_shadow_filter_set_quality(RSE::ShadowQuality p_quality) override;
	void directional_soft_shadow_filter_set_quality(RSE::ShadowQuality p_quality) override;

	RID fog_volume_instance_create(RID p_fog_volume) override;
	void fog_volume_instance_set_transform(RID p_fog_volume_instance, const Transform3D &p_transform) override;
	void fog_volume_instance_set_active(RID p_fog_volume_instance, bool p_active) override;
	RID fog_volume_instance_get_volume(RID p_fog_volume_instance) const override;
	Vector3 fog_volume_instance_get_position(RID p_fog_volume_instance) const override;

	RID voxel_gi_instance_create(RID p_voxel_gi) override;
	void voxel_gi_instance_set_transform_to_data(RID p_probe, const Transform3D &p_xform) override;
	bool voxel_gi_needs_update(RID p_probe) const override;
	void voxel_gi_update(RID p_probe, bool p_update_light_instances, const Vector<RID> &p_light_instances, const PagedArray<RenderGeometryInstance *> &p_dynamic_objects) override;

	void voxel_gi_set_quality(RSE::VoxelGIQuality) override;

	void render_scene(const Ref<RenderSceneBuffers> &p_render_buffers, const CameraData *p_camera_data, const CameraData *p_prev_camera_data, const PagedArray<RenderGeometryInstance *> &p_instances, const PagedArray<RID> &p_lights, const PagedArray<RID> &p_reflection_probes, const PagedArray<RID> &p_voxel_gi_instances, const PagedArray<RID> &p_decals, const PagedArray<RID> &p_lightmaps, const PagedArray<RID> &p_fog_volumes, RID p_environment, RID p_camera_attributes, RID p_compositor, RID p_shadow_atlas, RID p_occluder_debug_tex, RID p_reflection_atlas, RID p_reflection_probe, int p_reflection_probe_pass, float p_screen_mesh_lod_threshold, const RenderShadowData *p_render_shadows, int p_render_shadow_count, const RenderSDFGIData *p_render_sdfgi_regions, int p_render_sdfgi_region_count, float p_window_output_max_value, const RenderSDFGIUpdateData *p_sdfgi_update_data = nullptr, RenderingServerTypes::RenderInfo *r_render_info = nullptr) override;
	void render_material(const Transform3D &p_cam_transform, const Projection &p_cam_projection, bool p_cam_orthogonal, const PagedArray<RenderGeometryInstance *> &p_instances, RID p_framebuffer, const Rect2i &p_region) override;
	void render_particle_collider_heightfield(RID p_collider, const Transform3D &p_transform, const PagedArray<RenderGeometryInstance *> &p_instances) override;

	void set_scene_pass(uint64_t p_pass) override {
		scene_pass = p_pass;
	}

	_FORCE_INLINE_ uint64_t get_scene_pass() {
		return scene_pass;
	}

	void set_time(double p_time, double p_step) override;
	void set_debug_draw_mode(RSE::ViewportDebugDraw p_debug_draw) override;
	_FORCE_INLINE_ RSE::ViewportDebugDraw get_debug_draw_mode() const {
		return debug_draw;
	}

	Ref<RenderSceneBuffers> render_buffers_create() override;
	void gi_set_use_half_resolution(bool p_enable) override;

	void screen_space_roughness_limiter_set_active(bool p_enable, float p_amount, float p_curve) override;
	bool screen_space_roughness_limiter_is_active() const override;

	void sub_surface_scattering_set_quality(RSE::SubSurfaceScatteringQuality p_quality) override;
	void sub_surface_scattering_set_scale(float p_scale, float p_depth_scale) override;

	TypedArray<Image> bake_render_uv2(RID p_base, const TypedArray<RID> &p_material_overrides, const Size2i &p_image_size) override;
	virtual PackedByteArray bake_render_area_light_atlas(const TypedArray<RID> &p_area_light_textures, const TypedArray<Rect2> &p_area_light_atlas_texture_rects, const Size2i &p_size, int p_mipmaps) override { return PackedByteArray(); }
	void _render_uv2(const PagedArray<RenderGeometryInstance *> &p_instances, GLuint p_framebuffer, const Rect2i &p_region);

	bool free(RID p_rid) override;
	void update() override;
	void sdfgi_set_debug_probe_select(const Vector3 &p_position, const Vector3 &p_dir) override;

	void decals_set_filter(RSE::DecalFilter p_filter) override;
	void light_projectors_set_filter(RSE::LightProjectorFilter p_filter) override;
	virtual void lightmaps_set_bicubic_filter(bool p_enable) override;
	virtual void material_set_use_debanding(bool p_enable) override;

	RasterizerSceneGLES2();
	~RasterizerSceneGLES2();
};

#endif // GLES2_ENABLED
