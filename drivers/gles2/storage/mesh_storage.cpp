/**************************************************************************/
/*  mesh_storage.cpp                                                      */
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

#include "mesh_storage.h"

#ifdef GLES2_ENABLED

#include "drivers/gles2/storage/config.h"
#include "drivers/gles2/storage/texture_storage.h"
#include "drivers/gles2/storage/utilities.h"
#include "servers/rendering/renderer_viewport.h"
#include "servers/rendering/rendering_server.h"

using namespace GLES2;

MeshStorage *MeshStorage::singleton = nullptr;

MeshStorage *MeshStorage::get_singleton() {
	return singleton;
}

MeshStorage::MeshStorage() {
	singleton = this;
	// No transform-feedback skeleton shader: skinning runs on the CPU.
}

MeshStorage::~MeshStorage() {
	singleton = nullptr;
}

/* MESH API */

RID MeshStorage::mesh_allocate() {
	return mesh_owner.allocate_rid();
}

void MeshStorage::mesh_initialize(RID p_rid) {
	mesh_owner.initialize_rid(p_rid, Mesh());
}

void MeshStorage::mesh_free(RID p_rid) {
	mesh_clear(p_rid);
	mesh_set_shadow_mesh(p_rid, RID());
	Mesh *mesh = mesh_owner.get_or_null(p_rid);
	ERR_FAIL_NULL(mesh);

	mesh->dependency.deleted_notify(p_rid);
	if (mesh->instances.size()) {
		ERR_PRINT("deleting mesh with active instances");
	}
	if (mesh->shadow_owners.size()) {
		for (Mesh *E : mesh->shadow_owners) {
			Mesh *shadow_owner = E;
			shadow_owner->shadow_mesh = RID();
			shadow_owner->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MESH);
		}
	}
	mesh_owner.free(p_rid);
}

void MeshStorage::mesh_set_blend_shape_count(RID p_mesh, int p_blend_shape_count) {
	ERR_FAIL_COND(p_blend_shape_count < 0);

	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL(mesh);

	ERR_FAIL_COND(mesh->surface_count > 0); //surfaces already exist
	mesh->blend_shape_count = p_blend_shape_count;
}

bool MeshStorage::mesh_needs_instance(RID p_mesh, bool p_has_skeleton) {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL_V(mesh, false);

	return mesh->blend_shape_count > 0 || (mesh->has_bone_weights && p_has_skeleton);
}

void MeshStorage::mesh_add_surface(RID p_mesh, const RenderingServerTypes::SurfaceData &p_surface) {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL(mesh);

	ERR_FAIL_COND(mesh->surface_count == RSE::MAX_MESH_SURFACES);

#ifdef DEBUG_ENABLED
	//do a validation, to catch errors first
	{
		uint32_t stride = 0;
		uint32_t attrib_stride = 0;
		uint32_t skin_stride = 0;

		for (int i = 0; i < RSE::ARRAY_WEIGHTS; i++) {
			if ((p_surface.format & (1ULL << i))) {
				switch (i) {
					case RSE::ARRAY_VERTEX: {
						if ((p_surface.format & RSE::ARRAY_FLAG_USE_2D_VERTICES) || (p_surface.format & RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES)) {
							stride += sizeof(float) * 2;
						} else {
							stride += sizeof(float) * 3;
						}
					} break;
					case RSE::ARRAY_NORMAL: {
						stride += sizeof(uint16_t) * 2;

					} break;
					case RSE::ARRAY_TANGENT: {
						if (!(p_surface.format & RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES)) {
							stride += sizeof(uint16_t) * 2;
						}
					} break;
					case RSE::ARRAY_COLOR: {
						attrib_stride += sizeof(uint32_t);
					} break;
					case RSE::ARRAY_TEX_UV: {
						if (p_surface.format & RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES) {
							attrib_stride += sizeof(uint16_t) * 2;
						} else {
							attrib_stride += sizeof(float) * 2;
						}
					} break;
					case RSE::ARRAY_TEX_UV2: {
						if (p_surface.format & RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES) {
							attrib_stride += sizeof(uint16_t) * 2;
						} else {
							attrib_stride += sizeof(float) * 2;
						}
					} break;
					case RSE::ARRAY_CUSTOM0:
					case RSE::ARRAY_CUSTOM1:
					case RSE::ARRAY_CUSTOM2:
					case RSE::ARRAY_CUSTOM3: {
						int idx = i - RSE::ARRAY_CUSTOM0;
						uint32_t fmt_shift[RSE::ARRAY_CUSTOM_COUNT] = { RSE::ARRAY_FORMAT_CUSTOM0_SHIFT, RSE::ARRAY_FORMAT_CUSTOM1_SHIFT, RSE::ARRAY_FORMAT_CUSTOM2_SHIFT, RSE::ARRAY_FORMAT_CUSTOM3_SHIFT };
						uint32_t fmt = (p_surface.format >> fmt_shift[idx]) & RSE::ARRAY_FORMAT_CUSTOM_MASK;
						uint32_t fmtsize[RSE::ARRAY_CUSTOM_MAX] = { 4, 4, 4, 8, 4, 8, 12, 16 };
						attrib_stride += fmtsize[fmt];

					} break;
					case RSE::ARRAY_WEIGHTS:
					case RSE::ARRAY_BONES: {
						//uses a separate array
						bool use_8 = p_surface.format & RSE::ARRAY_FLAG_USE_8_BONE_WEIGHTS;
						skin_stride += sizeof(int16_t) * (use_8 ? 16 : 8);
					} break;
				}
			}
		}

		int expected_size = stride * p_surface.vertex_count;
		ERR_FAIL_COND_MSG(expected_size != p_surface.vertex_data.size(), "Size of vertex data provided (" + itos(p_surface.vertex_data.size()) + ") does not match expected (" + itos(expected_size) + ")");

		int bs_expected_size = expected_size * mesh->blend_shape_count;

		ERR_FAIL_COND_MSG(bs_expected_size != p_surface.blend_shape_data.size(), "Size of blend shape data provided (" + itos(p_surface.blend_shape_data.size()) + ") does not match expected (" + itos(bs_expected_size) + ")");

		int expected_attrib_size = attrib_stride * p_surface.vertex_count;
		ERR_FAIL_COND_MSG(expected_attrib_size != p_surface.attribute_data.size(), "Size of attribute data provided (" + itos(p_surface.attribute_data.size()) + ") does not match expected (" + itos(expected_attrib_size) + ")");

		if ((p_surface.format & RSE::ARRAY_FORMAT_WEIGHTS) && (p_surface.format & RSE::ARRAY_FORMAT_BONES)) {
			expected_size = skin_stride * p_surface.vertex_count;
			ERR_FAIL_COND_MSG(expected_size != p_surface.skin_data.size(), "Size of skin data provided (" + itos(p_surface.skin_data.size()) + ") does not match expected (" + itos(expected_size) + ")");
		}
	}

#endif

	uint64_t surface_version = p_surface.format & (uint64_t(RSE::ARRAY_FLAG_FORMAT_VERSION_MASK) << RSE::ARRAY_FLAG_FORMAT_VERSION_SHIFT);
	RenderingServerTypes::SurfaceData new_surface = p_surface;
#ifdef DISABLE_DEPRECATED

	ERR_FAIL_COND_MSG(surface_version != RSE::ARRAY_FLAG_FORMAT_CURRENT_VERSION, "Surface version provided (" + itos(int(surface_version >> RSE::ARRAY_FLAG_FORMAT_VERSION_SHIFT)) + ") does not match current version (" + itos(RSE::ARRAY_FLAG_FORMAT_CURRENT_VERSION >> RSE::ARRAY_FLAG_FORMAT_VERSION_SHIFT) + ")");

#else

	if (surface_version != uint64_t(RSE::ARRAY_FLAG_FORMAT_CURRENT_VERSION)) {
		RS::get_singleton()->fix_surface_compatibility(new_surface);
		surface_version = new_surface.format & (uint64_t(RSE::ARRAY_FLAG_FORMAT_VERSION_MASK) << RSE::ARRAY_FLAG_FORMAT_VERSION_SHIFT);
		ERR_FAIL_COND_MSG(surface_version != RSE::ARRAY_FLAG_FORMAT_CURRENT_VERSION,
				vformat("Surface version provided (%d) does not match current version (%d).",
						(surface_version >> RSE::ARRAY_FLAG_FORMAT_VERSION_SHIFT) & RSE::ARRAY_FLAG_FORMAT_VERSION_MASK,
						(RSE::ARRAY_FLAG_FORMAT_CURRENT_VERSION >> RSE::ARRAY_FLAG_FORMAT_VERSION_SHIFT) & RSE::ARRAY_FLAG_FORMAT_VERSION_MASK));
	}
#endif

	Mesh::Surface *s = memnew(Mesh::Surface);

	s->format = new_surface.format;
	s->primitive = new_surface.primitive;

	if (new_surface.vertex_data.size()) {
		glGenBuffers(1, &s->vertex_buffer);
		glBindBuffer(GL_ARRAY_BUFFER, s->vertex_buffer);
		// If we have an uncompressed surface that contains normals, but not tangents, we need to differentiate the array
		// from a compressed array in the shader. To do so, we allow the normal to read 4 components out of the buffer
		// But only give it 2 components per normal. So essentially, each vertex reads the next normal in normal.zw.
		// This allows us to avoid adding a shader permutation, and avoid passing dummy tangents. Since the stride is kept small
		// this should still be a net win for bandwidth.
		// If we do this, then the last normal will read past the end of the array. So we need to pad the array with dummy data.
		if (!(new_surface.format & RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES) && (new_surface.format & RSE::ARRAY_FORMAT_NORMAL) && !(new_surface.format & RSE::ARRAY_FORMAT_TANGENT)) {
			// Unfortunately, we need to copy the buffer, which is fine as doing a resize triggers a CoW anyway.
			Vector<uint8_t> new_vertex_data;
			new_vertex_data.resize_initialized(new_surface.vertex_data.size() + sizeof(uint16_t) * 2);
			memcpy(new_vertex_data.ptrw(), new_surface.vertex_data.ptr(), new_surface.vertex_data.size());
			GLES2::Utilities::get_singleton()->buffer_allocate_data(GL_ARRAY_BUFFER, s->vertex_buffer, new_vertex_data.size(), new_vertex_data.ptr(), (s->format & RSE::ARRAY_FLAG_USE_DYNAMIC_UPDATE) ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW, "Mesh vertex buffer");
			s->vertex_buffer_size = new_vertex_data.size();
		} else {
			GLES2::Utilities::get_singleton()->buffer_allocate_data(GL_ARRAY_BUFFER, s->vertex_buffer, new_surface.vertex_data.size(), new_surface.vertex_data.ptr(), (s->format & RSE::ARRAY_FLAG_USE_DYNAMIC_UPDATE) ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW, "Mesh vertex buffer");
			s->vertex_buffer_size = new_surface.vertex_data.size();
		}
		// GLES2 simplification: retain a CPU copy for software skinning.
		s->vertex_data_cpu = new_surface.vertex_data;
	}

	if (new_surface.attribute_data.size()) {
		glGenBuffers(1, &s->attribute_buffer);
		glBindBuffer(GL_ARRAY_BUFFER, s->attribute_buffer);
		GLES2::Utilities::get_singleton()->buffer_allocate_data(GL_ARRAY_BUFFER, s->attribute_buffer, new_surface.attribute_data.size(), new_surface.attribute_data.ptr(), (s->format & RSE::ARRAY_FLAG_USE_DYNAMIC_UPDATE) ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW, "Mesh attribute buffer");
		s->attribute_buffer_size = new_surface.attribute_data.size();
	}

	if (new_surface.skin_data.size()) {
		glGenBuffers(1, &s->skin_buffer);
		glBindBuffer(GL_ARRAY_BUFFER, s->skin_buffer);
		GLES2::Utilities::get_singleton()->buffer_allocate_data(GL_ARRAY_BUFFER, s->skin_buffer, new_surface.skin_data.size(), new_surface.skin_data.ptr(), (s->format & RSE::ARRAY_FLAG_USE_DYNAMIC_UPDATE) ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW, "Mesh skin buffer");
		s->skin_buffer_size = new_surface.skin_data.size();
		// GLES2 simplification: retain a CPU copy for software skinning.
		s->skin_data_cpu = new_surface.skin_data;
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	s->vertex_count = new_surface.vertex_count;

	if (new_surface.format & RSE::ARRAY_FORMAT_BONES) {
		mesh->has_bone_weights = true;
	}

	if (new_surface.index_count) {
		bool is_index_16 = new_surface.vertex_count <= 65536 && new_surface.vertex_count > 0;
		glGenBuffers(1, &s->index_buffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s->index_buffer);
		GLES2::Utilities::get_singleton()->buffer_allocate_data(GL_ELEMENT_ARRAY_BUFFER, s->index_buffer, new_surface.index_data.size(), new_surface.index_data.ptr(), GL_STATIC_DRAW, "Mesh index buffer");
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); //unbind
		s->index_count = new_surface.index_count;
		s->index_buffer_size = new_surface.index_data.size();

		if (new_surface.lods.size()) {
			s->lods = memnew_arr(Mesh::Surface::LOD, new_surface.lods.size());
			s->lod_count = new_surface.lods.size();

			for (int i = 0; i < new_surface.lods.size(); i++) {
				glGenBuffers(1, &s->lods[i].index_buffer);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s->lods[i].index_buffer);
				GLES2::Utilities::get_singleton()->buffer_allocate_data(GL_ELEMENT_ARRAY_BUFFER, s->lods[i].index_buffer, new_surface.lods[i].index_data.size(), new_surface.lods[i].index_data.ptr(), GL_STATIC_DRAW, "Mesh index buffer LOD[" + itos(i) + "]");
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); //unbind
				s->lods[i].edge_length = new_surface.lods[i].edge_length;
				s->lods[i].index_count = new_surface.lods[i].index_data.size() / (is_index_16 ? 2 : 4);
				s->lods[i].index_buffer_size = new_surface.lods[i].index_data.size();
			}
		}
	}

	ERR_FAIL_COND_MSG(!new_surface.index_count && !new_surface.vertex_count, "Meshes must contain a vertex array, an index array, or both");

	if (GLES2::Config::get_singleton()->generate_wireframes && s->primitive == RSE::PRIMITIVE_TRIANGLES) {
		// Generate wireframes. This is mostly used by the editor.
		s->wireframe = memnew(Mesh::Surface::Wireframe);
		Vector<uint32_t> wf_indices;
		uint32_t &wf_index_count = s->wireframe->index_count;
		uint32_t *wr = nullptr;

		if (new_surface.format & RSE::ARRAY_FORMAT_INDEX) {
			wf_index_count = s->index_count * 2;
			wf_indices.resize(wf_index_count);

			Vector<uint8_t> ir = new_surface.index_data;
			wr = wf_indices.ptrw();

			if (new_surface.vertex_count <= 65536) {
				// Read 16 bit indices.
				const uint16_t *src_idx = (const uint16_t *)ir.ptr();
				for (uint32_t i = 0; i + 5 < wf_index_count; i += 6) {
					// We use GL_LINES instead of GL_TRIANGLES for drawing these primitives later,
					// so we need double the indices for each triangle.
					wr[i + 0] = src_idx[i / 2];
					wr[i + 1] = src_idx[i / 2 + 1];
					wr[i + 2] = src_idx[i / 2 + 1];
					wr[i + 3] = src_idx[i / 2 + 2];
					wr[i + 4] = src_idx[i / 2 + 2];
					wr[i + 5] = src_idx[i / 2];
				}

			} else {
				// Read 32 bit indices.
				const uint32_t *src_idx = (const uint32_t *)ir.ptr();
				for (uint32_t i = 0; i + 5 < wf_index_count; i += 6) {
					wr[i + 0] = src_idx[i / 2];
					wr[i + 1] = src_idx[i / 2 + 1];
					wr[i + 2] = src_idx[i / 2 + 1];
					wr[i + 3] = src_idx[i / 2 + 2];
					wr[i + 4] = src_idx[i / 2 + 2];
					wr[i + 5] = src_idx[i / 2];
				}
			}
		} else {
			// Not using indices.
			wf_index_count = s->vertex_count * 2;
			wf_indices.resize(wf_index_count);
			wr = wf_indices.ptrw();

			for (uint32_t i = 0; i + 5 < wf_index_count; i += 6) {
				wr[i + 0] = i / 2;
				wr[i + 1] = i / 2 + 1;
				wr[i + 2] = i / 2 + 1;
				wr[i + 3] = i / 2 + 2;
				wr[i + 4] = i / 2 + 2;
				wr[i + 5] = i / 2;
			}
		}

		s->wireframe->index_buffer_size = wf_index_count * sizeof(uint32_t);
		glGenBuffers(1, &s->wireframe->index_buffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s->wireframe->index_buffer);
		GLES2::Utilities::get_singleton()->buffer_allocate_data(GL_ELEMENT_ARRAY_BUFFER, s->wireframe->index_buffer, s->wireframe->index_buffer_size, wr, GL_STATIC_DRAW, "Mesh wireframe index buffer");
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); // unbind
	}

	s->aabb = new_surface.aabb;
	s->bone_aabbs = new_surface.bone_aabbs; //only really useful for returning them.
	s->mesh_to_skeleton_xform = p_surface.mesh_to_skeleton_xform;

	s->uv_scale = new_surface.uv_scale;

	if (new_surface.skin_data.size() || mesh->blend_shape_count > 0) {
		// GLES2 simplification: blend shapes are retained on the CPU for software
		// skinning; no per-shape GPU buffers are created.
		s->blend_shape_data_cpu = new_surface.blend_shape_data;

		// No GPU blend-shape buffers: the CPU copy (blend_shape_data_cpu) feeds software skinning.

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	if (mesh->surface_count == 0) {
		mesh->aabb = new_surface.aabb;
	} else {
		mesh->aabb.merge_with(new_surface.aabb);
	}
	mesh->skeleton_aabb_version = 0;

	s->material = new_surface.material;

	mesh->surfaces = (Mesh::Surface **)memrealloc(mesh->surfaces, sizeof(Mesh::Surface *) * (mesh->surface_count + 1));
	mesh->surfaces[mesh->surface_count] = s;
	mesh->surface_count++;

	for (MeshInstance *mi : mesh->instances) {
		_mesh_instance_add_surface(mi, mesh, mesh->surface_count - 1);
	}

	mesh->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MESH);

	for (Mesh *E : mesh->shadow_owners) {
		Mesh *shadow_owner = E;
		shadow_owner->shadow_mesh = RID();
		shadow_owner->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MESH);
	}

	mesh->material_cache.clear();
}

void MeshStorage::_mesh_surface_clear(Mesh *mesh, int p_surface) {
	Mesh::Surface &s = *mesh->surfaces[p_surface];

	if (s.vertex_buffer != 0) {
		GLES2::Utilities::get_singleton()->buffer_free_data(s.vertex_buffer);
		s.vertex_buffer = 0;
	}

	if (s.version_count != 0) {
		for (uint32_t j = 0; j < s.version_count; j++) {
			glDeleteVertexArrays(1, &s.versions[j].vertex_array);
			s.versions[j].vertex_array = 0;
		}
	}

	if (s.attribute_buffer != 0) {
		GLES2::Utilities::get_singleton()->buffer_free_data(s.attribute_buffer);
		s.attribute_buffer = 0;
	}

	if (s.skin_buffer != 0) {
		GLES2::Utilities::get_singleton()->buffer_free_data(s.skin_buffer);
		s.skin_buffer = 0;
	}

	if (s.index_buffer != 0) {
		GLES2::Utilities::get_singleton()->buffer_free_data(s.index_buffer);
		s.index_buffer = 0;
	}

	if (s.versions) {
		memfree(s.versions); // reallocs, so free with memfree.
	}

	if (s.wireframe) {
		GLES2::Utilities::get_singleton()->buffer_free_data(s.wireframe->index_buffer);
		memdelete(s.wireframe);
	}

	if (s.lod_count) {
		for (uint32_t j = 0; j < s.lod_count; j++) {
			if (s.lods[j].index_buffer != 0) {
				GLES2::Utilities::get_singleton()->buffer_free_data(s.lods[j].index_buffer);
				s.lods[j].index_buffer = 0;
			}
		}
		memdelete_arr(s.lods);
	}

	// No GPU blend-shape buffers exist (CPU skinning only).

	memdelete(mesh->surfaces[p_surface]);
}

int MeshStorage::mesh_get_blend_shape_count(RID p_mesh) const {
	const Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL_V(mesh, -1);
	return mesh->blend_shape_count;
}

void MeshStorage::mesh_set_blend_shape_mode(RID p_mesh, RSE::BlendShapeMode p_mode) {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL(mesh);
	ERR_FAIL_INDEX((int)p_mode, 2);

	mesh->blend_shape_mode = p_mode;
}

RSE::BlendShapeMode MeshStorage::mesh_get_blend_shape_mode(RID p_mesh) const {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL_V(mesh, RSE::BLEND_SHAPE_MODE_NORMALIZED);
	return mesh->blend_shape_mode;
}

void MeshStorage::mesh_surface_update_vertex_region(RID p_mesh, int p_surface, int p_offset, const Vector<uint8_t> &p_data) {
	ERR_FAIL_COND(p_data.is_empty());
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL(mesh);
	ERR_FAIL_UNSIGNED_INDEX((uint32_t)p_surface, mesh->surface_count);

	uint64_t data_size = p_data.size();
	ERR_FAIL_COND(p_offset + data_size > mesh->surfaces[p_surface]->vertex_buffer_size);
	const uint8_t *r = p_data.ptr();

	glBindBuffer(GL_ARRAY_BUFFER, mesh->surfaces[p_surface]->vertex_buffer);
	glBufferSubData(GL_ARRAY_BUFFER, p_offset, data_size, r);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// GLES2 simplification: mirror into the CPU copy (unpadded layout).
	Vector<uint8_t> &cpu = mesh->surfaces[p_surface]->vertex_data_cpu;
	if (p_offset < cpu.size()) {
		uint64_t copy_size = MIN(data_size, uint64_t(cpu.size() - p_offset));
		memcpy(cpu.ptrw() + p_offset, r, copy_size);
	}
}

void MeshStorage::mesh_surface_update_attribute_region(RID p_mesh, int p_surface, int p_offset, const Vector<uint8_t> &p_data) {
	ERR_FAIL_COND(p_data.is_empty());
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL(mesh);
	ERR_FAIL_UNSIGNED_INDEX((uint32_t)p_surface, mesh->surface_count);

	uint64_t data_size = p_data.size();
	ERR_FAIL_COND(p_offset + data_size > mesh->surfaces[p_surface]->attribute_buffer_size);
	const uint8_t *r = p_data.ptr();

	glBindBuffer(GL_ARRAY_BUFFER, mesh->surfaces[p_surface]->attribute_buffer);
	glBufferSubData(GL_ARRAY_BUFFER, p_offset, data_size, r);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void MeshStorage::mesh_surface_update_skin_region(RID p_mesh, int p_surface, int p_offset, const Vector<uint8_t> &p_data) {
	ERR_FAIL_COND(p_data.is_empty());
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL(mesh);
	ERR_FAIL_UNSIGNED_INDEX((uint32_t)p_surface, mesh->surface_count);

	uint64_t data_size = p_data.size();
	ERR_FAIL_COND(p_offset + data_size > mesh->surfaces[p_surface]->skin_buffer_size);
	const uint8_t *r = p_data.ptr();

	glBindBuffer(GL_ARRAY_BUFFER, mesh->surfaces[p_surface]->skin_buffer);
	glBufferSubData(GL_ARRAY_BUFFER, p_offset, data_size, r);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// GLES2 simplification: mirror into the CPU copy.
	Vector<uint8_t> &cpu = mesh->surfaces[p_surface]->skin_data_cpu;
	if (p_offset < cpu.size()) {
		uint64_t copy_size = MIN(data_size, uint64_t(cpu.size() - p_offset));
		memcpy(cpu.ptrw() + p_offset, r, copy_size);
	}
}

void MeshStorage::mesh_surface_update_index_region(RID p_mesh, int p_surface, int p_offset, const Vector<uint8_t> &p_data) {
	ERR_FAIL_COND(p_data.is_empty());
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL(mesh);
	ERR_FAIL_UNSIGNED_INDEX((uint32_t)p_surface, mesh->surface_count);

	uint64_t data_size = p_data.size();
	ERR_FAIL_COND(p_offset + data_size > mesh->surfaces[p_surface]->index_buffer_size);
	const uint8_t *r = p_data.ptr();

	glBindBuffer(GL_ARRAY_BUFFER, mesh->surfaces[p_surface]->index_buffer);
	glBufferSubData(GL_ARRAY_BUFFER, p_offset, data_size, r);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void MeshStorage::mesh_surface_set_material(RID p_mesh, int p_surface, RID p_material) {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL(mesh);
	ERR_FAIL_UNSIGNED_INDEX((uint32_t)p_surface, mesh->surface_count);
	mesh->surfaces[p_surface]->material = p_material;

	mesh->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MATERIAL);
	mesh->material_cache.clear();
}

RID MeshStorage::mesh_surface_get_material(RID p_mesh, int p_surface) const {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL_V(mesh, RID());
	ERR_FAIL_UNSIGNED_INDEX_V((uint32_t)p_surface, mesh->surface_count, RID());

	return mesh->surfaces[p_surface]->material;
}

RenderingServerTypes::SurfaceData MeshStorage::mesh_get_surface(RID p_mesh, int p_surface) const {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL_V(mesh, RenderingServerTypes::SurfaceData());
	ERR_FAIL_UNSIGNED_INDEX_V((uint32_t)p_surface, mesh->surface_count, RenderingServerTypes::SurfaceData());

	Mesh::Surface &s = *mesh->surfaces[p_surface];

	RenderingServerTypes::SurfaceData sd;
	sd.format = s.format;
	if (s.vertex_buffer != 0) {
		sd.vertex_data = Utilities::buffer_get_data(GL_ARRAY_BUFFER, s.vertex_buffer, s.vertex_buffer_size);

		// When using an uncompressed buffer with normals, but without tangents, we have to trim the padding.
		if (!(s.format & RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES) && (s.format & RSE::ARRAY_FORMAT_NORMAL) && !(s.format & RSE::ARRAY_FORMAT_TANGENT)) {
			sd.vertex_data.resize(sd.vertex_data.size() - sizeof(uint16_t) * 2);
		}
	}

	if (s.attribute_buffer != 0) {
		sd.attribute_data = Utilities::buffer_get_data(GL_ARRAY_BUFFER, s.attribute_buffer, s.attribute_buffer_size);
	}

	if (s.skin_buffer != 0) {
		sd.skin_data = Utilities::buffer_get_data(GL_ARRAY_BUFFER, s.skin_buffer, s.skin_buffer_size);
	}

	sd.vertex_count = s.vertex_count;
	sd.index_count = s.index_count;
	sd.primitive = s.primitive;

	if (sd.index_count) {
		sd.index_data = Utilities::buffer_get_data(GL_ELEMENT_ARRAY_BUFFER, s.index_buffer, s.index_buffer_size);
	}

	sd.aabb = s.aabb;
	for (uint32_t i = 0; i < s.lod_count; i++) {
		RenderingServerTypes::SurfaceData::LOD lod;
		lod.edge_length = s.lods[i].edge_length;
		lod.index_data = Utilities::buffer_get_data(GL_ELEMENT_ARRAY_BUFFER, s.lods[i].index_buffer, s.lods[i].index_buffer_size);
		sd.lods.push_back(lod);
	}

	sd.bone_aabbs = s.bone_aabbs;
	sd.mesh_to_skeleton_xform = s.mesh_to_skeleton_xform;

	if (mesh->blend_shape_count) {
		// Served from the retained CPU copy (no GPU blend-shape buffers exist).
		sd.blend_shape_data = s.blend_shape_data_cpu;
	}

	sd.uv_scale = s.uv_scale;

	return sd;
}

int MeshStorage::mesh_get_surface_count(RID p_mesh) const {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL_V(mesh, 0);
	return mesh->surface_count;
}

void MeshStorage::mesh_set_custom_aabb(RID p_mesh, const AABB &p_aabb) {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL(mesh);
	mesh->custom_aabb = p_aabb;

	mesh->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
}

AABB MeshStorage::mesh_get_custom_aabb(RID p_mesh) const {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL_V(mesh, AABB());
	return mesh->custom_aabb;
}

AABB MeshStorage::mesh_get_aabb(RID p_mesh, RID p_skeleton) {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL_V(mesh, AABB());

	if (mesh->custom_aabb != AABB()) {
		return mesh->custom_aabb;
	}

	Skeleton *skeleton = skeleton_owner.get_or_null(p_skeleton);

	if (!skeleton || skeleton->size == 0 || mesh->skeleton_aabb_version == skeleton->version) {
		return mesh->aabb;
	}

	// Calculate AABB based on Skeleton

	AABB aabb;

	for (uint32_t i = 0; i < mesh->surface_count; i++) {
		AABB laabb;
		const Mesh::Surface &surface = *mesh->surfaces[i];
		if ((surface.format & RSE::ARRAY_FORMAT_BONES) && surface.bone_aabbs.size()) {
			int bs = surface.bone_aabbs.size();
			const AABB *skbones = surface.bone_aabbs.ptr();

			int sbs = skeleton->size;
			ERR_CONTINUE(bs > sbs);
			const float *baseptr = skeleton->data.ptr();

			bool found_bone_aabb = false;

			if (skeleton->use_2d) {
				for (int j = 0; j < bs; j++) {
					if (skbones[j].size == Vector3(-1, -1, -1)) {
						continue; //bone is unused
					}

					const float *dataptr = baseptr + j * 8;

					Transform3D mtx;

					mtx.basis.rows[0][0] = dataptr[0];
					mtx.basis.rows[0][1] = dataptr[1];
					mtx.origin.x = dataptr[3];

					mtx.basis.rows[1][0] = dataptr[4];
					mtx.basis.rows[1][1] = dataptr[5];
					mtx.origin.y = dataptr[7];

					// Transform bounds to skeleton's space before applying animation data.
					AABB baabb = surface.mesh_to_skeleton_xform.xform(skbones[j]);
					baabb = mtx.xform(baabb);

					if (!found_bone_aabb) {
						laabb = baabb;
						found_bone_aabb = true;
					} else {
						laabb.merge_with(baabb);
					}
				}
			} else {
				for (int j = 0; j < bs; j++) {
					if (skbones[j].size == Vector3(-1, -1, -1)) {
						continue; //bone is unused
					}

					const float *dataptr = baseptr + j * 12;

					Transform3D mtx;

					mtx.basis.rows[0][0] = dataptr[0];
					mtx.basis.rows[0][1] = dataptr[1];
					mtx.basis.rows[0][2] = dataptr[2];
					mtx.origin.x = dataptr[3];
					mtx.basis.rows[1][0] = dataptr[4];
					mtx.basis.rows[1][1] = dataptr[5];
					mtx.basis.rows[1][2] = dataptr[6];
					mtx.origin.y = dataptr[7];
					mtx.basis.rows[2][0] = dataptr[8];
					mtx.basis.rows[2][1] = dataptr[9];
					mtx.basis.rows[2][2] = dataptr[10];
					mtx.origin.z = dataptr[11];

					// Transform bounds to skeleton's space before applying animation data.
					AABB baabb = surface.mesh_to_skeleton_xform.xform(skbones[j]);
					baabb = mtx.xform(baabb);

					if (!found_bone_aabb) {
						laabb = baabb;
						found_bone_aabb = true;
					} else {
						laabb.merge_with(baabb);
					}
				}
			}

			if (found_bone_aabb) {
				// Transform skeleton bounds back to mesh's space if any animated AABB applied.
				laabb = surface.mesh_to_skeleton_xform.affine_inverse().xform(laabb);
			}

			if (laabb.size == Vector3()) {
				laabb = surface.aabb;
			}
		} else {
			laabb = surface.aabb;
		}

		if (i == 0) {
			aabb = laabb;
		} else {
			aabb.merge_with(laabb);
		}
	}

	mesh->aabb = aabb;
	mesh->skeleton_aabb_version = skeleton->version;
	return aabb;
}

void MeshStorage::mesh_set_path(RID p_mesh, const String &p_path) {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL(mesh);

	mesh->path = p_path;
}

String MeshStorage::mesh_get_path(RID p_mesh) const {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL_V(mesh, String());

	return mesh->path;
}

void MeshStorage::mesh_set_shadow_mesh(RID p_mesh, RID p_shadow_mesh) {
	ERR_FAIL_COND_MSG(p_mesh == p_shadow_mesh, "Cannot set a mesh as its own shadow mesh.");
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL(mesh);

	Mesh *shadow_mesh = mesh_owner.get_or_null(mesh->shadow_mesh);
	if (shadow_mesh) {
		shadow_mesh->shadow_owners.erase(mesh);
	}
	mesh->shadow_mesh = p_shadow_mesh;

	shadow_mesh = mesh_owner.get_or_null(mesh->shadow_mesh);

	if (shadow_mesh) {
		shadow_mesh->shadow_owners.insert(mesh);
	}

	mesh->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MESH);
}

void MeshStorage::mesh_clear(RID p_mesh) {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL(mesh);

	// Clear instance data before mesh data.
	for (MeshInstance *mi : mesh->instances) {
		_mesh_instance_clear(mi);
	}

	for (uint32_t i = 0; i < mesh->surface_count; i++) {
		_mesh_surface_clear(mesh, i);
	}
	if (mesh->surfaces) {
		memfree(mesh->surfaces);
	}

	mesh->surfaces = nullptr;
	mesh->surface_count = 0;
	mesh->material_cache.clear();
	mesh->has_bone_weights = false;
	mesh->aabb = AABB();
	mesh->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MESH);

	for (Mesh *E : mesh->shadow_owners) {
		Mesh *shadow_owner = E;
		shadow_owner->shadow_mesh = RID();
		shadow_owner->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MESH);
	}
}

void MeshStorage::_mesh_surface_generate_version_for_input_mask(Mesh::Surface::Version &v, Mesh::Surface *s, uint64_t p_input_mask, bool p_uses_motion_vectors, MeshInstance::Surface *mis, int p_current_vertex_buffer, int p_prev_vertex_buffer) {
	Mesh::Surface::Attrib attribs[RSE::ARRAY_MAX];

	int position_stride = 0; // Vertex position only.
	int normal_tangent_stride = 0;
	int attributes_stride = 0;
	int skin_stride = 0;

	for (int i = 0; i < RSE::ARRAY_INDEX; i++) {
		attribs[i].enabled = false;
		attribs[i].integer = false;
		if (!(s->format & (1ULL << i))) {
			continue;
		}

		if ((p_input_mask & (1ULL << i))) {
			// Only enable if it matches input mask.
			// Iterate over all anyway, so we can calculate stride.
			attribs[i].enabled = true;
		}

		switch (i) {
			case RSE::ARRAY_VERTEX: {
				attribs[i].offset = 0;
				attribs[i].type = GL_FLOAT;
				attribs[i].normalized = GL_FALSE;
				if (s->format & RSE::ARRAY_FLAG_USE_2D_VERTICES) {
					attribs[i].size = 2;
					position_stride = attribs[i].size * sizeof(float);
				} else {
					if (!mis && (s->format & RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES)) {
						attribs[i].size = 4;
						position_stride = attribs[i].size * sizeof(uint16_t);
						attribs[i].type = GL_UNSIGNED_SHORT;
						attribs[i].normalized = GL_TRUE;
					} else {
						attribs[i].size = 3;
						position_stride = attribs[i].size * sizeof(float);
					}
				}
			} break;
			case RSE::ARRAY_NORMAL: {
				if (!mis && (s->format & RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES)) {
					attribs[i].size = 2;
					normal_tangent_stride += 2 * attribs[i].size;
				} else {
					attribs[i].size = 4;
					// A small trick here: if we are uncompressed and we have normals, but no tangents. We need
					// the shader to think there are 4 components to "axis_tangent_attrib". So we give a size of 4,
					// but a stride based on only having 2 elements.
					if (!(s->format & RSE::ARRAY_FORMAT_TANGENT)) {
						normal_tangent_stride += (mis ? sizeof(float) : sizeof(uint16_t)) * 2;
					} else {
						normal_tangent_stride += (mis ? sizeof(float) : sizeof(uint16_t)) * 4;
					}
				}

				if (mis) {
					// Transform feedback has interleave all or no attributes. It can't mix interleaving.
					attribs[i].offset = position_stride;
					normal_tangent_stride += position_stride;
					position_stride = normal_tangent_stride;
				} else {
					attribs[i].offset = position_stride * s->vertex_count;
				}
				attribs[i].type = (mis ? GL_FLOAT : GL_UNSIGNED_SHORT);
				attribs[i].normalized = GL_TRUE;
			} break;
			case RSE::ARRAY_TANGENT: {
				// We never use the tangent attribute. It is always packed in ARRAY_NORMAL, or ARRAY_VERTEX.
				attribs[i].enabled = false;
				attribs[i].integer = false;
			} break;
			case RSE::ARRAY_COLOR: {
				attribs[i].offset = attributes_stride;
				attribs[i].size = 4;
				attribs[i].type = GL_UNSIGNED_BYTE;
				attributes_stride += 4;
				attribs[i].normalized = GL_TRUE;
			} break;
			case RSE::ARRAY_TEX_UV: {
				attribs[i].offset = attributes_stride;
				attribs[i].size = 2;
				if (s->format & RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES) {
					attribs[i].type = GL_UNSIGNED_SHORT;
					attributes_stride += 2 * sizeof(uint16_t);
					attribs[i].normalized = GL_TRUE;
				} else {
					attribs[i].type = GL_FLOAT;
					attributes_stride += 2 * sizeof(float);
					attribs[i].normalized = GL_FALSE;
				}
			} break;
			case RSE::ARRAY_TEX_UV2: {
				attribs[i].offset = attributes_stride;
				attribs[i].size = 2;
				if (s->format & RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES) {
					attribs[i].type = GL_UNSIGNED_SHORT;
					attributes_stride += 2 * sizeof(uint16_t);
					attribs[i].normalized = GL_TRUE;
				} else {
					attribs[i].type = GL_FLOAT;
					attributes_stride += 2 * sizeof(float);
					attribs[i].normalized = GL_FALSE;
				}
			} break;
			case RSE::ARRAY_CUSTOM0:
			case RSE::ARRAY_CUSTOM1:
			case RSE::ARRAY_CUSTOM2:
			case RSE::ARRAY_CUSTOM3: {
				attribs[i].offset = attributes_stride;

				int idx = i - RSE::ARRAY_CUSTOM0;
				uint32_t fmt_shift[RSE::ARRAY_CUSTOM_COUNT] = { RSE::ARRAY_FORMAT_CUSTOM0_SHIFT, RSE::ARRAY_FORMAT_CUSTOM1_SHIFT, RSE::ARRAY_FORMAT_CUSTOM2_SHIFT, RSE::ARRAY_FORMAT_CUSTOM3_SHIFT };
				uint32_t fmt = (s->format >> fmt_shift[idx]) & RSE::ARRAY_FORMAT_CUSTOM_MASK;
				uint32_t fmtsize[RSE::ARRAY_CUSTOM_MAX] = { 4, 4, 4, 8, 4, 8, 12, 16 };
				GLenum gl_type[RSE::ARRAY_CUSTOM_MAX] = { GL_UNSIGNED_BYTE, GL_BYTE, GL_HALF_FLOAT, GL_HALF_FLOAT, GL_FLOAT, GL_FLOAT, GL_FLOAT, GL_FLOAT };
				GLboolean norm[RSE::ARRAY_CUSTOM_MAX] = { GL_TRUE, GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE };
				attribs[i].type = gl_type[fmt];
				attributes_stride += fmtsize[fmt];
				attribs[i].size = fmtsize[fmt] / sizeof(float);
				attribs[i].normalized = norm[fmt];
			} break;
			case RSE::ARRAY_BONES: {
				attribs[i].offset = skin_stride;
				attribs[i].size = 4;
				attribs[i].type = GL_UNSIGNED_SHORT;
				skin_stride += 4 * sizeof(uint16_t);
				attribs[i].normalized = GL_FALSE;
				attribs[i].integer = true;
			} break;
			case RSE::ARRAY_WEIGHTS: {
				attribs[i].offset = skin_stride;
				attribs[i].size = 4;
				attribs[i].type = GL_UNSIGNED_SHORT;
				skin_stride += 4 * sizeof(uint16_t);
				attribs[i].normalized = GL_TRUE;
			} break;
		}
	}

	glGenVertexArrays(1, &v.vertex_array);
	glBindVertexArray(v.vertex_array);

	for (int i = 0; i < RSE::ARRAY_INDEX; i++) {
		if (!attribs[i].enabled) {
			glDisableVertexAttribArray(i);
			continue;
		}
		if (i <= RSE::ARRAY_TANGENT) {
			attribs[i].stride = (i == RSE::ARRAY_VERTEX) ? position_stride : normal_tangent_stride;
			if (mis) {
				glBindBuffer(GL_ARRAY_BUFFER, mis->vertex_buffers[p_current_vertex_buffer]);
			} else {
				glBindBuffer(GL_ARRAY_BUFFER, s->vertex_buffer);
			}
		} else if (i <= RSE::ARRAY_CUSTOM3) {
			attribs[i].stride = attributes_stride;
			glBindBuffer(GL_ARRAY_BUFFER, s->attribute_buffer);
		} else {
			attribs[i].stride = skin_stride;
			glBindBuffer(GL_ARRAY_BUFFER, s->skin_buffer);
		}

		if (attribs[i].integer) {
			glVertexAttribIPointer(i, attribs[i].size, attribs[i].type, attribs[i].stride, CAST_INT_TO_UCHAR_PTR(attribs[i].offset));
		} else {
			glVertexAttribPointer(i, attribs[i].size, attribs[i].type, attribs[i].normalized, attribs[i].stride, CAST_INT_TO_UCHAR_PTR(attribs[i].offset));
		}
		glEnableVertexAttribArray(i);
	}

	if (p_uses_motion_vectors) {
		for (int i = 0; i < RSE::ARRAY_TANGENT; i++) {
			if (mis) {
				glBindBuffer(GL_ARRAY_BUFFER, mis->vertex_buffers[mis->prev_vertex_buffer]);
			} else {
				glBindBuffer(GL_ARRAY_BUFFER, s->vertex_buffer);
			}

			glVertexAttribPointer(i + 16, attribs[i].size, attribs[i].type, attribs[i].normalized, attribs[i].stride, CAST_INT_TO_UCHAR_PTR(attribs[i].offset));
			glEnableVertexAttribArray(i + 16);
		}
	}

	// Do not bind index here as we want to switch between index buffers for LOD

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	v.input_mask = p_input_mask;
	v.uses_motion_vectors = p_uses_motion_vectors;
	v.current_vertex_buffer = p_current_vertex_buffer;
	v.prev_vertex_buffer = p_prev_vertex_buffer;
}

void MeshStorage::mesh_surface_remove(RID p_mesh, int p_surface) {
	Mesh *mesh = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL(mesh);
	ERR_FAIL_UNSIGNED_INDEX((uint32_t)p_surface, mesh->surface_count);

	// Clear instance data before mesh data.
	for (MeshInstance *mi : mesh->instances) {
		_mesh_instance_remove_surface(mi, p_surface);
	}

	_mesh_surface_clear(mesh, p_surface);

	if ((uint32_t)p_surface < mesh->surface_count - 1) {
		memmove(mesh->surfaces + p_surface, mesh->surfaces + p_surface + 1, sizeof(Mesh::Surface *) * (mesh->surface_count - (p_surface + 1)));
	}
	mesh->surfaces = (Mesh::Surface **)memrealloc(mesh->surfaces, sizeof(Mesh::Surface *) * (mesh->surface_count - 1));
	--mesh->surface_count;

	mesh->material_cache.clear();

	mesh->skeleton_aabb_version = 0;

	if (mesh->has_bone_weights) {
		mesh->has_bone_weights = false;
		for (uint32_t i = 0; i < mesh->surface_count; i++) {
			if (mesh->surfaces[i]->format & RSE::ARRAY_FORMAT_BONES) {
				mesh->has_bone_weights = true;
				break;
			}
		}
	}

	if (mesh->surface_count == 0) {
		mesh->aabb = AABB();
	} else {
		mesh->aabb = mesh->surfaces[0]->aabb;
		for (uint32_t i = 1; i < mesh->surface_count; i++) {
			mesh->aabb.merge_with(mesh->surfaces[i]->aabb);
		}
	}

	mesh->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MESH);

	for (Mesh *E : mesh->shadow_owners) {
		Mesh *shadow_owner = E;
		shadow_owner->shadow_mesh = RID();
		shadow_owner->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MESH);
	}
}

/* MESH INSTANCE API */

RID MeshStorage::mesh_instance_create(RID p_base) {
	Mesh *mesh = mesh_owner.get_or_null(p_base);
	ERR_FAIL_NULL_V(mesh, RID());

	RID rid = mesh_instance_owner.make_rid();
	MeshInstance *mi = mesh_instance_owner.get_or_null(rid);

	mi->mesh = mesh;

	for (uint32_t i = 0; i < mesh->surface_count; i++) {
		_mesh_instance_add_surface(mi, mesh, i);
	}

	mi->I = mesh->instances.push_back(mi);

	mi->dirty = true;

	return rid;
}

void MeshStorage::mesh_instance_free(RID p_rid) {
	MeshInstance *mi = mesh_instance_owner.get_or_null(p_rid);
	_mesh_instance_clear(mi);
	mi->mesh->instances.erase(mi->I);
	mi->I = nullptr;

	mesh_instance_owner.free(p_rid);
}

void MeshStorage::mesh_instance_set_skeleton(RID p_mesh_instance, RID p_skeleton) {
	MeshInstance *mi = mesh_instance_owner.get_or_null(p_mesh_instance);
	if (mi->skeleton == p_skeleton) {
		return;
	}
	mi->skeleton = p_skeleton;
	mi->skeleton_version = 0;
	mi->dirty = true;
}

void MeshStorage::mesh_instance_set_blend_shape_weight(RID p_mesh_instance, int p_shape, float p_weight) {
	MeshInstance *mi = mesh_instance_owner.get_or_null(p_mesh_instance);
	ERR_FAIL_NULL(mi);
	ERR_FAIL_INDEX(p_shape, (int)mi->blend_weights.size());
	mi->blend_weights[p_shape] = p_weight;
	mi->dirty = true;
}

void MeshStorage::_mesh_instance_clear(MeshInstance *mi) {
	while (mi->surfaces.size()) {
		_mesh_instance_remove_surface(mi, mi->surfaces.size() - 1);
	}
	mi->dirty = false;
}

void MeshStorage::_mesh_instance_add_surface(MeshInstance *mi, Mesh *mesh, uint32_t p_surface) {
	if (mesh->blend_shape_count > 0) {
		mi->blend_weights.resize(mesh->blend_shape_count);
		for (uint32_t i = 0; i < mi->blend_weights.size(); i++) {
			mi->blend_weights[i] = 0.0;
		}
	}

	MeshInstance::Surface s;
	if ((mesh->blend_shape_count > 0 || (mesh->surfaces[p_surface]->format & RSE::ARRAY_FORMAT_BONES)) && mesh->surfaces[p_surface]->vertex_buffer_size > 0) {
		// Cache surface properties
		s.format_cache = mesh->surfaces[p_surface]->format;
		if ((s.format_cache & (1ULL << RSE::ARRAY_VERTEX))) {
			if (s.format_cache & RSE::ARRAY_FLAG_USE_2D_VERTICES) {
				s.vertex_size_cache = 2;
			} else {
				s.vertex_size_cache = 3;
			}
			s.vertex_stride_cache = sizeof(float) * s.vertex_size_cache;
		}
		if ((s.format_cache & (1ULL << RSE::ARRAY_NORMAL))) {
			s.vertex_normal_offset_cache = s.vertex_stride_cache;
			s.vertex_stride_cache += sizeof(uint32_t) * 2;
		}
		if ((s.format_cache & (1ULL << RSE::ARRAY_TANGENT))) {
			s.vertex_tangent_offset_cache = s.vertex_stride_cache;
			s.vertex_stride_cache += sizeof(uint32_t) * 2;
		}

		int buffer_size = s.vertex_stride_cache * mesh->surfaces[p_surface]->vertex_count;

		// First buffer to be used for rendering. Final output of skeleton and blend shapes.
		// If motion vectors are enabled, a second buffer will be created on demand, and they'll be swapped every frame.
		glGenBuffers(1, &s.vertex_buffers[0]);
		glBindBuffer(GL_ARRAY_BUFFER, s.vertex_buffers[0]);
		GLES2::Utilities::get_singleton()->buffer_allocate_data(GL_ARRAY_BUFFER, s.vertex_buffers[0], buffer_size, nullptr, GL_DYNAMIC_DRAW, "MeshInstance vertex buffer");
		// No ping-pong blend-shape buffers: blending runs on the CPU.
		glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind
	}

	mi->surfaces.push_back(s);
	mi->dirty = true;
}

void MeshStorage::_mesh_instance_remove_surface(MeshInstance *mi, int p_surface) {
	MeshInstance::Surface &surface = mi->surfaces[p_surface];

	if (surface.version_count != 0) {
		for (uint32_t j = 0; j < surface.version_count; j++) {
			glDeleteVertexArrays(1, &surface.versions[j].vertex_array);
			surface.versions[j].vertex_array = 0;
		}
		memfree(surface.versions);
	}

	for (int i = 0; i < 2; i++) {
		if (surface.vertex_buffers[i] != 0) {
			GLES2::Utilities::get_singleton()->buffer_free_data(surface.vertex_buffers[i]);
			surface.vertex_buffers[i] = 0;
		}
	}

	mi->surfaces.remove_at(p_surface);

	if (mi->surfaces.is_empty()) {
		mi->blend_weights.clear();
		mi->weights_dirty = false;
		mi->skeleton_version = 0;
	}
	mi->dirty = true;
}

void MeshStorage::mesh_instance_check_for_update(RID p_mesh_instance) {
	MeshInstance *mi = mesh_instance_owner.get_or_null(p_mesh_instance);

	bool needs_update = mi->dirty;

	if (mi->array_update_list.in_list()) {
		return;
	}

	if (!needs_update && mi->skeleton.is_valid()) {
		Skeleton *sk = skeleton_owner.get_or_null(mi->skeleton);
		if (sk && sk->version != mi->skeleton_version) {
			needs_update = true;
		}
	}

	if (needs_update) {
		dirty_mesh_instance_arrays.add(&mi->array_update_list);
	}
}

void MeshStorage::mesh_instance_set_canvas_item_transform(RID p_mesh_instance, const Transform2D &p_transform) {
	MeshInstance *mi = mesh_instance_owner.get_or_null(p_mesh_instance);
	mi->canvas_item_transform_2d = p_transform;
}

// GLES2 simplification: equivalentes CPU das funcoes octaedricas de skeleton.glsl
// (oct_to_vec3, vec3_to_oct, oct_to_tang, tang_to_oct), para skinning em software.
static float _sw_unorm16_to_float(uint16_t p_v) {
	return float(p_v) * (1.0f / 65535.0f);
}

static Vector3 _sw_oct_to_vec3(const Vector2 &p_oct) {
	Vector2 oct = p_oct * 2.0f - Vector2(1.0f, 1.0f);
	float ox = oct.x;
	float oy = oct.y;
	float z = 1.0f - Math::abs(ox) - Math::abs(oy);
	float x = ox;
	float y = oy;
	if (z < 0.0f) {
		x = (1.0f - Math::abs(oy)) * (ox >= 0.0f ? 1.0f : -1.0f);
		y = (1.0f - Math::abs(ox)) * (oy >= 0.0f ? 1.0f : -1.0f);
	}
	return Vector3(x, y, z).normalized();
}

static Vector2 _sw_vec3_to_oct(const Vector3 &p_e) {
	float inv = 1.0f / MAX(Math::abs(p_e.x) + Math::abs(p_e.y) + Math::abs(p_e.z), 1e-7f);
	Vector3 e = p_e * inv;
	Vector2 oct;
	if (e.z >= 0.0f) {
		oct = Vector2(e.x, e.y);
	} else {
		float sx = e.x >= 0.0f ? 1.0f : -1.0f;
		float sy = e.y >= 0.0f ? 1.0f : -1.0f;
		oct = Vector2((1.0f - Math::abs(e.y)) * sx, (1.0f - Math::abs(e.x)) * sy);
	}
	return oct * 0.5f + Vector2(0.5f, 0.5f);
}

static Vector4 _sw_oct_to_tang(const Vector2 &p_o) {
	Vector2 oct(p_o.x, Math::abs(p_o.y) * 2.0f - 1.0f);
	Vector3 t = _sw_oct_to_vec3(oct);
	float w = p_o.y > 0.0f ? 1.0f : (p_o.y < 0.0f ? -1.0f : 0.0f);
	return Vector4(t.x, t.y, t.z, w);
}

static Vector2 _sw_tang_to_oct(const Vector4 &p_b) {
	Vector2 oct = _sw_vec3_to_oct(Vector3(p_b.x, p_b.y, p_b.z));
	oct.y = oct.y * 0.5f + 0.5f;
	if (p_b.w < 0.0f) {
		oct.y = 1.0f - oct.y;
	}
	return oct;
}

// GLES2 simplification: CPU skinning (blend shapes + 2D/3D skeletons),
// replicating the skeleton.glsl math without transform feedback.
// Returns false when the format requires the legacy TF path (compressed
// attributes or missing CPU copies). Output buffers use the same layout as the
// TF path (float positions + oct normals/tangents as floats), so rendering is unchanged.
bool MeshStorage::_mesh_instance_process_software(MeshInstance *p_mi, Skeleton *p_sk, uint32_t p_surface, float p_base_weight, bool p_can_use_skeleton, bool p_use_8_weights, bool p_array_is_2d) {
	Mesh::Surface *s = p_mi->mesh->surfaces[p_surface];
	MeshInstance::Surface &is = p_mi->surfaces[p_surface];

	if (s->format & RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES) {
		return false;
	}
	if (s->vertex_data_cpu.is_empty()) {
		return false;
	}
	if (p_can_use_skeleton && s->skin_data_cpu.is_empty()) {
		return false;
	}

	const uint32_t vertex_count = s->vertex_count;
	if (vertex_count == 0) {
		return true;
	}
	const bool has_normal = (s->format & (1ULL << RSE::ARRAY_NORMAL)) && !p_array_is_2d;
	const bool has_tangent = (s->format & (1ULL << RSE::ARRAY_TANGENT)) && !p_array_is_2d;
	const uint32_t blend_count = p_mi->mesh->blend_shape_count;
	if (blend_count > 0 && size_t(s->blend_shape_data_cpu.size()) < size_t(blend_count) * size_t(s->vertex_data_cpu.size())) {
		return false;
	}

	const uint32_t pos_size = p_array_is_2d ? 2 : 3;
	const uint32_t pos_bytes = pos_size * sizeof(float);
	const uint32_t nrm_block = pos_bytes * vertex_count;

	const uint8_t *base_ptr = s->vertex_data_cpu.ptr();
	const uint8_t *skin_ptr = s->skin_data_cpu.ptr();
	const uint8_t *blend_ptr = s->blend_shape_data_cpu.ptr();
	const int shape_size = s->vertex_data_cpu.size();

	Transform2D skeleton_xform_2d;
	if (p_can_use_skeleton && p_array_is_2d && p_sk != nullptr) {
		skeleton_xform_2d = p_mi->canvas_item_transform_2d.affine_inverse() * p_sk->base_transform_2d;
	}

	const float *sk_data = (p_can_use_skeleton && p_sk != nullptr) ? p_sk->data.ptr() : nullptr;
	const int bone_floats = p_array_is_2d ? 8 : 12;
	const int bone_sets = p_use_8_weights ? 2 : 1;
	const int skin_stride = int(sizeof(uint16_t)) * (p_use_8_weights ? 16 : 8);

	const int out_stride = is.vertex_stride_cache;
	const int out_size = out_stride * int(vertex_count);
	Vector<uint8_t> out;
	out.resize_initialized(out_size);
	uint8_t *out_ptr = out.ptrw();

	for (uint32_t v = 0; v < vertex_count; v++) {
		const float *bp = (const float *)(base_ptr + v * pos_bytes);
		Vector3 pos(bp[0], pos_size > 1 ? bp[1] : 0.0f, pos_size > 2 ? bp[2] : 0.0f);
		Vector3 nrm(0.0f, 0.0f, 1.0f);
		Vector4 tan(0.0f, 0.0f, 1.0f, 1.0f);
		if (has_normal) {
			const uint16_t *np = (const uint16_t *)(base_ptr + nrm_block + v * sizeof(uint16_t) * 2);
			nrm = _sw_oct_to_vec3(Vector2(_sw_unorm16_to_float(np[0]), _sw_unorm16_to_float(np[1])));
		}
		if (has_tangent) {
			const uint32_t tan_block = nrm_block + (has_normal ? sizeof(uint16_t) * 2 * vertex_count : 0);
			const uint16_t *tp = (const uint16_t *)(base_ptr + tan_block + v * sizeof(uint16_t) * 2);
			tan = _sw_oct_to_tang(Vector2(_sw_unorm16_to_float(tp[0]), _sw_unorm16_to_float(tp[1])));
		}

		if (blend_count > 0) {
			pos *= p_base_weight;
			if (has_normal) {
				nrm *= p_base_weight;
			}
			if (has_tangent) {
				tan = Vector4(tan.x * p_base_weight, tan.y * p_base_weight, tan.z * p_base_weight, tan.w);
			}
			for (uint32_t bs = 0; bs < blend_count; bs++) {
				float w = p_mi->blend_weights[bs];
				if (Math::is_zero_approx(w)) {
					continue;
				}
				const float *spp = (const float *)(blend_ptr + bs * shape_size + v * pos_bytes);
				pos += Vector3(spp[0], pos_size > 1 ? spp[1] : 0.0f, pos_size > 2 ? spp[2] : 0.0f) * w;
				if (has_normal) {
					const uint16_t *snp = (const uint16_t *)(blend_ptr + bs * shape_size + nrm_block + v * sizeof(uint16_t) * 2);
					nrm += _sw_oct_to_vec3(Vector2(_sw_unorm16_to_float(snp[0]), _sw_unorm16_to_float(snp[1]))) * w;
				}
				if (has_tangent) {
					const uint32_t tan_block = nrm_block + (has_normal ? sizeof(uint16_t) * 2 * vertex_count : 0);
					const uint16_t *stp = (const uint16_t *)(blend_ptr + bs * shape_size + tan_block + v * sizeof(uint16_t) * 2);
					Vector4 tb = _sw_oct_to_tang(Vector2(_sw_unorm16_to_float(stp[0]), _sw_unorm16_to_float(stp[1])));
					tan = Vector4(tan.x + tb.x * w, tan.y + tb.y * w, tan.z + tb.z * w, tan.w);
				}
			}
			if (p_array_is_2d) {
				// Replicates the 2D+blend FINAL_PASS normalize() from skeleton.glsl.
				pos = pos.normalized();
			}
		}

		if (p_can_use_skeleton && sk_data != nullptr) {
			if (p_array_is_2d) {
				Vector2 acc(0.0f, 0.0f);
				Vector2 pv(pos.x, pos.y);
				for (int set = 0; set < bone_sets; set++) {
					const uint16_t *bu = (const uint16_t *)(skin_ptr + v * skin_stride + set * sizeof(uint16_t) * 8);
					const uint16_t *wu = bu + 4;
					for (int k = 0; k < 4; k++) {
						uint32_t bi = bu[k];
						float w = _sw_unorm16_to_float(wu[k]);
						if (w == 0.0f || bi >= uint32_t(p_sk->size)) {
							continue;
						}
						const float *dp = sk_data + bi * bone_floats;
						Transform2D b;
						b.columns[0][0] = dp[0];
						b.columns[1][0] = dp[1];
						b.columns[2][0] = dp[3];
						b.columns[0][1] = dp[4];
						b.columns[1][1] = dp[5];
						b.columns[2][1] = dp[7];
						acc += b.xform(pv) * w;
					}
				}
				Vector2 skinned = skeleton_xform_2d.xform(acc);
				pos = Vector3(skinned.x, skinned.y, 0.0f);
			} else {
				Vector3 acc_p(0.0f, 0.0f, 0.0f);
				Vector3 acc_n(0.0f, 0.0f, 0.0f);
				Vector3 acc_t(0.0f, 0.0f, 0.0f);
				for (int set = 0; set < bone_sets; set++) {
					const uint16_t *bu = (const uint16_t *)(skin_ptr + v * skin_stride + set * sizeof(uint16_t) * 8);
					const uint16_t *wu = bu + 4;
					for (int k = 0; k < 4; k++) {
						uint32_t bi = bu[k];
						float w = _sw_unorm16_to_float(wu[k]);
						if (w == 0.0f || bi >= uint32_t(p_sk->size)) {
							continue;
						}
						const float *dp = sk_data + bi * bone_floats;
						Transform3D b;
						b.basis.rows[0][0] = dp[0];
						b.basis.rows[0][1] = dp[1];
						b.basis.rows[0][2] = dp[2];
						b.origin.x = dp[3];
						b.basis.rows[1][0] = dp[4];
						b.basis.rows[1][1] = dp[5];
						b.basis.rows[1][2] = dp[6];
						b.origin.y = dp[7];
						b.basis.rows[2][0] = dp[8];
						b.basis.rows[2][1] = dp[9];
						b.basis.rows[2][2] = dp[10];
						b.origin.z = dp[11];
						acc_p += b.xform(pos) * w;
						if (has_normal) {
							acc_n += b.basis.xform(nrm) * w;
						}
						if (has_tangent) {
							acc_t += b.basis.xform(Vector3(tan.x, tan.y, tan.z)) * w;
						}
					}
				}
				pos = acc_p;
				if (has_normal) {
					nrm = acc_n.normalized();
				}
				if (has_tangent) {
					Vector3 tn = acc_t.normalized();
					tan = Vector4(tn.x, tn.y, tn.z, tan.w);
				}
			}
		} else if (blend_count > 0) {
			if (has_normal) {
				nrm = nrm.normalized();
			}
			if (has_tangent) {
				Vector3 tn = Vector3(tan.x, tan.y, tan.z).normalized();
				tan = Vector4(tn.x, tn.y, tn.z, tan.w);
			}
		}

		float *op = (float *)(out_ptr + v * out_stride);
		op[0] = pos.x;
		op[1] = pos.y;
		if (pos_size > 2) {
			op[2] = pos.z;
		}
		if (has_normal) {
			Vector2 o = _sw_vec3_to_oct(nrm);
			float *np2 = (float *)(out_ptr + v * out_stride + is.vertex_normal_offset_cache);
			np2[0] = o.x;
			np2[1] = o.y;
		}
		if (has_tangent) {
			Vector2 o = _sw_tang_to_oct(tan);
			float *tp2 = (float *)(out_ptr + v * out_stride + is.vertex_tangent_offset_cache);
			tp2[0] = o.x;
			tp2[1] = o.y;
		}
	}

	glBindBuffer(GL_ARRAY_BUFFER, is.vertex_buffers[is.current_vertex_buffer]);
	GLES2::Utilities::get_singleton()->buffer_allocate_data(GL_ARRAY_BUFFER, is.vertex_buffers[is.current_vertex_buffer], out_size, out.ptr(), GL_DYNAMIC_DRAW, "MeshInstance vertex buffer (software)");
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	return true;
}

void MeshStorage::update_mesh_instances() {
	if (dirty_mesh_instance_arrays.first() == nullptr) {
		return; //nothing to do
	}

	// Blend shapes and skeletons are processed on the CPU; no GL state needed.
	while (dirty_mesh_instance_arrays.first()) {
		MeshInstance *mi = dirty_mesh_instance_arrays.first()->self();

		bool uses_motion_vectors = RSG::viewport->get_num_viewports_with_motion_vectors() > 0;
		int frame = RSG::rasterizer->get_frame_number();
		if (uses_motion_vectors) {
			for (uint32_t i = 0; i < mi->surfaces.size(); i++) {
				mi->surfaces[i].prev_vertex_buffer = mi->surfaces[i].current_vertex_buffer;

				if (frame - mi->surfaces[i].last_change == 1) {
					// Previous buffer's data can only be one frame old to be able to use motion vectors.
					uint32_t new_buffer_index = mi->surfaces[i].current_vertex_buffer ^ 1;

					if (mi->surfaces[i].vertex_buffers[new_buffer_index] == 0) {
						// Create the new vertex buffer on demand where the result for the current frame will be stored.
						GLuint new_vertex_buffer = 0;
						GLES2::Mesh::Surface *surface = mi->mesh->surfaces[i];
						int buffer_size = mi->surfaces[i].vertex_stride_cache * surface->vertex_count;
						glGenBuffers(1, &new_vertex_buffer);
						glBindBuffer(GL_ARRAY_BUFFER, new_vertex_buffer);
						GLES2::Utilities::get_singleton()->buffer_allocate_data(GL_ARRAY_BUFFER, new_vertex_buffer, buffer_size, nullptr, (surface->format & RSE::ARRAY_FLAG_USE_DYNAMIC_UPDATE) ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW, "Secondary mesh vertex buffer");
						glBindBuffer(GL_ARRAY_BUFFER, 0);

						mi->surfaces[i].vertex_buffers[new_buffer_index] = new_vertex_buffer;
					}

					mi->surfaces[i].current_vertex_buffer = new_buffer_index;
				}

				mi->surfaces[i].last_change = frame;
			}
		}

		Skeleton *sk = skeleton_owner.get_or_null(mi->skeleton);

		// Precompute base weight if using blend shapes.
		float base_weight = 1.0;
		if (mi->surfaces.size() && mi->mesh->blend_shape_count && mi->mesh->blend_shape_mode == RSE::BLEND_SHAPE_MODE_NORMALIZED) {
			for (uint32_t i = 0; i < mi->mesh->blend_shape_count; i++) {
				base_weight -= mi->blend_weights[i];
			}
		}

		for (uint32_t i = 0; i < mi->surfaces.size(); i++) {
			if (mi->surfaces[i].vertex_buffers[mi->surfaces[i].current_vertex_buffer] == 0) {
				continue;
			}

			bool array_is_2d = mi->surfaces[i].format_cache & RSE::ARRAY_FLAG_USE_2D_VERTICES;
			bool can_use_skeleton = sk != nullptr && sk->use_2d == array_is_2d && (mi->surfaces[i].format_cache & RSE::ARRAY_FORMAT_BONES);
			bool use_8_weights = mi->surfaces[i].format_cache & RSE::ARRAY_FLAG_USE_8_BONE_WEIGHTS;

			// GLES2 simplification: CPU skinning only; transform feedback is gone.
			// Surfaces that software processing rejects (e.g. compressed attributes
			// with skinning/blend shapes) are left unprocessed and warned about once.
			if ((mi->mesh->blend_shape_count > 0 || can_use_skeleton) && !_mesh_instance_process_software(mi, sk, i, base_weight, can_use_skeleton, use_8_weights, array_is_2d)) {
				WARN_PRINT_ONCE_ED("GLES2 driver requires uncompressed vertices for skinned/blend-shape meshes.");
			}
		}
		mi->dirty = false;
		if (sk) {
			mi->skeleton_version = sk->version;
		}
		dirty_mesh_instance_arrays.remove(&mi->array_update_list);
	}
}

/* MULTIMESH API */

RID MeshStorage::_multimesh_allocate() {
	return multimesh_owner.allocate_rid();
}

void MeshStorage::_multimesh_initialize(RID p_rid) {
	multimesh_owner.initialize_rid(p_rid, MultiMesh());
}

void MeshStorage::_multimesh_free(RID p_rid) {
	// Remove from interpolator.
	_interpolation_data.notify_free_multimesh(p_rid);
	_update_dirty_multimeshes();
	multimesh_allocate_data(p_rid, 0, RSE::MULTIMESH_TRANSFORM_2D);
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_rid);
	multimesh->dependency.deleted_notify(p_rid);
	multimesh_owner.free(p_rid);
}

void MeshStorage::_multimesh_allocate_data(RID p_multimesh, int p_instances, RSE::MultimeshTransformFormat p_transform_format, bool p_use_colors, bool p_use_custom_data, bool p_use_indirect) {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL(multimesh);

	if (multimesh->instances == p_instances && multimesh->xform_format == p_transform_format && multimesh->uses_colors == p_use_colors && multimesh->uses_custom_data == p_use_custom_data) {
		return;
	}

	for (int i = 0; i < 2; i++) {
		if (multimesh->buffer[i] != 0) {
			GLES2::Utilities::get_singleton()->buffer_free_data(multimesh->buffer[i]);
			multimesh->buffer[i] = 0;
		}
	}

	if (multimesh->data_cache_dirty_regions) {
		memdelete_arr(multimesh->data_cache_dirty_regions);
		multimesh->data_cache_dirty_regions = nullptr;
		multimesh->data_cache_used_dirty_regions = 0;
	}

	// If we have either color or custom data, reserve space for both to make data handling logic simpler.
	// This way we can always treat them both as a single, compressed uvec4.
	int color_and_custom_strides = (p_use_colors || p_use_custom_data) ? 2 : 0;

	multimesh->instances = p_instances;
	multimesh->xform_format = p_transform_format;
	multimesh->uses_colors = p_use_colors;
	multimesh->color_offset_cache = p_transform_format == RSE::MULTIMESH_TRANSFORM_2D ? 8 : 12;
	multimesh->uses_custom_data = p_use_custom_data;
	multimesh->custom_data_offset_cache = multimesh->color_offset_cache + color_and_custom_strides;
	multimesh->stride_cache = multimesh->custom_data_offset_cache + color_and_custom_strides;
	multimesh->buffer_set = false;

	multimesh->data_cache = Vector<float>();
	multimesh->aabb = AABB();
	multimesh->aabb_dirty = false;
	multimesh->visible_instances = MIN(multimesh->visible_instances, multimesh->instances);

	if (multimesh->instances) {
		glGenBuffers(1, &multimesh->buffer[0]);
		glBindBuffer(GL_ARRAY_BUFFER, multimesh->buffer[0]);
		GLES2::Utilities::get_singleton()->buffer_allocate_data(GL_ARRAY_BUFFER, multimesh->buffer[0], multimesh->instances * multimesh->stride_cache * sizeof(float), nullptr, GL_STATIC_DRAW, "MultiMesh buffer");
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	multimesh->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MULTIMESH);
}

int MeshStorage::_multimesh_get_instance_count(RID p_multimesh) const {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL_V(multimesh, 0);
	return multimesh->instances;
}

void MeshStorage::_multimesh_set_mesh(RID p_multimesh, RID p_mesh) {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL(multimesh);
	if (multimesh->mesh == p_mesh || p_mesh.is_null()) {
		return;
	}
	multimesh->mesh = p_mesh;

	if (multimesh->instances == 0) {
		return;
	}

	if (multimesh->data_cache.size()) {
		//we have a data cache, just mark it dirty
		_multimesh_mark_all_dirty(multimesh, false, true);
	} else if (multimesh->instances) {
		// Need to re-create AABB. Unfortunately, calling this has a penalty.
		if (multimesh->buffer_set) {
			Vector<uint8_t> buffer = Utilities::buffer_get_data(GL_ARRAY_BUFFER, multimesh->buffer[multimesh->current_buffer], multimesh->instances * multimesh->stride_cache * sizeof(float));
			const uint8_t *r = buffer.ptr();
			const float *data = (const float *)r;
			_multimesh_re_create_aabb(multimesh, data, multimesh->instances);
		}
	}

	multimesh->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MESH);
}

#define MULTIMESH_DIRTY_REGION_SIZE 512

void MeshStorage::_multimesh_make_local(MultiMesh *multimesh) const {
	if (multimesh->data_cache.size() > 0 || multimesh->instances == 0) {
		return; //already local
	}
	ERR_FAIL_COND(multimesh->data_cache.size() > 0);
	// this means that the user wants to load/save individual elements,
	// for this, the data must reside on CPU, so just copy it there.
	multimesh->data_cache.resize(multimesh->instances * multimesh->stride_cache);
	{
		float *w = multimesh->data_cache.ptrw();

		if (multimesh->buffer_set) {
			Vector<uint8_t> buffer = Utilities::buffer_get_data(GL_ARRAY_BUFFER, multimesh->buffer[multimesh->current_buffer], multimesh->instances * multimesh->stride_cache * sizeof(float));

			{
				const uint8_t *r = buffer.ptr();
				memcpy(w, r, buffer.size());
			}
		} else {
			memset(w, 0, (size_t)multimesh->instances * multimesh->stride_cache * sizeof(float));
		}
	}
	uint32_t data_cache_dirty_region_count = Math::division_round_up(multimesh->instances, MULTIMESH_DIRTY_REGION_SIZE);
	multimesh->data_cache_dirty_regions = memnew_arr(bool, data_cache_dirty_region_count);
	for (uint32_t i = 0; i < data_cache_dirty_region_count; i++) {
		multimesh->data_cache_dirty_regions[i] = false;
	}
	multimesh->data_cache_used_dirty_regions = 0;
}

void MeshStorage::_multimesh_mark_dirty(MultiMesh *multimesh, int p_index, bool p_aabb) {
	uint32_t region_index = p_index / MULTIMESH_DIRTY_REGION_SIZE;
#ifdef DEBUG_ENABLED
	uint32_t data_cache_dirty_region_count = Math::division_round_up(multimesh->instances, MULTIMESH_DIRTY_REGION_SIZE);
	ERR_FAIL_UNSIGNED_INDEX(region_index, data_cache_dirty_region_count); //bug
#endif
	if (!multimesh->data_cache_dirty_regions[region_index]) {
		multimesh->data_cache_dirty_regions[region_index] = true;
		multimesh->data_cache_used_dirty_regions++;
	}

	if (p_aabb) {
		multimesh->aabb_dirty = true;
	}

	if (!multimesh->dirty) {
		multimesh->dirty_list = multimesh_dirty_list;
		multimesh_dirty_list = multimesh;
		multimesh->dirty = true;
	}
}

void MeshStorage::_multimesh_mark_all_dirty(MultiMesh *multimesh, bool p_data, bool p_aabb) {
	if (p_data) {
		uint32_t data_cache_dirty_region_count = Math::division_round_up(multimesh->instances, MULTIMESH_DIRTY_REGION_SIZE);

		for (uint32_t i = 0; i < data_cache_dirty_region_count; i++) {
			if (!multimesh->data_cache_dirty_regions[i]) {
				multimesh->data_cache_dirty_regions[i] = true;
				multimesh->data_cache_used_dirty_regions++;
			}
		}
	}

	if (p_aabb) {
		multimesh->aabb_dirty = true;
	}

	if (!multimesh->dirty) {
		multimesh->dirty_list = multimesh_dirty_list;
		multimesh_dirty_list = multimesh;
		multimesh->dirty = true;
	}
}

void MeshStorage::_multimesh_re_create_aabb(MultiMesh *multimesh, const float *p_data, int p_instances) {
	ERR_FAIL_COND(multimesh->mesh.is_null());
	if (multimesh->custom_aabb != AABB()) {
		return;
	}
	AABB aabb;
	AABB mesh_aabb = mesh_get_aabb(multimesh->mesh);
	for (int i = 0; i < p_instances; i++) {
		const float *data = p_data + multimesh->stride_cache * i;
		Transform3D t;

		if (multimesh->xform_format == RSE::MULTIMESH_TRANSFORM_3D) {
			t.basis.rows[0][0] = data[0];
			t.basis.rows[0][1] = data[1];
			t.basis.rows[0][2] = data[2];
			t.origin.x = data[3];
			t.basis.rows[1][0] = data[4];
			t.basis.rows[1][1] = data[5];
			t.basis.rows[1][2] = data[6];
			t.origin.y = data[7];
			t.basis.rows[2][0] = data[8];
			t.basis.rows[2][1] = data[9];
			t.basis.rows[2][2] = data[10];
			t.origin.z = data[11];

		} else {
			t.basis.rows[0][0] = data[0];
			t.basis.rows[0][1] = data[1];
			t.origin.x = data[3];

			t.basis.rows[1][0] = data[4];
			t.basis.rows[1][1] = data[5];
			t.origin.y = data[7];
		}

		if (i == 0) {
			aabb = t.xform(mesh_aabb);
		} else {
			aabb.merge_with(t.xform(mesh_aabb));
		}
	}

	multimesh->aabb = aabb;
}

void MeshStorage::_multimesh_instance_set_transform(RID p_multimesh, int p_index, const Transform3D &p_transform) {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL(multimesh);
	ERR_FAIL_INDEX(p_index, multimesh->instances);
	ERR_FAIL_COND(multimesh->xform_format != RSE::MULTIMESH_TRANSFORM_3D);

	_multimesh_make_local(multimesh);

	{
		float *w = multimesh->data_cache.ptrw();

		float *dataptr = w + p_index * multimesh->stride_cache;

		dataptr[0] = p_transform.basis.rows[0][0];
		dataptr[1] = p_transform.basis.rows[0][1];
		dataptr[2] = p_transform.basis.rows[0][2];
		dataptr[3] = p_transform.origin.x;
		dataptr[4] = p_transform.basis.rows[1][0];
		dataptr[5] = p_transform.basis.rows[1][1];
		dataptr[6] = p_transform.basis.rows[1][2];
		dataptr[7] = p_transform.origin.y;
		dataptr[8] = p_transform.basis.rows[2][0];
		dataptr[9] = p_transform.basis.rows[2][1];
		dataptr[10] = p_transform.basis.rows[2][2];
		dataptr[11] = p_transform.origin.z;
	}

	_multimesh_mark_dirty(multimesh, p_index, true);
}

void MeshStorage::_multimesh_instance_set_transform_2d(RID p_multimesh, int p_index, const Transform2D &p_transform) {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL(multimesh);
	ERR_FAIL_INDEX(p_index, multimesh->instances);
	ERR_FAIL_COND(multimesh->xform_format != RSE::MULTIMESH_TRANSFORM_2D);

	_multimesh_make_local(multimesh);

	{
		float *w = multimesh->data_cache.ptrw();

		float *dataptr = w + p_index * multimesh->stride_cache;

		dataptr[0] = p_transform.columns[0][0];
		dataptr[1] = p_transform.columns[1][0];
		dataptr[2] = 0;
		dataptr[3] = p_transform.columns[2][0];
		dataptr[4] = p_transform.columns[0][1];
		dataptr[5] = p_transform.columns[1][1];
		dataptr[6] = 0;
		dataptr[7] = p_transform.columns[2][1];
	}

	_multimesh_mark_dirty(multimesh, p_index, true);
}

void MeshStorage::_multimesh_instance_set_color(RID p_multimesh, int p_index, const Color &p_color) {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL(multimesh);
	ERR_FAIL_INDEX(p_index, multimesh->instances);
	ERR_FAIL_COND(!multimesh->uses_colors);

	_multimesh_make_local(multimesh);

	{
		// Colors are packed into 2 floats.
		float *w = multimesh->data_cache.ptrw();

		float *dataptr = w + p_index * multimesh->stride_cache + multimesh->color_offset_cache;
		uint16_t val[4] = { Math::make_half_float(p_color.r), Math::make_half_float(p_color.g), Math::make_half_float(p_color.b), Math::make_half_float(p_color.a) };
		memcpy(dataptr, val, 2 * 4);
	}

	_multimesh_mark_dirty(multimesh, p_index, false);
}

void MeshStorage::_multimesh_instance_set_custom_data(RID p_multimesh, int p_index, const Color &p_color) {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL(multimesh);
	ERR_FAIL_INDEX(p_index, multimesh->instances);
	ERR_FAIL_COND(!multimesh->uses_custom_data);

	_multimesh_make_local(multimesh);

	{
		float *w = multimesh->data_cache.ptrw();

		float *dataptr = w + p_index * multimesh->stride_cache + multimesh->custom_data_offset_cache;
		uint16_t val[4] = { Math::make_half_float(p_color.r), Math::make_half_float(p_color.g), Math::make_half_float(p_color.b), Math::make_half_float(p_color.a) };
		memcpy(dataptr, val, 2 * 4);
	}

	_multimesh_mark_dirty(multimesh, p_index, false);
}

RID MeshStorage::_multimesh_get_mesh(RID p_multimesh) const {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL_V(multimesh, RID());

	return multimesh->mesh;
}

void MeshStorage::_multimesh_set_custom_aabb(RID p_multimesh, const AABB &p_aabb) {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL(multimesh);
	multimesh->custom_aabb = p_aabb;
	multimesh->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
}

AABB MeshStorage::_multimesh_get_custom_aabb(RID p_multimesh) const {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL_V(multimesh, AABB());
	return multimesh->custom_aabb;
}

AABB MeshStorage::_multimesh_get_aabb(RID p_multimesh) {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL_V(multimesh, AABB());
	if (multimesh->custom_aabb != AABB()) {
		return multimesh->custom_aabb;
	}
	if (multimesh->aabb_dirty) {
		_update_dirty_multimeshes();
	}
	return multimesh->aabb;
}

Transform3D MeshStorage::_multimesh_instance_get_transform(RID p_multimesh, int p_index) const {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL_V(multimesh, Transform3D());
	ERR_FAIL_INDEX_V(p_index, multimesh->instances, Transform3D());
	ERR_FAIL_COND_V(multimesh->xform_format != RSE::MULTIMESH_TRANSFORM_3D, Transform3D());

	_multimesh_make_local(multimesh);

	Transform3D t;
	{
		const float *r = multimesh->data_cache.ptr();

		const float *dataptr = r + p_index * multimesh->stride_cache;

		t.basis.rows[0][0] = dataptr[0];
		t.basis.rows[0][1] = dataptr[1];
		t.basis.rows[0][2] = dataptr[2];
		t.origin.x = dataptr[3];
		t.basis.rows[1][0] = dataptr[4];
		t.basis.rows[1][1] = dataptr[5];
		t.basis.rows[1][2] = dataptr[6];
		t.origin.y = dataptr[7];
		t.basis.rows[2][0] = dataptr[8];
		t.basis.rows[2][1] = dataptr[9];
		t.basis.rows[2][2] = dataptr[10];
		t.origin.z = dataptr[11];
	}

	return t;
}

Transform2D MeshStorage::_multimesh_instance_get_transform_2d(RID p_multimesh, int p_index) const {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL_V(multimesh, Transform2D());
	ERR_FAIL_INDEX_V(p_index, multimesh->instances, Transform2D());
	ERR_FAIL_COND_V(multimesh->xform_format != RSE::MULTIMESH_TRANSFORM_2D, Transform2D());

	_multimesh_make_local(multimesh);

	Transform2D t;
	{
		const float *r = multimesh->data_cache.ptr();

		const float *dataptr = r + p_index * multimesh->stride_cache;

		t.columns[0][0] = dataptr[0];
		t.columns[1][0] = dataptr[1];
		t.columns[2][0] = dataptr[3];
		t.columns[0][1] = dataptr[4];
		t.columns[1][1] = dataptr[5];
		t.columns[2][1] = dataptr[7];
	}

	return t;
}

Color MeshStorage::_multimesh_instance_get_color(RID p_multimesh, int p_index) const {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL_V(multimesh, Color());
	ERR_FAIL_INDEX_V(p_index, multimesh->instances, Color());
	ERR_FAIL_COND_V(!multimesh->uses_colors, Color());

	_multimesh_make_local(multimesh);

	Color c;
	{
		const float *r = multimesh->data_cache.ptr();

		const float *dataptr = r + p_index * multimesh->stride_cache + multimesh->color_offset_cache;
		uint16_t raw_data[4];
		memcpy(raw_data, dataptr, 2 * 4);
		c.r = Math::half_to_float(raw_data[0]);
		c.g = Math::half_to_float(raw_data[1]);
		c.b = Math::half_to_float(raw_data[2]);
		c.a = Math::half_to_float(raw_data[3]);
	}

	return c;
}

Color MeshStorage::_multimesh_instance_get_custom_data(RID p_multimesh, int p_index) const {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL_V(multimesh, Color());
	ERR_FAIL_INDEX_V(p_index, multimesh->instances, Color());
	ERR_FAIL_COND_V(!multimesh->uses_custom_data, Color());

	_multimesh_make_local(multimesh);

	Color c;
	{
		const float *r = multimesh->data_cache.ptr();

		const float *dataptr = r + p_index * multimesh->stride_cache + multimesh->custom_data_offset_cache;
		uint16_t raw_data[4];
		memcpy(raw_data, dataptr, 2 * 4);
		c.r = Math::half_to_float(raw_data[0]);
		c.g = Math::half_to_float(raw_data[1]);
		c.b = Math::half_to_float(raw_data[2]);
		c.a = Math::half_to_float(raw_data[3]);
	}

	return c;
}

void MeshStorage::_multimesh_set_buffer(RID p_multimesh, const Vector<float> &p_buffer) {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL(multimesh);

	// Assign data to previous buffer if motion vectors are used, that data will be made current in _update_dirty_multimeshes().
	bool uses_motion_vectors = RSG::viewport->get_num_viewports_with_motion_vectors() > 0;
	int buffer_index = uses_motion_vectors ? multimesh->prev_buffer : multimesh->current_buffer;

	if (multimesh->uses_colors || multimesh->uses_custom_data) {
		// Color and custom need to be packed so copy buffer to data_cache and pack.

		_multimesh_make_local(multimesh);

		uint32_t old_stride = multimesh->xform_format == RSE::MULTIMESH_TRANSFORM_2D ? 8 : 12;
		old_stride += multimesh->uses_colors ? 4 : 0;
		old_stride += multimesh->uses_custom_data ? 4 : 0;
		ERR_FAIL_COND(p_buffer.size() != (multimesh->instances * (int)old_stride));

		multimesh->data_cache = p_buffer;

		float *w = multimesh->data_cache.ptrw();

		for (int i = 0; i < multimesh->instances; i++) {
			{
				float *dataptr = w + i * old_stride;
				float *newptr = w + i * multimesh->stride_cache;
				float vals[8] = { dataptr[0], dataptr[1], dataptr[2], dataptr[3], dataptr[4], dataptr[5], dataptr[6], dataptr[7] };
				memcpy(newptr, vals, 8 * 4);
			}

			if (multimesh->xform_format == RSE::MULTIMESH_TRANSFORM_3D) {
				float *dataptr = w + i * old_stride + 8;
				float *newptr = w + i * multimesh->stride_cache + 8;
				float vals[8] = { dataptr[0], dataptr[1], dataptr[2], dataptr[3] };
				memcpy(newptr, vals, 4 * 4);
			}

			if (multimesh->uses_colors) {
				float *dataptr = w + i * old_stride + (multimesh->xform_format == RSE::MULTIMESH_TRANSFORM_2D ? 8 : 12);
				float *newptr = w + i * multimesh->stride_cache + multimesh->color_offset_cache;
				uint16_t val[4] = { Math::make_half_float(dataptr[0]), Math::make_half_float(dataptr[1]), Math::make_half_float(dataptr[2]), Math::make_half_float(dataptr[3]) };
				memcpy(newptr, val, 2 * 4);
			}
			if (multimesh->uses_custom_data) {
				float *dataptr = w + i * old_stride + (multimesh->xform_format == RSE::MULTIMESH_TRANSFORM_2D ? 8 : 12) + (multimesh->uses_colors ? 4 : 0);
				float *newptr = w + i * multimesh->stride_cache + multimesh->custom_data_offset_cache;
				uint16_t val[4] = { Math::make_half_float(dataptr[0]), Math::make_half_float(dataptr[1]), Math::make_half_float(dataptr[2]), Math::make_half_float(dataptr[3]) };
				memcpy(newptr, val, 2 * 4);
			}
		}

		multimesh->data_cache.resize(multimesh->instances * (int)multimesh->stride_cache);
		const float *r = multimesh->data_cache.ptr();
		glBindBuffer(GL_ARRAY_BUFFER, multimesh->buffer[buffer_index]);
		glBufferData(GL_ARRAY_BUFFER, multimesh->data_cache.size() * sizeof(float), r, GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

	} else {
		// If we have a data cache, just update it.
		if (multimesh->data_cache.size()) {
			multimesh->data_cache = p_buffer;
		}

		// Only Transform is being used, so we can upload directly.
		ERR_FAIL_COND(p_buffer.size() != (multimesh->instances * (int)multimesh->stride_cache));
		const float *r = p_buffer.ptr();
		glBindBuffer(GL_ARRAY_BUFFER, multimesh->buffer[buffer_index]);
		glBufferData(GL_ARRAY_BUFFER, p_buffer.size() * sizeof(float), r, GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	multimesh->buffer_set = true;

	if (multimesh->data_cache.size() || multimesh->uses_colors || multimesh->uses_custom_data) {
		// Clear dirty since nothing will be dirty anymore.
		uint32_t data_cache_dirty_region_count = Math::division_round_up(multimesh->instances, MULTIMESH_DIRTY_REGION_SIZE);
		for (uint32_t i = 0; i < data_cache_dirty_region_count; i++) {
			multimesh->data_cache_dirty_regions[i] = false;
		}
		multimesh->data_cache_used_dirty_regions = 0;

		_multimesh_mark_all_dirty(multimesh, false, true); //update AABB
	} else if (multimesh->mesh.is_valid()) {
		//if we have a mesh set, we need to re-generate the AABB from the new data
		const float *data = p_buffer.ptr();

		if (multimesh->custom_aabb == AABB()) {
			_multimesh_re_create_aabb(multimesh, data, multimesh->instances);
			multimesh->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
		}
	}
}

RID MeshStorage::_multimesh_get_command_buffer_rd_rid(RID p_multimesh) const {
	ERR_FAIL_V_MSG(RID(), "GLES2 does not implement indirect multimeshes.");
}

RID MeshStorage::_multimesh_get_buffer_rd_rid(RID p_multimesh) const {
	ERR_FAIL_V_MSG(RID(), "GLES2 does not contain a Rid for the multimesh buffer.");
}

Vector<float> MeshStorage::_multimesh_get_buffer(RID p_multimesh) const {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL_V(multimesh, Vector<float>());
	Vector<float> ret;
	if (multimesh->buffer[multimesh->current_buffer] == 0 || multimesh->instances == 0) {
		return Vector<float>();
	} else if (multimesh->data_cache.size()) {
		ret = multimesh->data_cache;
	} else {
		// Buffer not cached, so fetch from GPU memory. This can be a stalling operation, avoid whenever possible.

		Vector<uint8_t> buffer = Utilities::buffer_get_data(GL_ARRAY_BUFFER, multimesh->buffer[multimesh->current_buffer], multimesh->instances * multimesh->stride_cache * sizeof(float));
		ret.resize(multimesh->instances * multimesh->stride_cache);
		{
			float *w = ret.ptrw();
			const uint8_t *r = buffer.ptr();
			memcpy(w, r, buffer.size());
		}
	}
	if (multimesh->uses_colors || multimesh->uses_custom_data) {
		// Need to decompress buffer.
		uint32_t new_stride = multimesh->xform_format == RSE::MULTIMESH_TRANSFORM_2D ? 8 : 12;
		new_stride += multimesh->uses_colors ? 4 : 0;
		new_stride += multimesh->uses_custom_data ? 4 : 0;

		Vector<float> decompressed;
		decompressed.resize(multimesh->instances * (int)new_stride);
		float *w = decompressed.ptrw();
		const float *r = ret.ptr();

		for (int i = 0; i < multimesh->instances; i++) {
			{
				float *newptr = w + i * new_stride;
				const float *oldptr = r + i * multimesh->stride_cache;
				float vals[8] = { oldptr[0], oldptr[1], oldptr[2], oldptr[3], oldptr[4], oldptr[5], oldptr[6], oldptr[7] };
				memcpy(newptr, vals, 8 * 4);
			}

			if (multimesh->xform_format == RSE::MULTIMESH_TRANSFORM_3D) {
				float *newptr = w + i * new_stride + 8;
				const float *oldptr = r + i * multimesh->stride_cache + 8;
				float vals[8] = { oldptr[0], oldptr[1], oldptr[2], oldptr[3] };
				memcpy(newptr, vals, 4 * 4);
			}

			if (multimesh->uses_colors) {
				float *newptr = w + i * new_stride + (multimesh->xform_format == RSE::MULTIMESH_TRANSFORM_2D ? 8 : 12);
				const float *oldptr = r + i * multimesh->stride_cache + multimesh->color_offset_cache;
				uint16_t raw_data[4];
				memcpy(raw_data, oldptr, 2 * 4);
				newptr[0] = Math::half_to_float(raw_data[0]);
				newptr[1] = Math::half_to_float(raw_data[1]);
				newptr[2] = Math::half_to_float(raw_data[2]);
				newptr[3] = Math::half_to_float(raw_data[3]);
			}
			if (multimesh->uses_custom_data) {
				float *newptr = w + i * new_stride + (multimesh->xform_format == RSE::MULTIMESH_TRANSFORM_2D ? 8 : 12) + (multimesh->uses_colors ? 4 : 0);
				const float *oldptr = r + i * multimesh->stride_cache + multimesh->custom_data_offset_cache;
				uint16_t raw_data[4];
				memcpy(raw_data, oldptr, 2 * 4);
				newptr[0] = Math::half_to_float(raw_data[0]);
				newptr[1] = Math::half_to_float(raw_data[1]);
				newptr[2] = Math::half_to_float(raw_data[2]);
				newptr[3] = Math::half_to_float(raw_data[3]);
			}
		}
		return decompressed;
	} else {
		return ret;
	}
}

void MeshStorage::_multimesh_set_visible_instances(RID p_multimesh, int p_visible) {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL(multimesh);
	ERR_FAIL_COND(p_visible < -1 || p_visible > multimesh->instances);
	if (multimesh->visible_instances == p_visible) {
		return;
	}

	if (multimesh->data_cache.size()) {
		// There is a data cache, but we may need to update some sections.
		_multimesh_mark_all_dirty(multimesh, false, true);
		int start = multimesh->visible_instances >= 0 ? multimesh->visible_instances : multimesh->instances;
		for (int i = start; i < p_visible; i++) {
			_multimesh_mark_dirty(multimesh, i, true);
		}
	}

	multimesh->visible_instances = p_visible;

	multimesh->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MULTIMESH_VISIBLE_INSTANCES);
}

int MeshStorage::_multimesh_get_visible_instances(RID p_multimesh) const {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL_V(multimesh, 0);
	return multimesh->visible_instances;
}

MeshStorage::MultiMeshInterpolator *MeshStorage::_multimesh_get_interpolator(RID p_multimesh) const {
	MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL_V_MSG(multimesh, nullptr, "Multimesh not found: " + itos(p_multimesh.get_id()));

	return &multimesh->interpolator;
}

void MeshStorage::_update_dirty_multimeshes() {
	while (multimesh_dirty_list) {
		MultiMesh *multimesh = multimesh_dirty_list;

		bool uses_motion_vectors = RSG::viewport->get_num_viewports_with_motion_vectors() > 0;
		if (uses_motion_vectors) {
			multimesh->prev_buffer = multimesh->current_buffer;
			uint32_t new_buffer_index = multimesh->current_buffer ^ 1;

			// Generate secondary buffer if it doesn't exist.
			if (multimesh->buffer[new_buffer_index] == 0 && multimesh->instances) {
				GLuint new_buffer = 0;
				glGenBuffers(1, &new_buffer);
				glBindBuffer(GL_ARRAY_BUFFER, new_buffer);
				GLES2::Utilities::get_singleton()->buffer_allocate_data(GL_ARRAY_BUFFER, new_buffer, multimesh->instances * multimesh->stride_cache * sizeof(float), nullptr, GL_STATIC_DRAW, "MultiMesh secondary buffer");
				glBindBuffer(GL_ARRAY_BUFFER, 0);
				multimesh->buffer[new_buffer_index] = new_buffer;
			}

			multimesh->current_buffer = new_buffer_index;
			multimesh->last_change = RSG::rasterizer->get_frame_number();
		}

		_update_dirty_multimesh(multimesh, uses_motion_vectors);

		multimesh_dirty_list = multimesh->dirty_list;

		multimesh->dirty_list = nullptr;
		multimesh->dirty = false;
	}

	multimesh_dirty_list = nullptr;
}

void MeshStorage::_update_dirty_multimesh(MultiMesh *p_multimesh, bool p_uses_motion_vectors) {
	if (p_multimesh->data_cache.size()) { // May have been cleared, so only process if it exists.
		const float *data = p_multimesh->data_cache.ptr();

		uint32_t visible_instances = p_multimesh->visible_instances >= 0 ? p_multimesh->visible_instances : p_multimesh->instances;

		if (p_multimesh->data_cache_used_dirty_regions) {
			uint32_t data_cache_dirty_region_count = Math::division_round_up(p_multimesh->instances, (int)MULTIMESH_DIRTY_REGION_SIZE);
			uint32_t visible_region_count = visible_instances == 0 ? 0 : Math::division_round_up(visible_instances, (uint32_t)MULTIMESH_DIRTY_REGION_SIZE);

			GLint region_size = p_multimesh->stride_cache * MULTIMESH_DIRTY_REGION_SIZE * sizeof(float);

			if (p_multimesh->data_cache_used_dirty_regions > 32 || p_multimesh->data_cache_used_dirty_regions > visible_region_count / 2 || p_uses_motion_vectors) {
				// If there are too many dirty regions, the dirty regions represent the majority of visible regions, or motion vectors are used:
				// Just copy all, else transfer cost piles up too much.
				glBindBuffer(GL_ARRAY_BUFFER, p_multimesh->buffer[p_multimesh->current_buffer]);
				glBufferSubData(GL_ARRAY_BUFFER, 0, MIN(visible_region_count * region_size, p_multimesh->instances * p_multimesh->stride_cache * sizeof(float)), data);
				glBindBuffer(GL_ARRAY_BUFFER, 0);
			} else {
				// Not that many regions? Update them all.
				// TODO: profile the performance cost on low end
				glBindBuffer(GL_ARRAY_BUFFER, p_multimesh->buffer[p_multimesh->current_buffer]);
				for (uint32_t i = 0; i < visible_region_count; i++) {
					if (p_multimesh->data_cache_dirty_regions[i]) {
						GLint offset = i * region_size;
						GLint size = p_multimesh->stride_cache * (uint32_t)p_multimesh->instances * (uint32_t)sizeof(float);
						uint32_t region_start_index = p_multimesh->stride_cache * MULTIMESH_DIRTY_REGION_SIZE * i;
						glBufferSubData(GL_ARRAY_BUFFER, offset, MIN(region_size, size - offset), &data[region_start_index]);
					}
				}
				glBindBuffer(GL_ARRAY_BUFFER, 0);
			}

			for (uint32_t i = 0; i < data_cache_dirty_region_count; i++) {
				p_multimesh->data_cache_dirty_regions[i] = false;
			}

			p_multimesh->data_cache_used_dirty_regions = 0;
		}

		if (p_multimesh->aabb_dirty && p_multimesh->mesh.is_valid()) {
			p_multimesh->aabb_dirty = false;
			if (p_multimesh->custom_aabb == AABB()) {
				_multimesh_re_create_aabb(p_multimesh, data, visible_instances);
				p_multimesh->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
			}
		}
	}
}

void GLES2::MeshStorage::multimesh_vertex_attrib_setup(GLuint p_instance_buffer, uint32_t p_stride, bool p_uses_format_2d, bool p_has_color_or_custom_data, int p_attrib_base_index) {
	glBindBuffer(GL_ARRAY_BUFFER, p_instance_buffer);

	glEnableVertexAttribArray(p_attrib_base_index + 0);
	glVertexAttribPointer(p_attrib_base_index + 0, 4, GL_FLOAT, GL_FALSE, p_stride * sizeof(float), CAST_INT_TO_UCHAR_PTR(0));
	glVertexAttribDivisor(p_attrib_base_index + 0, 1);
	glEnableVertexAttribArray(p_attrib_base_index + 1);
	glVertexAttribPointer(p_attrib_base_index + 1, 4, GL_FLOAT, GL_FALSE, p_stride * sizeof(float), CAST_INT_TO_UCHAR_PTR(sizeof(float) * 4));
	glVertexAttribDivisor(p_attrib_base_index + 1, 1);
	if (!p_uses_format_2d) {
		glEnableVertexAttribArray(p_attrib_base_index + 2);
		glVertexAttribPointer(p_attrib_base_index + 2, 4, GL_FLOAT, GL_FALSE, p_stride * sizeof(float), CAST_INT_TO_UCHAR_PTR(sizeof(float) * 8));
		glVertexAttribDivisor(p_attrib_base_index + 2, 1);
	}

	if (p_has_color_or_custom_data) {
		uint32_t color_custom_offset = p_uses_format_2d ? 8 : 12;
		glEnableVertexAttribArray(p_attrib_base_index + 3);
		glVertexAttribIPointer(p_attrib_base_index + 3, 4, GL_UNSIGNED_INT, p_stride * sizeof(float), CAST_INT_TO_UCHAR_PTR(color_custom_offset * sizeof(float)));
		glVertexAttribDivisor(p_attrib_base_index + 3, 1);
	} else {
		// Set all default instance color and custom data values to 1.0 or 0.0 using a compressed format.
		uint16_t zero = Math::make_half_float(0.0f);
		uint16_t one = Math::make_half_float(1.0f);
		GLuint default_color = (uint32_t(one) << 16) | one;
		GLuint default_custom = (uint32_t(zero) << 16) | zero;
		glVertexAttribI4ui(p_attrib_base_index + 3, default_color, default_color, default_custom, default_custom);
	}
}

/* SKELETON API */

RID MeshStorage::skeleton_allocate() {
	return skeleton_owner.allocate_rid();
}

void MeshStorage::skeleton_initialize(RID p_rid) {
	skeleton_owner.initialize_rid(p_rid, Skeleton());
}

void MeshStorage::skeleton_free(RID p_rid) {
	_update_dirty_skeletons();
	skeleton_allocate_data(p_rid, 0);
	Skeleton *skeleton = skeleton_owner.get_or_null(p_rid);
	skeleton->dependency.deleted_notify(p_rid);
	skeleton_owner.free(p_rid);
}

void MeshStorage::_skeleton_make_dirty(Skeleton *skeleton) {
	if (!skeleton->dirty) {
		skeleton->dirty = true;
		skeleton->dirty_list = skeleton_dirty_list;
		skeleton_dirty_list = skeleton;
	}
}

void MeshStorage::skeleton_allocate_data(RID p_skeleton, int p_bones, bool p_2d_skeleton) {
	Skeleton *skeleton = skeleton_owner.get_or_null(p_skeleton);
	ERR_FAIL_NULL(skeleton);
	ERR_FAIL_COND(p_bones < 0);

	if (skeleton->size == p_bones && skeleton->use_2d == p_2d_skeleton) {
		return;
	}

	skeleton->size = p_bones;
	skeleton->use_2d = p_2d_skeleton;

	// No transforms texture: bone matrices live only in skeleton->data for CPU skinning.
	// Keep the historical 256-wide row layout so bone indexing is unchanged.
	int rows = (p_bones * (p_2d_skeleton ? 2 : 3)) / 256;
	if ((p_bones * (p_2d_skeleton ? 2 : 3)) % 256) {
		rows++;
	}

	skeleton->data.clear();

	if (skeleton->size) {
		skeleton->data.resize(256 * rows * 4);

		memset(skeleton->data.ptr(), 0, skeleton->data.size() * sizeof(float));

		_skeleton_make_dirty(skeleton);
	}

	skeleton->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_SKELETON_DATA);
}

void MeshStorage::skeleton_set_base_transform_2d(RID p_skeleton, const Transform2D &p_base_transform) {
	Skeleton *skeleton = skeleton_owner.get_or_null(p_skeleton);

	ERR_FAIL_NULL(skeleton);
	ERR_FAIL_COND(!skeleton->use_2d);

	skeleton->base_transform_2d = p_base_transform;
}

int MeshStorage::skeleton_get_bone_count(RID p_skeleton) const {
	Skeleton *skeleton = skeleton_owner.get_or_null(p_skeleton);
	ERR_FAIL_NULL_V(skeleton, 0);

	return skeleton->size;
}

void MeshStorage::skeleton_bone_set_transform(RID p_skeleton, int p_bone, const Transform3D &p_transform) {
	Skeleton *skeleton = skeleton_owner.get_or_null(p_skeleton);

	ERR_FAIL_NULL(skeleton);
	ERR_FAIL_INDEX(p_bone, skeleton->size);
	ERR_FAIL_COND(skeleton->use_2d);

	float *dataptr = skeleton->data.ptr() + p_bone * 12;

	dataptr[0] = p_transform.basis.rows[0][0];
	dataptr[1] = p_transform.basis.rows[0][1];
	dataptr[2] = p_transform.basis.rows[0][2];
	dataptr[3] = p_transform.origin.x;
	dataptr[4] = p_transform.basis.rows[1][0];
	dataptr[5] = p_transform.basis.rows[1][1];
	dataptr[6] = p_transform.basis.rows[1][2];
	dataptr[7] = p_transform.origin.y;
	dataptr[8] = p_transform.basis.rows[2][0];
	dataptr[9] = p_transform.basis.rows[2][1];
	dataptr[10] = p_transform.basis.rows[2][2];
	dataptr[11] = p_transform.origin.z;

	_skeleton_make_dirty(skeleton);
}

Transform3D MeshStorage::skeleton_bone_get_transform(RID p_skeleton, int p_bone) const {
	Skeleton *skeleton = skeleton_owner.get_or_null(p_skeleton);

	ERR_FAIL_NULL_V(skeleton, Transform3D());
	ERR_FAIL_INDEX_V(p_bone, skeleton->size, Transform3D());
	ERR_FAIL_COND_V(skeleton->use_2d, Transform3D());

	const float *dataptr = skeleton->data.ptr() + p_bone * 12;

	Transform3D t;

	t.basis.rows[0][0] = dataptr[0];
	t.basis.rows[0][1] = dataptr[1];
	t.basis.rows[0][2] = dataptr[2];
	t.origin.x = dataptr[3];
	t.basis.rows[1][0] = dataptr[4];
	t.basis.rows[1][1] = dataptr[5];
	t.basis.rows[1][2] = dataptr[6];
	t.origin.y = dataptr[7];
	t.basis.rows[2][0] = dataptr[8];
	t.basis.rows[2][1] = dataptr[9];
	t.basis.rows[2][2] = dataptr[10];
	t.origin.z = dataptr[11];

	return t;
}

void MeshStorage::skeleton_bone_set_transform_2d(RID p_skeleton, int p_bone, const Transform2D &p_transform) {
	Skeleton *skeleton = skeleton_owner.get_or_null(p_skeleton);

	ERR_FAIL_NULL(skeleton);
	ERR_FAIL_INDEX(p_bone, skeleton->size);
	ERR_FAIL_COND(!skeleton->use_2d);

	float *dataptr = skeleton->data.ptr() + p_bone * 8;

	dataptr[0] = p_transform.columns[0][0];
	dataptr[1] = p_transform.columns[1][0];
	dataptr[2] = 0;
	dataptr[3] = p_transform.columns[2][0];
	dataptr[4] = p_transform.columns[0][1];
	dataptr[5] = p_transform.columns[1][1];
	dataptr[6] = 0;
	dataptr[7] = p_transform.columns[2][1];

	_skeleton_make_dirty(skeleton);
}

Transform2D MeshStorage::skeleton_bone_get_transform_2d(RID p_skeleton, int p_bone) const {
	Skeleton *skeleton = skeleton_owner.get_or_null(p_skeleton);

	ERR_FAIL_NULL_V(skeleton, Transform2D());
	ERR_FAIL_INDEX_V(p_bone, skeleton->size, Transform2D());
	ERR_FAIL_COND_V(!skeleton->use_2d, Transform2D());

	const float *dataptr = skeleton->data.ptr() + p_bone * 8;

	Transform2D t;
	t.columns[0][0] = dataptr[0];
	t.columns[1][0] = dataptr[1];
	t.columns[2][0] = dataptr[3];
	t.columns[0][1] = dataptr[4];
	t.columns[1][1] = dataptr[5];
	t.columns[2][1] = dataptr[7];

	return t;
}

void MeshStorage::_update_dirty_skeletons() {
	while (skeleton_dirty_list) {
		Skeleton *skeleton = skeleton_dirty_list;

		// No texture upload: CPU skinning reads skeleton->data directly.

		skeleton_dirty_list = skeleton->dirty_list;

		skeleton->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_SKELETON_BONES);

		skeleton->version++;

		skeleton->dirty = false;
		skeleton->dirty_list = nullptr;
	}

	skeleton_dirty_list = nullptr;
}

void MeshStorage::skeleton_update_dependency(RID p_skeleton, DependencyTracker *p_instance) {
	Skeleton *skeleton = skeleton_owner.get_or_null(p_skeleton);
	ERR_FAIL_NULL(skeleton);

	p_instance->update_dependency(&skeleton->dependency);
}

#endif // GLES2_ENABLED
