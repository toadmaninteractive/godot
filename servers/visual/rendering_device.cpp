/*************************************************************************/
/*  rendering_device.cpp                                                 */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "rendering_device.h"
#include "core/crypto/crypto_core.h"

RenderingDevice *RenderingDevice::singleton = NULL;

RenderingDevice *RenderingDevice::get_singleton() {
	return singleton;
}

RenderingDevice::ShaderCompileFunction RenderingDevice::compile_function = NULL;
RenderingDevice::ShaderCacheFunction RenderingDevice::cache_function = NULL;

void RenderingDevice::shader_set_compile_function(ShaderCompileFunction p_function) {
	compile_function = p_function;
}
void RenderingDevice::shader_set_cache_function(ShaderCacheFunction p_function) {
	cache_function = p_function;
}

PoolVector<uint8_t> RenderingDevice::shader_compile_from_source(ShaderStage p_stage, const String &p_source_code, ShaderLanguage p_language, String *r_error, bool p_allow_cache) {
	if (p_allow_cache && cache_function) {
		PoolVector<uint8_t> cache = cache_function(p_stage, p_source_code, p_language);
		if (cache.size()) {
			return cache;
		}
	}

	ERR_FAIL_COND_V(!compile_function, PoolVector<uint8_t>());

#ifdef PS4_EDITOR_TOOLS
	PoolVector<uint8_t> data = compile_function(p_stage, p_source_code, p_language, r_error);

	// Cache the data so that we can fetch it
	uint32_t* uint_data = (uint32_t*)data.read().ptr();

	CompiledShaderCacheEntry entry;
	entry.size = data.size() / sizeof(uint32_t);
	entry.data.resize(entry.size);
	entry.orig_source_code = p_source_code;

	if (entry.size == 0) {
		return data;
	}
	
	PoolVector<uint32_t>::Write write = entry.data.write();
	
	for (uint32_t i = 0; i < entry.size; ++i) {
		write[i] = uint_data[i];
	}

	unsigned char hash[16];
	CryptoCore::md5(data.read().ptr(), data.size(), hash);
	compiled_shader_cache.insert(String::md5(hash), entry);

	return data;
#else
	return compile_function(p_stage, p_source_code, p_language, r_error);
#endif
}

RenderingDevice::RenderingDevice() {

	ShaderCompileFunction compile_function;
	ShaderCacheFunction cache_function;

	singleton = this;
}
