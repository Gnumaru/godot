/**************************************************************************/
/*  render_scene_buffers_gles2.cpp                                        */
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

#include "render_scene_buffers_gles2.h"

#ifdef GLES2_ENABLED

#include "drivers/gles2/storage/config.h"
#include "drivers/gles2/storage/texture_storage.h"
#include "drivers/gles2/storage/utilities.h"

// Will only be defined if GLES 3.2 headers are included
#ifndef GL_TEXTURE_2D_MULTISAMPLE_ARRAY
#define GL_TEXTURE_2D_MULTISAMPLE_ARRAY 0x9102
#endif

RenderSceneBuffersGLES2::RenderSceneBuffersGLES2() {
	for (int i = 0; i < 4; i++) {
		glow.levels[i].color = 0;
		glow.levels[i].fbo = 0;
	}
}

RenderSceneBuffersGLES2::~RenderSceneBuffersGLES2() {
	free_render_buffer_data();
}

void RenderSceneBuffersGLES2::_rt_attach_textures(GLuint p_color, GLuint p_depth, GLsizei p_samples, uint32_t p_view_count, bool p_depth_has_stencil) {
	// GLES2 simplification (3D minimo low-end): sem multiview/XR nem MSAA.
	// Apenas single-view sem amostras; parametros extras ignorados para manter a API.
	(void)p_samples;
	(void)p_view_count;
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, p_color, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, p_depth_has_stencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, p_depth, 0);
}

GLuint RenderSceneBuffersGLES2::_rt_get_cached_fbo(GLuint p_color, GLuint p_depth, GLsizei p_samples, uint32_t p_view_count) {
	// GLES2 simplification (3D minimo low-end): sem cache de FBO MSAA
	// (usado apenas por MSAA direto no render target em Android/Web).
	(void)p_color;
	(void)p_depth;
	(void)p_samples;
	(void)p_view_count;
	return 0;
}

void RenderSceneBuffersGLES2::configure(const RenderSceneBuffersConfiguration *p_config) {
	GLES2::TextureStorage *texture_storage = GLES2::TextureStorage::get_singleton();
	GLES2::Config *config = GLES2::Config::get_singleton();

	free_render_buffer_data();

	internal_size = p_config->get_internal_size();
	target_size = p_config->get_target_size();
	scaling_3d_mode = p_config->get_scaling_3d_mode();
	//fsr_sharpness = p_config->get_fsr_sharpness();
	//texture_mipmap_bias = p_config->get_texture_mipmap_bias();
	//anisotropic_filtering_level = p_config->get_anisotropic_filtering_level();
	render_target = p_config->get_render_target();
	// GLES2 simplification (3D minimo low-end): sem MSAA.
	msaa3d.mode = RSE::VIEWPORT_MSAA_DISABLED;
	//screen_space_aa = p_config->get_screen_space_aa();
	//use_debanding = p_config->get_use_debanding();
	// GLES2 simplification (3D minimo low-end): sem multiview/XR (mono apenas).
	view_count = 1;

	bool use_multiview = false;

	// Get color format data from our render target so we match those
	if (render_target.is_valid()) {
		color_internal_format = texture_storage->render_target_get_color_internal_format(render_target);
		color_format = texture_storage->render_target_get_color_format(render_target);
		color_type = texture_storage->render_target_get_color_type(render_target);
		color_format_size = texture_storage->render_target_get_color_format_size(render_target);
	} else {
		// reflection probe? or error?
		color_internal_format = GL_RGBA8;
		color_format = GL_RGBA;
		color_type = GL_UNSIGNED_BYTE;
		color_format_size = 4;
	}

	// Check our scaling mode
	if (scaling_3d_mode != RSE::VIEWPORT_SCALING_3D_MODE_OFF && internal_size.x == 0 && internal_size.y == 0) {
		// Disable, no size set.
		scaling_3d_mode = RSE::VIEWPORT_SCALING_3D_MODE_OFF;
	} else if (scaling_3d_mode != RSE::VIEWPORT_SCALING_3D_MODE_OFF && internal_size == target_size) {
		// If size matches, we won't use scaling.
		scaling_3d_mode = RSE::VIEWPORT_SCALING_3D_MODE_OFF;
	} else if (scaling_3d_mode != RSE::VIEWPORT_SCALING_3D_MODE_OFF && !(scaling_3d_mode == RSE::VIEWPORT_SCALING_3D_MODE_BILINEAR || scaling_3d_mode == RSE::VIEWPORT_SCALING_3D_MODE_NEAREST)) {
		WARN_PRINT_ONCE("The Compatibility rendering method only supports bilinear and nearest-neighbor scaling. Falling back to bilinear scaling.");
		scaling_3d_mode = RSE::VIEWPORT_SCALING_3D_MODE_BILINEAR;
	}
	// Nearest/bilinear scaling decision is handled in `RasterizerSceneGLES2::_render_post_processing()`.

	// Check if we support MSAA.
	if (msaa3d.mode != RSE::VIEWPORT_MSAA_DISABLED && internal_size.x == 0 && internal_size.y == 0) {
		// Disable, no size set.
		msaa3d.mode = RSE::VIEWPORT_MSAA_DISABLED;
	} else if (!use_multiview && msaa3d.mode != RSE::VIEWPORT_MSAA_DISABLED && !config->msaa_supported && !config->rt_msaa_supported) {
		WARN_PRINT_ONCE("MSAA is not supported on this device.");
		msaa3d.mode = RSE::VIEWPORT_MSAA_DISABLED;
	} else if (use_multiview && msaa3d.mode != RSE::VIEWPORT_MSAA_DISABLED && !config->msaa_multiview_supported && !config->rt_msaa_multiview_supported) {
		WARN_PRINT_ONCE("Multiview MSAA is not supported on this device.");
		msaa3d.mode = RSE::VIEWPORT_MSAA_DISABLED;
	}

	// We don't create our buffers right away because post effects can be made active at any time and change our buffer configuration.
}

void RenderSceneBuffersGLES2::_check_render_buffers() {
	GLES2::TextureStorage *texture_storage = GLES2::TextureStorage::get_singleton();
	// GLES2 simplification: sem MSAA/multiview, Config nao necessario aqui.

	ERR_FAIL_COND(view_count == 0);

	bool use_internal_buffer = scaling_3d_mode != RSE::VIEWPORT_SCALING_3D_MODE_OFF || apply_environment_effects_in_post || apply_canvas_bg_exposure;
	GLenum depth_format = GL_DEPTH24_STENCIL8;
	uint32_t depth_format_size = 4;
	// GLES2 simplification (3D minimo low-end): sem multiview/XR (mono apenas).

	if (!use_internal_buffer && internal3d.color != 0) {
		_clear_intermediate_buffers();
	}

	if ((!use_internal_buffer || internal3d.color != 0) && (msaa3d.mode == RSE::VIEWPORT_MSAA_DISABLED || msaa3d.color != 0)) {
		// already setup!
		return;
	}

	if (use_internal_buffer && internal3d.color == 0) {
		// Setup our internal buffer.
		// GLES2 simplification (3D minimo low-end): single-view apenas.
		GLenum texture_target = GL_TEXTURE_2D;

		// Create our color buffer.
		glGenTextures(1, &internal3d.color);
		glBindTexture(texture_target, internal3d.color);

		glTexImage2D(texture_target, 0, color_internal_format, internal_size.x, internal_size.y, 0, color_format, color_type, nullptr);

		glTexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		GLES2::Utilities::get_singleton()->texture_allocated_data(internal3d.color, internal_size.x * internal_size.y * view_count * color_format_size, "3D color texture");

		// Create our depth buffer.
		glGenTextures(1, &internal3d.depth);
		glBindTexture(texture_target, internal3d.depth);

		glTexImage2D(texture_target, 0, depth_format, internal_size.x, internal_size.y, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);

		glTexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		GLES2::Utilities::get_singleton()->texture_allocated_data(internal3d.depth, internal_size.x * internal_size.y * view_count * depth_format_size, "3D depth texture");

		// Create our internal 3D FBO.
		// Note that if MSAA is used and our rt_msaa_* extensions are available, this is only used for blitting and effects.
		glGenFramebuffers(1, &internal3d.fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, internal3d.fbo);

		// GLES2 simplification (3D minimo low-end): single-view apenas.
		{
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_target, internal3d.color, 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, texture_target, internal3d.depth, 0);
		}

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			_clear_intermediate_buffers();
			WARN_PRINT("Could not create 3D internal buffers, status: " + texture_storage->get_framebuffer_error(status));
		}

		glBindTexture(texture_target, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, GLES2::TextureStorage::system_fbo);
	}

	// GLES2 simplification (3D minimo low-end): sem MSAA 3D.
	// Mantido como stub que ainda compila como GLES3 para facilitar porte futuro.
	msaa3d.samples = 1;
	msaa3d.check_fbo_cache = false;
}

void RenderSceneBuffersGLES2::configure_for_probe(Size2i p_size) {
	internal_size = p_size;
	target_size = p_size;
	scaling_3d_mode = RSE::VIEWPORT_SCALING_3D_MODE_OFF;
	view_count = 1;
}

void RenderSceneBuffersGLES2::_clear_msaa3d_buffers() {
	for (const FBDEF &cached_fbo : msaa3d.cached_fbos) {
		GLuint fbo = cached_fbo.fbo;
		glDeleteFramebuffers(1, &fbo);
	}
	msaa3d.cached_fbos.clear();

	if (msaa3d.fbo) {
		glDeleteFramebuffers(1, &msaa3d.fbo);
		msaa3d.fbo = 0;
	}

	if (msaa3d.color != 0) {
		if (view_count == 1) {
			GLES2::Utilities::get_singleton()->render_buffer_free_data(msaa3d.color);
		} else {
			GLES2::Utilities::get_singleton()->texture_free_data(msaa3d.color);
		}
		msaa3d.color = 0;
	}

	if (msaa3d.depth != 0) {
		if (view_count == 1) {
			GLES2::Utilities::get_singleton()->render_buffer_free_data(msaa3d.depth);
		} else {
			GLES2::Utilities::get_singleton()->texture_free_data(msaa3d.depth);
		}
		msaa3d.depth = 0;
	}
}

void RenderSceneBuffersGLES2::_clear_intermediate_buffers() {
	if (internal3d.fbo) {
		glDeleteFramebuffers(1, &internal3d.fbo);
		internal3d.fbo = 0;
	}

	if (internal3d.color != 0) {
		GLES2::Utilities::get_singleton()->texture_free_data(internal3d.color);
		internal3d.color = 0;
	}

	if (internal3d.depth != 0) {
		GLES2::Utilities::get_singleton()->texture_free_data(internal3d.depth);
		internal3d.depth = 0;
	}
}

void RenderSceneBuffersGLES2::check_backbuffer(bool p_need_color, bool p_need_depth) {
	GLES2::TextureStorage *texture_storage = GLES2::TextureStorage::get_singleton();

	// Setup our back buffer

	if (backbuffer3d.fbo == 0) {
		glGenFramebuffers(1, &backbuffer3d.fbo);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, backbuffer3d.fbo);

	// GLES2 simplification (3D minimo low-end): sem multiview/XR (mono apenas).
	GLenum texture_target = GL_TEXTURE_2D;
	GLenum depth_format = GL_DEPTH24_STENCIL8;
	uint32_t depth_format_size = 4;

	if (backbuffer3d.color == 0 && p_need_color) {
		glGenTextures(1, &backbuffer3d.color);
		glBindTexture(texture_target, backbuffer3d.color);

		glTexImage2D(texture_target, 0, color_internal_format, internal_size.x, internal_size.y, 0, color_format, color_type, nullptr);

		glTexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		GLES2::Utilities::get_singleton()->texture_allocated_data(backbuffer3d.color, internal_size.x * internal_size.y * view_count * color_format_size, "3D Back buffer color texture");

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_target, backbuffer3d.color, 0);
	}

	if (backbuffer3d.depth == 0 && p_need_depth) {
		glGenTextures(1, &backbuffer3d.depth);
		glBindTexture(texture_target, backbuffer3d.depth);

		glTexImage2D(texture_target, 0, depth_format, internal_size.x, internal_size.y, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);

		glTexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		GLES2::Utilities::get_singleton()->texture_allocated_data(backbuffer3d.depth, internal_size.x * internal_size.y * view_count * depth_format_size, "3D back buffer depth texture");

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, texture_target, backbuffer3d.depth, 0);
	}

	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		_clear_back_buffers();
		WARN_PRINT("Could not create 3D back buffers, status: " + texture_storage->get_framebuffer_error(status));
	}

	glBindTexture(texture_target, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, GLES2::TextureStorage::system_fbo);
}

void RenderSceneBuffersGLES2::_clear_back_buffers() {
	if (backbuffer3d.fbo) {
		glDeleteFramebuffers(1, &backbuffer3d.fbo);
		backbuffer3d.fbo = 0;
	}

	if (backbuffer3d.color != 0) {
		GLES2::Utilities::get_singleton()->texture_free_data(backbuffer3d.color);
		backbuffer3d.color = 0;
	}

	if (backbuffer3d.depth != 0) {
		GLES2::Utilities::get_singleton()->texture_free_data(backbuffer3d.depth);
		backbuffer3d.depth = 0;
	}
}

void RenderSceneBuffersGLES2::set_apply_environment_effects_in_post(bool p_apply_in_post) {
	apply_environment_effects_in_post = p_apply_in_post;
}

void RenderSceneBuffersGLES2::set_apply_canvas_bg_exposure(bool p_apply_canvas_bg_exposure) {
	apply_canvas_bg_exposure = p_apply_canvas_bg_exposure;
}

void RenderSceneBuffersGLES2::check_glow_buffers() {
	// GLES2 simplification (Fase A): glow desativado, sem alocacao de texturas/FBOs.
	// Stub no-op que ainda compila como GLES3.
}

void RenderSceneBuffersGLES2::_clear_glow_buffers() {
	// GLES2 simplification (Fase A): nada alocado, nada a liberar.
}

void RenderSceneBuffersGLES2::free_render_buffer_data() {
	_clear_msaa3d_buffers();
	_clear_intermediate_buffers();
	_clear_back_buffers();
	_clear_glow_buffers();
}

GLuint RenderSceneBuffersGLES2::get_render_fbo() {
	GLES2::TextureStorage *texture_storage = GLES2::TextureStorage::get_singleton();
	GLuint rt_fbo = 0;

	_check_render_buffers();

	if (msaa3d.check_fbo_cache) {
		GLuint color = texture_storage->render_target_get_color(render_target);
		GLuint depth = texture_storage->render_target_get_depth(render_target);

		rt_fbo = _rt_get_cached_fbo(color, depth, msaa3d.samples, view_count);
		if (rt_fbo == 0) {
			// Somehow couldn't obtain this? Just render without MSAA.
			rt_fbo = texture_storage->render_target_get_fbo(render_target);
		}
	} else if (msaa3d.fbo != 0) {
		// We have an MSAA fbo, render to our MSAA buffer
		return msaa3d.fbo;
	} else if (internal3d.fbo != 0) {
		// We have an internal buffer, render to our internal buffer!
		return internal3d.fbo;
	} else {
		rt_fbo = texture_storage->render_target_get_fbo(render_target);
	}

	if (texture_storage->render_target_is_reattach_textures(render_target)) {
		GLuint color = texture_storage->render_target_get_color(render_target);
		GLuint depth = texture_storage->render_target_get_depth(render_target);
		bool depth_has_stencil = texture_storage->render_target_get_depth_has_stencil(render_target);

		glBindFramebuffer(GL_FRAMEBUFFER, rt_fbo);
		_rt_attach_textures(color, depth, msaa3d.samples, view_count, depth_has_stencil);
		glBindFramebuffer(GL_FRAMEBUFFER, texture_storage->system_fbo);
	}

	return rt_fbo;
}

#endif // GLES2_ENABLED
