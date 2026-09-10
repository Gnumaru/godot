/**************************************************************************/
/*  glow.cpp                                                              */
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

#include "glow.h"

#ifdef GLES2_ENABLED

#include "drivers/gles2/storage/texture_storage.h"

using namespace GLES2;

Glow *Glow::singleton = nullptr;

Glow *Glow::get_singleton() {
	return singleton;
}

Glow::Glow() {
	singleton = this;
	// GLES2 simplification (Fase A, 3D minimo low-end): glow desativado.
	// Mantido como stub no-op que ainda compila como GLES3 para facilitar porte futuro.
	// Sem inicializacao de shader, VBO/VAO ou estado GL.
}

Glow::~Glow() {
	singleton = nullptr;
}

void Glow::_draw_screen_triangle() {
	// No-op (glow desativado na simplificacao GLES2).
}

void Glow::process_glow(GLuint p_source_color, Size2i p_size, const Glow::Level *p_glow_buffers, uint32_t p_view, bool p_use_multiview) {
	// No-op (glow desativado na simplificacao GLES2).
	(void)p_source_color;
	(void)p_size;
	(void)p_glow_buffers;
	(void)p_view;
	(void)p_use_multiview;
}

#endif // GLES2_ENABLED
