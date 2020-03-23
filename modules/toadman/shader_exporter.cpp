#include "shader_exporter.h"
#include "core/io/json.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "core/string_builder.h"
#include "editor/editor_file_system.h"
#include "servers/visual/rasterizer_rd/rasterizer_canvas_rd.h"
#include "servers/visual/rasterizer_rd/rasterizer_scene_high_end_rd.h"
#include "servers/visual/rasterizer_rd/rasterizer_storage_rd.h"
#include "servers/visual/visual_server_globals.h"

#include <shader.h>
#include <shader/wave_psslc.h>
#include <spirv_hlsl.hpp>

#include <regex>

#include "thirdparty/spirv-reflect/spirv_reflect.h"

namespace reflect {
std::string spirv(const uint32_t *spirv_data, size_t size) {
	SpvReflectShaderModule module;
	SpvReflectResult result = spvReflectCreateShaderModule(size * sizeof(uint32_t), spirv_data, &module);
	ERR_FAIL_COND_V(result != SPV_REFLECT_RESULT_SUCCESS, "");

	Dictionary reflected_data;

	uint32_t binding_count = 0;
	result = spvReflectEnumerateDescriptorBindings(&module, &binding_count, NULL);
	ERR_FAIL_COND_V(result != SPV_REFLECT_RESULT_SUCCESS, "");
	if (binding_count > 0) {

		//Parse bindings

		Vector<SpvReflectDescriptorBinding *> bindings;
		bindings.resize(binding_count);
		result = spvReflectEnumerateDescriptorBindings(&module, &binding_count, bindings.ptrw());
		ERR_FAIL_COND_V(result != SPV_REFLECT_RESULT_SUCCESS, "");

		struct VariantBinding {
			Variant name;
			Variant bind_point;
		};

		struct Set {
			Vector<VariantBinding> bindings;
		};

		Vector<Set> sets;
		for (uint32_t b = 0; b < binding_count; b++) {
			const SpvReflectDescriptorBinding &binding = *bindings[b];
			if (sets.size() <= binding.set)
				sets.resize(binding.set + 1);

			Set &set = sets.write[binding.set];
			VariantBinding vb;
			if (binding.resource_type == SPV_REFLECT_RESOURCE_FLAG_CBV)
				vb.name = Variant(binding.type_description->type_name);
			else
				vb.name = Variant(binding.name);
			vb.bind_point = Variant(binding.binding);
			set.bindings.push_back(vb);
		}

		Array vsets;
		for (uint32_t i = 0; i < sets.size(); ++i) {
			const Set &set = sets[i];
			if (set.bindings.size() == 0)
				continue;

			Dictionary vset;
			Array vbindings;
			vset[(Variant("set"))] = Variant(i);
			for (uint32_t j = 0; j < set.bindings.size(); ++j) {
				Dictionary vbinding;
				vbinding[Variant("name")] = set.bindings[j].name;
				vbinding[Variant("bind_point")] = set.bindings[j].bind_point;
				vbindings.push_back(vbinding);
			}
			vset[Variant("bindings")] = vbindings;
			vsets.push_back(vset);
		}

		reflected_data[Variant("sets")] = vsets;
	}

	uint32_t push_constant_count = 0;
	result = spvReflectEnumeratePushConstantBlocks(&module, &push_constant_count, NULL);
	ERR_FAIL_COND_V(result != SPV_REFLECT_RESULT_SUCCESS, "");
	if (push_constant_count > 0) {
		push_constant_count = 1;
		SpvReflectBlockVariable *push_constant;
		result = spvReflectEnumeratePushConstantBlocks(&module, &push_constant_count, &push_constant);

		Dictionary vpush_constant;
		vpush_constant[Variant("name")] = push_constant->name;
		vpush_constant[Variant("size")] = push_constant->size;

		reflected_data[Variant("push_constant")] = vpush_constant;
	}

	return JSON::print(reflected_data).utf8().ptr();
}
} // namespace reflect

struct CompilationConfiguration {
	bool unroll;
	int stage;
};

bool potential_shader(const StringName file_type) {
	return file_type == "Shader" ||
		   file_type == "VisualShader" ||
		   file_type == "ShaderMaterial" ||
		   file_type == "PackedScene";
}

void ShaderExporter::export_shaders() {
	// Create the shader-folder if it does not exist
	DirAccess *da = DirAccess::open("res://");
	if (da->change_dir(".shaders") != OK) {
		Error err = da->make_dir(".shaders");
		if (err) {
			memdelete(da);
			ERR_FAIL_MSG("Failed to create 'res://.shaders' folder.");
			return;
		}
	} else {
		// If the folder exists, remove all the files in it
		da->erase_contents_recursive();
	}
	memdelete(da);

	// Iterate over all files in the project
	EditorFileSystemDirectory *root_path = EditorFileSystem::get_singleton()->get_filesystem();

	// Go through and load all resources that might have shaders in them
	// This will make sure they are caught in our custom cache.
	int num_files = root_path->get_file_count();
	for (int i = 0; i < num_files; ++i) {
		StringName file_type = root_path->get_file_type(i);
		String file_path = root_path->get_file_path(i);

		if (potential_shader(file_type)) {
			Ref<Resource> res = ResourceLoader::load(file_path);
		}
	}

	// Now all shaders should have been compiled and we have the SPIR-V
	// code for all of them.
	// Now we just need to iterate over them, generate PSSL bytecode from them
	// and save them to disk.
	auto *node = RD::get_singleton()->compiled_shader_cache.front();
	while (node != nullptr) {
		String hash = node->key();
		RenderingDevice::CompiledShaderCacheEntry entry = node->value();

		CompilationConfiguration config;
		config.unroll = false;
		config.stage = entry.stage;

		// TEMP: Store the generated PSSL as well so that we can debug errors
		char temp_pssl_file_name[256];
		sprintf(temp_pssl_file_name, "res://.shaders/%ws.pssl", hash.c_str());

		char temp_hlsl_file_name[256];
		sprintf(temp_hlsl_file_name, "res://.shaders/%ws.hlsl", hash.c_str());

		printf("Compiling %s\n", temp_pssl_file_name);

		// Relect SpirV
		std::string spirv_reflected_data = reflect::spirv(entry.data.read().ptr(), entry.size);

		// Compile to PSSL (SpirV -> HLSL -> PSSL -> Bytecode)
		std::string hlsl_source = spirv_to_hlsl(entry.data.read().ptr(), entry.size);
		std::string pssl_source = hlsl_to_pssl(hlsl_source, config);

		FileAccess *fa_tmp = FileAccess::open(temp_pssl_file_name, FileAccess::WRITE);
		fa_tmp->store_buffer((uint8_t *)pssl_source.c_str(), pssl_source.size());
		fa_tmp->close();
		memdelete(fa_tmp);

		FileAccess *fa_tmp2 = FileAccess::open(temp_hlsl_file_name, FileAccess::WRITE);
		fa_tmp2->store_buffer((uint8_t *)hlsl_source.c_str(), hlsl_source.size());
		fa_tmp2->close();
		memdelete(fa_tmp2);

		std::string pssl_bytecode = compile_pssl(pssl_source, config);

		if (pssl_bytecode.size() == 0) {
			node = node->next();
			continue;
		}

		// Generate filename for this data
		char file_name[256];
		sprintf(file_name, "res://.shaders/%ws.psslb", hash.c_str());

		// File layout: header|sets|psslb
		struct Header {
			uint32_t spirv_reflected_data_size;
		};

		const size_t file_size = sizeof(Header) + spirv_reflected_data.size() + pssl_bytecode.size();
		uint8_t *file_output = (uint8_t *)memalloc(file_size);
		Header header = { spirv_reflected_data.size() };
		memcpy(file_output, &header, sizeof(Header));
		memcpy(file_output + sizeof(Header), spirv_reflected_data.c_str(), spirv_reflected_data.size());
		memcpy(file_output + sizeof(Header) + spirv_reflected_data.size(), pssl_bytecode.c_str(), pssl_bytecode.size());

		// Write the compiled data to file
		FileAccess *fa = FileAccess::open(file_name, FileAccess::WRITE);
		fa->store_buffer(file_output, file_size);
		fa->close();

		memdelete(fa);
		memfree(file_output);

		node = node->next();
	}
}

std::string ShaderExporter::spirv_to_hlsl(const uint32_t *spirv_data, size_t size) {
	spirv_cross::CompilerHLSL hlsl(spirv_data, size);

	spirv_cross::CompilerHLSL::Options opts;
	opts.shader_model = 50;
	opts.point_size_compat = true;
	opts.point_coord_compat = true;
	hlsl.set_hlsl_options(opts);

	return hlsl.compile();
}

#define SHADER_REPLACE(from, to) output = std::regex_replace(output, std::regex(from), to)
std::string ShaderExporter::hlsl_to_pssl(const std::string &hlsl, CompilationConfiguration &config) {
	std::string output = hlsl;

	// Check if we have a call to "asfloat(uint4x4)"
	if (std::regex_search(output, std::regex("asfloat\\(uint4x4"))) {
		output = std::string("float4x4 asfloat(uint4x4 x) { union { uint4x4 i; float4x4 f; } u; u.i = x; return u.f; }\n").append(output);
	}

	SHADER_REPLACE("cbuffer", "ConstantBuffer");
	SHADER_REPLACE("SV_VertexID", "S_VERTEX_ID");
	SHADER_REPLACE("SV_Position", "S_POSITION");
	SHADER_REPLACE("SV_InstanceID", "S_INSTANCE_ID");
	SHADER_REPLACE("SV_Target", "S_TARGET_OUTPUT");
	SHADER_REPLACE("SV_IsFrontFace", "S_FRONT_FACE");
	SHADER_REPLACE("RWTexture3D", "RW_Texture3D");
	SHADER_REPLACE("RWTexture2D", "RW_Texture2D");
	SHADER_REPLACE("SampleLevel", "SampleLOD");
	SHADER_REPLACE("Buffer<", "RegularBuffer<"); // '<' is a hack since it would modify ConstantBuffer
	SHADER_REPLACE("RWByteAddressBuffer", "RW_ByteBuffer");
	SHADER_REPLACE("TextureCubeArray", "TextureCube_Array");
	SHADER_REPLACE("nointerpolation", "nointerp");
	SHADER_REPLACE("numthreads", "NUM_THREADS");
	SHADER_REPLACE("SV_DispatchThreadID", "S_DISPATCH_THREAD_ID");
	SHADER_REPLACE("SV_GroupThreadID", "S_GROUP_THREAD_ID");
	SHADER_REPLACE("groupshared", "thread_group_memory");
	SHADER_REPLACE("AllMemoryBarrier", "ThreadGroupMemoryBarrier");
	SHADER_REPLACE("GroupMemoryBarrierWithGroupSync", "ThreadGroupMemoryBarrierSync");
	SHADER_REPLACE("<unorm ", "<");

	// Remove registers
	SHADER_REPLACE(" : register\\(.*\\)", "");

	// Check if we can find any static arrays for Textures which would cause us to
	// have to unroll all loops in the shader.
	if (std::regex_search(output, std::regex("Texture2D<.*> .*\\[.*\\];"))) {
		config.unroll = true;
	}

	if (std::regex_search(output, std::regex("Texture3D<.*> .*\\[.*\\];"))) {
		config.unroll = true;
	}

	return output;
}

std::string ShaderExporter::compile_pssl(const std::string &pssl, const CompilationConfiguration &config) {
	// Get the type of stage for this file

	sce::Shader::Wave::Psslc::Options options;
	sce::Shader::Wave::Psslc::initializeOptions(&options, SCE_WAVE_API_VERSION);

	// Set the source-file to dymmy since we will supply the
	// code from memory instead of disk (through user-data)
	options.mainSourceFile = "dummy";
	options.entryFunctionName = "main";
	options.unrollAllLoops = config.unroll;
	options.userData = (void *)&pssl;

	static uint32_t suppressed_warnings[] = {
		20088, // unreferenced local variable
		20087, // unreferenced formal parameter
		8201, // ptoentially using uninitialized variabel
	};
	static uint32_t n_suppressed_warnings = sizeof(suppressed_warnings) / sizeof(suppressed_warnings[0]);

	options.suppressedWarningsCount = n_suppressed_warnings;
	options.suppressedWarnings = suppressed_warnings;

	if (config.stage == RD::ShaderStage::SHADER_STAGE_VERTEX) {
		options.targetProfile = sce::Shader::Wave::Psslc::kTargetProfileVsVs;
	} else if (config.stage == RD::ShaderStage::SHADER_STAGE_FRAGMENT) {
		options.targetProfile = sce::Shader::Wave::Psslc::kTargetProfilePs;
	} else if (config.stage == RD::ShaderStage::SHADER_STAGE_COMPUTE) {
		options.targetProfile = sce::Shader::Wave::Psslc::kTargetProfileCs;
	} else {
		ERR_FAIL_V("");
	}

	sce::Shader::Wave::Psslc::CallbackList callbacks;
	sce::Shader::Wave::Psslc::initializeCallbackList(&callbacks, sce::Shader::Wave::Psslc::kCallbackDefaultsTrivial);

	auto open_file = [](const char *fileName,
							 const sce::Shader::Wave::Psslc::SourceLocation *includedFrom,
							 const sce::Shader::Wave::Psslc::OptionsBase *compileOptions,
							 void *user_data,
							 const char **errorString) {
		auto src_file = (sce::Shader::Wave::Psslc::SourceFile *)memalloc(sizeof(sce::Shader::Wave::Psslc::SourceFile));

		std::string *pssl_code = (std::string *)user_data;

		src_file->fileName = fileName;
		src_file->size = pssl_code->size(); //pssl.size();
		src_file->text = pssl_code->c_str(); //pssl.c_str();

		return src_file;
	};

	auto release_file = [](
								const sce::Shader::Wave::Psslc::SourceFile *file,
								const sce::Shader::Wave::Psslc::OptionsBase *compileOptions,
								void *user_data) {
		memfree((void *)file);
	};

	callbacks.openFile = open_file;
	callbacks.releaseFile = release_file;

	const sce::Shader::Wave::Psslc::Output *output = sce::Shader::Wave::Psslc::run(&options, &callbacks);

	if (output->diagnosticCount > 0) {
		printf("Errors in shader\n");

		for (int i = 0; i < output->diagnosticCount; ++i) {
			if (output->diagnostics[i].level == 0) {
				printf("\t[I] ");
			} else if (output->diagnostics[i].level == 1) {
				printf("\t[W] ");
			} else if (output->diagnostics[i].level == 2) {
				printf("\t[E] ");
			}
			if (output->diagnostics[i].location) {
				printf("(%d, %d): ", output->diagnostics[i].location->lineNumber, output->diagnostics[i].location->columnNumber);
			}

			printf("[%d] %s\n", output->diagnostics[i].code, output->diagnostics[i].message);
		}
	}

	if (output->programSize == 0) {
		return "";
	}

	std::string output_data;
	output_data.reserve(output->programSize);
	output_data.append((const char *)output->programData, output->programSize * sizeof(uint8_t));
	return output_data;
}
