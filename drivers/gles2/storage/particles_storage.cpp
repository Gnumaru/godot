/**************************************************************************/
/*  particles_storage.cpp                                                 */
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

#include "particles_storage.h"

#ifdef GLES2_ENABLED

#include "drivers/gles2/storage/config.h"
#include "drivers/gles2/storage/material_storage.h"
#include "drivers/gles2/storage/mesh_storage.h"
#include "drivers/gles2/storage/texture_storage.h"
#include "drivers/gles2/storage/utilities.h"
#include "servers/rendering/rendering_server_globals.h"

using namespace GLES2;

ParticlesStorage *ParticlesStorage::singleton = nullptr;

ParticlesStorage *ParticlesStorage::get_singleton() {
	return singleton;
}

ParticlesStorage::ParticlesStorage() {
	singleton = this;
	// GLES2 simplification (Fase C, 3D minimo low-end): particulas GPU desativadas.
	// Mantido como stub no-op que ainda compila como GLES3 para facilitar porte futuro.
	// Sem inicializacao de process shader, material/shader default ou copy shader
	// (evita transform feedback, UBOs e VBOs/VAOs de particulas).
}

ParticlesStorage::~ParticlesStorage() {
	singleton = nullptr;
	// Nada alocado no construtor simplificado, nada a liberar.
}

/* PARTICLES */

RID ParticlesStorage::particles_allocate() {
	return particles_owner.allocate_rid();
}

void ParticlesStorage::particles_initialize(RID p_rid) {
	particles_owner.initialize_rid(p_rid);
}

void ParticlesStorage::particles_free(RID p_rid) {
	Particles *particles = particles_owner.get_or_null(p_rid);

	particles->dependency.deleted_notify(p_rid);
	particles->update_list.remove_from_list();

	_particles_free_data(particles);
	particles_owner.free(p_rid);
}

void ParticlesStorage::particles_set_mode(RID p_particles, RSE::ParticlesMode p_mode) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	if (particles->mode == p_mode) {
		return;
	}

	_particles_free_data(particles);

	particles->mode = p_mode;
}

void ParticlesStorage::particles_set_emitting(RID p_particles, bool p_emitting) {
	ERR_FAIL_COND_MSG(GLES2::Config::get_singleton()->disable_particles_workaround, "Due to driver bugs, GPUParticles are not supported on Adreno 3XX devices. Please use CPUParticles instead.");

	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->emitting = p_emitting;
}

bool ParticlesStorage::particles_get_emitting(RID p_particles) {
	if (GLES2::Config::get_singleton()->disable_particles_workaround) {
		return false;
	}

	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL_V(particles, false);

	return particles->emitting;
}

void ParticlesStorage::_particles_free_data(Particles *particles) {
	// GLES2 simplification (Fase C): sem buffers GL para liberar.
	particles->userdata_count = 0;
	particles->instance_buffer_size_cache = 0;
	particles->instance_buffer_stride_cache = 0;
	particles->num_attrib_arrays_cache = 0;
	particles->process_buffer_stride_cache = 0;
	particles->front_vertex_array = 0;
	particles->front_process_buffer = 0;
	particles->front_instance_buffer = 0;
	particles->back_vertex_array = 0;
	particles->back_process_buffer = 0;
	particles->back_instance_buffer = 0;
	particles->last_frame_buffer = 0;
	particles->sort_buffer = 0;
	particles->sort_buffer_filled = false;
	particles->last_frame_buffer_filled = false;
	particles->frame_params_ubo = 0;
}

void ParticlesStorage::particles_set_amount(RID p_particles, int p_amount) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	if (particles->amount == p_amount) {
		return;
	}

	_particles_free_data(particles);

	particles->amount = p_amount;

	particles->prev_ticks = 0;
	particles->phase = 0;
	particles->prev_phase = 0;
	particles->clear = true;

	particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_PARTICLES);
}

void ParticlesStorage::particles_set_amount_ratio(RID p_particles, float p_amount_ratio) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->amount_ratio = p_amount_ratio;
}

void ParticlesStorage::particles_set_lifetime(RID p_particles, double p_lifetime) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	particles->lifetime = p_lifetime;
}

void ParticlesStorage::particles_set_one_shot(RID p_particles, bool p_one_shot) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	particles->one_shot = p_one_shot;
}

void ParticlesStorage::particles_set_pre_process_time(RID p_particles, double p_time) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	particles->pre_process_time = p_time;
}

void ParticlesStorage::particles_request_process_time(RID p_particles, real_t p_request_process_time, real_t p_request_process_time_residual) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	particles->request_process_time = p_request_process_time;
	particles->request_process_time_residual = p_request_process_time_residual;
}

void ParticlesStorage::particles_set_seed(RID p_particles, uint32_t p_seed) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	particles->random_seed = p_seed;
}

void ParticlesStorage::particles_set_explosiveness_ratio(RID p_particles, real_t p_ratio) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	particles->explosiveness = p_ratio;
}
void ParticlesStorage::particles_set_randomness_ratio(RID p_particles, real_t p_ratio) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	particles->randomness = p_ratio;
}

void ParticlesStorage::particles_set_custom_aabb(RID p_particles, const AABB &p_aabb) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	particles->custom_aabb = p_aabb;
	particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
}

void ParticlesStorage::particles_set_speed_scale(RID p_particles, double p_scale) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->speed_scale = p_scale;
}
void ParticlesStorage::particles_set_use_local_coordinates(RID p_particles, bool p_enable) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->use_local_coords = p_enable;
	particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_PARTICLES);
}

void ParticlesStorage::particles_set_fixed_fps(RID p_particles, int p_fps) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->fixed_fps = p_fps;

	_particles_free_data(particles);

	particles->prev_ticks = 0;
	particles->phase = 0;
	particles->prev_phase = 0;
	particles->clear = true;

	particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_PARTICLES);
}

void ParticlesStorage::particles_set_interpolate(RID p_particles, bool p_enable) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->interpolate = p_enable;
}

void ParticlesStorage::particles_set_fractional_delta(RID p_particles, bool p_enable) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->fractional_delta = p_enable;
}

void ParticlesStorage::particles_set_trails(RID p_particles, bool p_enable, double p_length) {
	if (p_enable) {
		WARN_PRINT_ONCE_ED("The Compatibility renderer does not support particle trails.");
	}
}

void ParticlesStorage::particles_set_trail_bind_poses(RID p_particles, const Vector<Transform3D> &p_bind_poses) {
	if (p_bind_poses.size() != 0) {
		WARN_PRINT_ONCE_ED("The Compatibility renderer does not support particle trails.");
	}
}

void ParticlesStorage::particles_set_collision_base_size(RID p_particles, real_t p_size) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->collision_base_size = p_size;
}

void ParticlesStorage::particles_set_transform_align(RID p_particles, RSE::ParticlesTransformAlign p_transform_align) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->transform_align = p_transform_align;
}

void ParticlesStorage::particles_set_transform_align_channel_filter(RID p_particles, RSE::ParticlesTransformAlignCustomSrc p_channel_filter) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->transform_align_channel_filter = p_channel_filter;
}

void ParticlesStorage::particles_set_transform_align_axis(RID p_particles, RSE::ParticlesTransformAlignAxis p_rotation_axis) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->transform_align_axis = p_rotation_axis;
}

void ParticlesStorage::particles_set_process_material(RID p_particles, RID p_material) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->process_material = p_material;
	particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_PARTICLES); //the instance buffer may have changed
}

RID ParticlesStorage::particles_get_process_material(RID p_particles) const {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL_V(particles, RID());

	return particles->process_material;
}

void ParticlesStorage::particles_set_draw_order(RID p_particles, RSE::ParticlesDrawOrder p_order) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->draw_order = p_order;
}

void ParticlesStorage::particles_set_draw_passes(RID p_particles, int p_passes) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->draw_passes.resize(p_passes);
}

void ParticlesStorage::particles_set_draw_pass_mesh(RID p_particles, int p_pass, RID p_mesh) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	ERR_FAIL_INDEX(p_pass, particles->draw_passes.size());
	particles->draw_passes.write[p_pass] = p_mesh;
	particles->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_PARTICLES);
}

void ParticlesStorage::particles_restart(RID p_particles) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->restart_request = true;
}

void ParticlesStorage::particles_set_subemitter(RID p_particles, RID p_subemitter_particles) {
	if (p_subemitter_particles.is_valid()) {
		WARN_PRINT_ONCE_ED("The Compatibility renderer does not support particle sub-emitters.");
	}
}

void ParticlesStorage::particles_emit(RID p_particles, const Transform3D &p_transform, const Vector3 &p_velocity, const Color &p_color, const Color &p_custom, uint32_t p_emit_flags) {
	WARN_PRINT_ONCE_ED("The Compatibility renderer does not support manually emitting particles.");
}

void ParticlesStorage::particles_request_process(RID p_particles) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	if (!particles->dirty) {
		particles->dirty = true;

		if (!particles->update_list.in_list()) {
			particle_update_list.add(&particles->update_list);
		}
	}
}

AABB ParticlesStorage::particles_get_current_aabb(RID p_particles) {
	// GLES2 simplification (Fase C): sem leitura GPU; retorna AABB customizada da CPU.
	const Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL_V(particles, AABB());
	return particles->custom_aabb;
}

AABB ParticlesStorage::particles_get_aabb(RID p_particles) const {
	const Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL_V(particles, AABB());

	return particles->custom_aabb;
}

void ParticlesStorage::particles_set_emission_transform(RID p_particles, const Transform3D &p_transform) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->emission_transform = p_transform;
}

void ParticlesStorage::particles_set_emitter_velocity(RID p_particles, const Vector3 &p_velocity) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->emitter_velocity = p_velocity;
}

void ParticlesStorage::particles_set_interp_to_end(RID p_particles, float p_interp) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);

	particles->interp_to_end = p_interp;
}

int ParticlesStorage::particles_get_draw_passes(RID p_particles) const {
	const Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL_V(particles, 0);

	return particles->draw_passes.size();
}

RID ParticlesStorage::particles_get_draw_pass_mesh(RID p_particles, int p_pass) const {
	const Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL_V(particles, RID());
	ERR_FAIL_INDEX_V(p_pass, particles->draw_passes.size(), RID());

	return particles->draw_passes[p_pass];
}

void ParticlesStorage::particles_add_collision(RID p_particles, RID p_particles_collision_instance) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	particles->collisions.insert(p_particles_collision_instance);
}

void ParticlesStorage::particles_remove_collision(RID p_particles, RID p_particles_collision_instance) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	particles->collisions.erase(p_particles_collision_instance);
}

void ParticlesStorage::particles_set_canvas_sdf_collision(RID p_particles, bool p_enable, const Transform2D &p_xform, const Rect2 &p_to_screen, GLuint p_texture) {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL(particles);
	particles->has_sdf_collision = p_enable;
	particles->sdf_collision_transform = p_xform;
	particles->sdf_collision_to_screen = p_to_screen;
	particles->sdf_collision_texture = p_texture;
}

// Does one step of processing particles by reading from back_process_buffer and writing to front_process_buffer.
void ParticlesStorage::_particles_process(Particles *p_particles, double p_delta) {
	// GLES2 simplification (Fase C): sem processamento GPU (transform feedback).
	(void)p_particles;
	(void)p_delta;
}

void ParticlesStorage::particles_set_view_axis(RID p_particles, const Vector3 &p_axis, const Vector3 &p_up_axis) {
	// GLES2 simplification (Fase C): sem ordenacao por profundidade nem TF.
	(void)p_particles;
	(void)p_axis;
	(void)p_up_axis;
}

void ParticlesStorage::_particles_update_buffers(Particles *particles) {
	// GLES2 simplification (Fase C): sem alocacao de VAO/VBO de particulas.
	(void)particles;
}

void ParticlesStorage::_particles_allocate_history_buffers(Particles *particles) {
	// GLES2 simplification (Fase C): sem history buffers.
	(void)particles;
}
void ParticlesStorage::_particles_update_instance_buffer(Particles *particles, const Vector3 &p_axis, const Vector3 &p_up_axis) {
	// GLES2 simplification (Fase C): sem copy shader / transform feedback.
	(void)particles;
	(void)p_axis;
	(void)p_up_axis;
}

void ParticlesStorage::update_particles() {
	// GLES2 simplification (Fase C): particulas GPU desativadas; apenas drena a lista.
	while (particle_update_list.first()) {
		Particles *particles = particle_update_list.first()->self();
		particles->update_list.remove_from_list();
		particles->dirty = false;
	}
}

template <typename ParticleInstanceData>
void ParticlesStorage::_particles_reverse_lifetime_sort(Particles *particles) {
	// GLES2 simplification (Fase C): sem sort GPU.
	(void)particles;
}

Dependency *ParticlesStorage::particles_get_dependency(RID p_particles) const {
	Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL_V(particles, nullptr);

	return &particles->dependency;
}

bool ParticlesStorage::particles_is_inactive(RID p_particles) const {
	const Particles *particles = particles_owner.get_or_null(p_particles);
	ERR_FAIL_NULL_V(particles, false);
	return !particles->emitting && particles->inactive;
}

/* PARTICLES COLLISION API */

RID ParticlesStorage::particles_collision_allocate() {
	return particles_collision_owner.allocate_rid();
}
void ParticlesStorage::particles_collision_initialize(RID p_rid) {
	particles_collision_owner.initialize_rid(p_rid, ParticlesCollision());
}

void ParticlesStorage::particles_collision_free(RID p_rid) {
	// GLES2 simplification (Fase C): sem heightfield texture/FBO para liberar.
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_rid);
	particles_collision->heightfield_texture = 0;
	particles_collision->heightfield_fb = 0;
	particles_collision->dependency.deleted_notify(p_rid);
	particles_collision_owner.free(p_rid);
}

GLuint ParticlesStorage::particles_collision_get_heightfield_framebuffer(RID p_particles_collision) const {
	// GLES2 simplification (Fase C): sem heightfield texture/FBO.
	(void)p_particles_collision;
	return 0;
}

void ParticlesStorage::particles_collision_set_collision_type(RID p_particles_collision, RSE::ParticlesCollisionType p_type) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL(particles_collision);
	if (p_type == particles_collision->type) {
		return;
	}
	// GLES2 simplification (Fase C): sem heightfield texture/FBO.
	particles_collision->heightfield_texture = 0;
	particles_collision->heightfield_fb = 0;
	particles_collision->type = p_type;
	particles_collision->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
}

void ParticlesStorage::particles_collision_set_cull_mask(RID p_particles_collision, uint32_t p_cull_mask) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL(particles_collision);
	particles_collision->cull_mask = p_cull_mask;
	particles_collision->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_CULL_MASK);
}

uint32_t ParticlesStorage::particles_collision_get_cull_mask(RID p_particles_collision) const {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL_V(particles_collision, 0);
	return particles_collision->cull_mask;
}

void ParticlesStorage::particles_collision_set_sphere_radius(RID p_particles_collision, real_t p_radius) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL(particles_collision);

	particles_collision->radius = p_radius;
	particles_collision->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
}

void ParticlesStorage::particles_collision_set_box_extents(RID p_particles_collision, const Vector3 &p_extents) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL(particles_collision);

	particles_collision->extents = p_extents;
	particles_collision->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
}

void ParticlesStorage::particles_collision_set_attractor_strength(RID p_particles_collision, real_t p_strength) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL(particles_collision);

	particles_collision->attractor_strength = p_strength;
}

void ParticlesStorage::particles_collision_set_attractor_directionality(RID p_particles_collision, real_t p_directionality) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL(particles_collision);

	particles_collision->attractor_directionality = p_directionality;
}

void ParticlesStorage::particles_collision_set_attractor_attenuation(RID p_particles_collision, real_t p_curve) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL(particles_collision);

	particles_collision->attractor_attenuation = p_curve;
}

void ParticlesStorage::particles_collision_set_field_texture(RID p_particles_collision, RID p_texture) {
	WARN_PRINT_ONCE_ED("The Compatibility renderer does not support SDF collisions in 3D particle shaders");
}

void ParticlesStorage::particles_collision_height_field_update(RID p_particles_collision) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL(particles_collision);
	particles_collision->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
}

void ParticlesStorage::particles_collision_set_height_field_resolution(RID p_particles_collision, RSE::ParticlesCollisionHeightfieldResolution p_resolution) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL(particles_collision);
	ERR_FAIL_INDEX(p_resolution, RSE::PARTICLES_COLLISION_HEIGHTFIELD_RESOLUTION_MAX);
	if (particles_collision->heightfield_resolution == p_resolution) {
		return;
	}
	particles_collision->heightfield_resolution = p_resolution;
	// GLES2 simplification (Fase C): sem heightfield texture/FBO.
	particles_collision->heightfield_texture = 0;
	particles_collision->heightfield_fb = 0;
}

AABB ParticlesStorage::particles_collision_get_aabb(RID p_particles_collision) const {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL_V(particles_collision, AABB());

	switch (particles_collision->type) {
		case RSE::PARTICLES_COLLISION_TYPE_SPHERE_ATTRACT:
		case RSE::PARTICLES_COLLISION_TYPE_SPHERE_COLLIDE: {
			AABB aabb;
			aabb.position = -Vector3(1, 1, 1) * particles_collision->radius;
			aabb.size = Vector3(2, 2, 2) * particles_collision->radius;
			return aabb;
		}
		default: {
			AABB aabb;
			aabb.position = -particles_collision->extents;
			aabb.size = particles_collision->extents * 2;
			return aabb;
		}
	}
}

Vector3 ParticlesStorage::particles_collision_get_extents(RID p_particles_collision) const {
	const ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL_V(particles_collision, Vector3());
	return particles_collision->extents;
}

bool ParticlesStorage::particles_collision_is_heightfield(RID p_particles_collision) const {
	const ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL_V(particles_collision, false);
	return particles_collision->type == RSE::PARTICLES_COLLISION_TYPE_HEIGHTFIELD_COLLIDE;
}

uint32_t ParticlesStorage::particles_collision_get_height_field_mask(RID p_particles_collision) const {
	const ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL_V(particles_collision, false);
	return particles_collision->heightfield_mask;
}

void ParticlesStorage::particles_collision_set_height_field_mask(RID p_particles_collision, uint32_t p_heightfield_mask) {
	ParticlesCollision *particles_collision = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL(particles_collision);
	particles_collision->heightfield_mask = p_heightfield_mask;
}

Dependency *ParticlesStorage::particles_collision_get_dependency(RID p_particles_collision) const {
	ParticlesCollision *pc = particles_collision_owner.get_or_null(p_particles_collision);
	ERR_FAIL_NULL_V(pc, nullptr);

	return &pc->dependency;
}

/* Particles collision instance */

RID ParticlesStorage::particles_collision_instance_create(RID p_collision) {
	ParticlesCollisionInstance pci;
	pci.collision = p_collision;
	return particles_collision_instance_owner.make_rid(pci);
}

void ParticlesStorage::particles_collision_instance_free(RID p_rid) {
	particles_collision_instance_owner.free(p_rid);
}

void ParticlesStorage::particles_collision_instance_set_transform(RID p_collision_instance, const Transform3D &p_transform) {
	ParticlesCollisionInstance *pci = particles_collision_instance_owner.get_or_null(p_collision_instance);
	ERR_FAIL_NULL(pci);
	pci->transform = p_transform;
}

void ParticlesStorage::particles_collision_instance_set_active(RID p_collision_instance, bool p_active) {
	ParticlesCollisionInstance *pci = particles_collision_instance_owner.get_or_null(p_collision_instance);
	ERR_FAIL_NULL(pci);
	pci->active = p_active;
}

#endif // GLES2_ENABLED
