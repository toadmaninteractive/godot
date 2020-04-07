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

#include "thirdparty/spirv-reflect/spirv_reflect.h"

#include <shader.h>
#include <shader/binary.h>
#include <shader/wave_psslc.h>

#include <spirv_hlsl.hpp>

#include <regex>

struct CompilationConfiguration {
	bool unroll;
	int stage;
};

struct PsslCompileResult {
	std::string program;
	std::string sdb_ext;
	std::string sdb;
};

std::string spirv_to_hlsl(const uint32_t *spirv_data, size_t size);
std::string hlsl_to_pssl(const std::string &hlsl, CompilationConfiguration &config);
PsslCompileResult compile_pssl(const std::string &pssl, const CompilationConfiguration &config);
std::string build_metadata(const uint32_t *spirv_data, size_t spirv_size, const std::string &pssl_bytecode);

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

		PsslCompileResult pssl_compile_res = compile_pssl(pssl_source, config);
		if (pssl_compile_res.program.size() == 0) {
			node = node->next();
			continue;
		}

		{
			std::string &pssl_bytecode = pssl_compile_res.program;

			std::string metadata = build_metadata(entry.data.read().ptr(), entry.size, pssl_bytecode);
			if (metadata.size() == 0) {
				node = node->next();
				continue;
			}

			// Generate filename for shader program
			char file_name[256];
			sprintf(file_name, "res://.shaders/%ws.sp", hash.c_str());

			// File layout: header|metadata|sb
			struct Header {
				uint32_t metadata_size;
			};

			const size_t file_size = sizeof(Header) + metadata.size() + pssl_bytecode.size();
			uint8_t *file_output = (uint8_t *)memalloc(file_size);
			Header header = { metadata.size() };
			memcpy(file_output, &header, sizeof(Header));
			memcpy(file_output + sizeof(Header), metadata.c_str(), metadata.size());
			memcpy(file_output + sizeof(Header) + metadata.size(), pssl_bytecode.c_str(), pssl_bytecode.size());

			// Write shader program to file
			FileAccess *fa = FileAccess::open(file_name, FileAccess::WRITE);
			fa->store_buffer(file_output, file_size);
			fa->close();

			memdelete(fa);
			memfree(file_output);
		}

		{
			std::string &pssl_sdb = pssl_compile_res.sdb;
			std::string &pssl_sdb_ext = pssl_compile_res.sdb_ext;

			char file_name[256];
			sprintf(file_name, "res://.shaders/%ws%s", hash.c_str(), pssl_sdb_ext.c_str());

			// Write SDB
			FileAccess *fa = FileAccess::open(file_name, FileAccess::WRITE);
			fa->store_buffer((uint8_t *)pssl_sdb.c_str(), pssl_sdb.size());
			fa->close();

			memdelete(fa);
		}

		node = node->next();
	}
}

std::string spirv_to_hlsl(const uint32_t *spirv_data, size_t size) {
	spirv_cross::CompilerHLSL hlsl(spirv_data, size);

	spirv_cross::CompilerHLSL::Options opts;
	opts.shader_model = 50;
	opts.point_size_compat = true;
	opts.point_coord_compat = true;
	hlsl.set_hlsl_options(opts);

	return hlsl.compile();
}

#define SHADER_REPLACE(from, to) output = std::regex_replace(output, std::regex(from), to)
std::string hlsl_to_pssl(const std::string &hlsl, CompilationConfiguration &config) {
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

PsslCompileResult compile_pssl(const std::string &pssl, const CompilationConfiguration &config) {
	PsslCompileResult res;

	// Get the type of stage for this file

	sce::Shader::Wave::Psslc::Options options;
	sce::Shader::Wave::Psslc::initializeOptions(&options, SCE_WAVE_API_VERSION);

	// Set the source-file to dymmy since we will supply the
	// code from memory instead of disk (through user-data)
	options.mainSourceFile = "dummy";
	options.entryFunctionName = "main";
	options.unrollAllLoops = config.unroll;
	options.userData = (void *)&pssl;
	options.sdbCache = 1;

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
		ERR_FAIL_V(res);
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

	res.program.append((const char *)output->programData, output->programSize);
	res.sdb.append((const char *)output->sdbData, output->sdbDataSize);
	if (output->sdbExt)
		res.sdb_ext.append((const char *)output->sdbExt, strlen(output->sdbExt));

	sce::Shader::Wave::Psslc::destroyOutput(output);

	return res;
}

std::string build_metadata(const uint32_t *spirv_data, size_t spirv_size, const std::string &pssl_bytecode) {

	struct SpirvReflection {
		SpvReflectShaderModule module;
		Vector<SpvReflectDescriptorBinding *> descriptor_bindings;
		SpvReflectBlockVariable *push_constant;
	};

	struct PSSLReflection {
		struct Resource {
			String name;
			uint32_t bind_point;
			uint32_t size; // used by buffers
		};

		Vector<Resource> constant_buffers;
		Vector<Resource> resources;
		Vector<Resource> rw_resources;
		Vector<Resource> samplers;

		Vector<uint8_t> semantic_indices; // semantic_indices[semantic_index] = semantic_map_index
		Vector<uint32_t> semantic_map; // semantic_map[semantic_map_index] = resource_index
	};

	SpirvReflection spirv_reflection;
	{
		spirv_reflection.push_constant = nullptr;

		SpvReflectResult result = spvReflectCreateShaderModule(spirv_size * sizeof(uint32_t), spirv_data, &spirv_reflection.module);
		ERR_FAIL_COND_V(result != SPV_REFLECT_RESULT_SUCCESS, "");

		uint32_t descriptor_bindings_count = 0;
		result = spvReflectEnumerateDescriptorBindings(&spirv_reflection.module, &descriptor_bindings_count, NULL);
		ERR_FAIL_COND_V(result != SPV_REFLECT_RESULT_SUCCESS, "");
		if (descriptor_bindings_count > 0) {
			spirv_reflection.descriptor_bindings.resize(descriptor_bindings_count);
			result = spvReflectEnumerateDescriptorBindings(&spirv_reflection.module, &descriptor_bindings_count, spirv_reflection.descriptor_bindings.ptrw());
			ERR_FAIL_COND_V(result != SPV_REFLECT_RESULT_SUCCESS, "");
		}

		uint32_t push_constant_count = 0;
		result = spvReflectEnumeratePushConstantBlocks(&spirv_reflection.module, &push_constant_count, NULL);
		ERR_FAIL_COND_V(result != SPV_REFLECT_RESULT_SUCCESS, "");
		if (push_constant_count > 0) {
			push_constant_count = 1;
			result = spvReflectEnumeratePushConstantBlocks(&spirv_reflection.module, &push_constant_count, &spirv_reflection.push_constant);
		}
	}

	PSSLReflection pssl_reflection;
	{
		sce::Shader::Binary::Program pssl_program;
		sce::Shader::Binary::PsslStatus ret = pssl_program.loadFromMemory(pssl_bytecode.c_str(), pssl_bytecode.size());
		ERR_FAIL_COND_V(ret != sce::Shader::Binary::kStatusOk, "");

		for (uint32_t b = 0; b < pssl_program.m_numBuffers; ++b) {
			const sce::Shader::Binary::Buffer &buffer = pssl_program.m_buffers[b];
			const sce::Shader::Binary::PsslInternalBufferType internal_type = (sce::Shader::Binary::PsslInternalBufferType)buffer.m_internalType;

			PSSLReflection::Resource resource;
			resource.name = buffer.getName();
			resource.bind_point = buffer.m_resourceIndex;
			resource.size = buffer.m_strideSize;

			const bool unsupported_internal_type = internal_type != sce::Shader::Binary::kInternalBufferTypeCbuffer &&
												   internal_type != sce::Shader::Binary::kInternalBufferTypeTextureSampler &&
												   internal_type != sce::Shader::Binary::kInternalBufferTypeUav &&
												   internal_type != sce::Shader::Binary::kInternalBufferTypeSrv;
			ERR_FAIL_COND_V_MSG(unsupported_internal_type, "", "Unsupported type " + String(sce::Shader::Binary::getPsslInternalBufferTypeString(internal_type)));

			if (internal_type == sce::Shader::Binary::kInternalBufferTypeSrv ||
					internal_type == sce::Shader::Binary::kInternalBufferTypeTextureSampler)
				pssl_reflection.resources.push_back(resource);
			else if (internal_type == sce::Shader::Binary::kInternalBufferTypeUav)
				pssl_reflection.rw_resources.push_back(resource);
			else
				pssl_reflection.constant_buffers.push_back(resource);
		}

		for (uint32_t ss = 0; ss < pssl_program.m_numSamplerStates; ++ss) {
			const sce::Shader::Binary::SamplerState &sampler_state = pssl_program.m_samplerStates[ss];

			PSSLReflection::Resource resource;
			resource.name = sampler_state.getName();
			resource.bind_point = sampler_state.m_resourceIndex;
			resource.size = 0;
			pssl_reflection.samplers.push_back(resource);
		}

		pssl_reflection.semantic_map.resize(pssl_program.m_numInputAttributes);
		memset(pssl_reflection.semantic_map.ptrw(), UINT32_MAX, sizeof(uint32_t) * pssl_program.m_numInputAttributes);
		for (uint8_t a = 0; a < pssl_program.m_numInputAttributes; ++a) {
			const sce::Shader::Binary::Attribute &attribute = pssl_program.m_inputAttributes[a];
			const sce::Shader::Binary::PsslSemantic pssl_semantic = (sce::Shader::Binary::PsslSemantic)attribute.m_psslSemantic;

			// filter system semantics
			if ((pssl_semantic > sce::Shader::Binary::kSemanticUserDefined && pssl_semantic < sce::Shader::Binary::kSemanticSpriteCoord) ||
					pssl_semantic == sce::Shader::Binary::kSemanticImplicit)
				continue;

			// assume all semantics after filtering is TEXCOORD[0...31]
			const String semantic_name = attribute.getSemanticName();
			ERR_FAIL_COND_V(semantic_name.find("TEXCOORD") == -1, "");

			if (attribute.m_semanticIndex >= pssl_reflection.semantic_indices.size()) {
				const uint32_t old_size = pssl_reflection.semantic_indices.size();
				pssl_reflection.semantic_indices.resize(attribute.m_semanticIndex + 1);
				memset(pssl_reflection.semantic_indices.ptrw() + old_size, 0xFF, sizeof(uint8_t) * (pssl_reflection.semantic_indices.size() - old_size));
			}

			pssl_reflection.semantic_indices.write[attribute.m_semanticIndex] = a;
			pssl_reflection.semantic_map.write[a] = attribute.m_resourceIndex;
		}
	}

	// generate mapping between spirv bindings and pssl program resources

	Vector<uint32_t> constant_buffer_mapping;
	Vector<uint32_t> resource_mapping;
	Vector<uint32_t> rw_resource_mapping;
	Vector<uint32_t> sampler_mapping;

	auto try_map_resource = [](const String name, const uint32_t id, const SpvReflectDescriptorBinding &descriptor_binding, const Vector<PSSLReflection::Resource> &pssl_resources, Vector<uint32_t> &mapping) -> bool {
		static const RenderingDevice::UniformType uniform_types[] = {
			RenderingDevice::UNIFORM_TYPE_SAMPLER, // SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER
			RenderingDevice::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, // SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
			RenderingDevice::UNIFORM_TYPE_TEXTURE, // SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE
			RenderingDevice::UNIFORM_TYPE_IMAGE, // SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE
			RenderingDevice::UNIFORM_TYPE_TEXTURE_BUFFER, // SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER
			RenderingDevice::UNIFORM_TYPE_IMAGE_BUFFER, // SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER
			RenderingDevice::UNIFORM_TYPE_UNIFORM_BUFFER, // SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER
			RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER, // SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER
			RenderingDevice::UNIFORM_TYPE_MAX, // SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
			RenderingDevice::UNIFORM_TYPE_MAX, // SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC
			RenderingDevice::UNIFORM_TYPE_INPUT_ATTACHMENT, // SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT
		};
		RenderingDevice::UniformType uniform_type = uniform_types[descriptor_binding.descriptor_type];
		ERR_FAIL_COND_V(uniform_type == RenderingDevice::UNIFORM_TYPE_MAX, false);

		for (uint32_t i = 0; i < pssl_resources.size(); ++i) {
			const PSSLReflection::Resource &pssl_resource = pssl_resources[i];
			if (name == pssl_resource.name) {
				if (pssl_resource.bind_point >= mapping.size())
					mapping.resize(pssl_resource.bind_point + 1);

				mapping.write[pssl_resource.bind_point] =
						(descriptor_binding.set << 28) |
						((uint32_t)uniform_type << 24) |
						(descriptor_binding.binding << 20) |
						id;
				return true;
			};
		}
		return false;
	};

	for (uint32_t db = 0; db < spirv_reflection.descriptor_bindings.size(); ++db) {
		const SpvReflectDescriptorBinding &descriptor_binding = *spirv_reflection.descriptor_bindings[db];

		if (descriptor_binding.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
			// constant buffer
			ERR_FAIL_COND_V(descriptor_binding.count != 1, "");
			try_map_resource(descriptor_binding.type_description->type_name, 0, descriptor_binding, pssl_reflection.constant_buffers, constant_buffer_mapping);
		} else if (descriptor_binding.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
				   descriptor_binding.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
				   descriptor_binding.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER) {
			// buffer
			ERR_FAIL_COND_V(descriptor_binding.count != 1, "");
			try_map_resource(descriptor_binding.name, 0, descriptor_binding, pssl_reflection.resources, resource_mapping);
			try_map_resource(descriptor_binding.name, 0, descriptor_binding, pssl_reflection.rw_resources, rw_resource_mapping);
		} else if (descriptor_binding.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
				   descriptor_binding.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
			// texture
			const bool is_array = descriptor_binding.count > 1;
			char id_buffer[4];
			for (uint32_t i = 0; i < descriptor_binding.count; ++i) {
				String name = descriptor_binding.name;
				if (is_array) {
					sprintf(id_buffer, "%d", i);
					name = name + "[" + id_buffer + "]";
				}
				try_map_resource(name, i, descriptor_binding, pssl_reflection.resources, resource_mapping);
				try_map_resource(name, i, descriptor_binding, pssl_reflection.rw_resources, rw_resource_mapping);
			}
		} else if (descriptor_binding.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
			// 0: sampler 1: texture
			ERR_FAIL_COND_V(descriptor_binding.count != 1, "");
			const String sampler_name = "_" + String(descriptor_binding.name) + "_sampler";
			try_map_resource(sampler_name, 0, descriptor_binding, pssl_reflection.samplers, sampler_mapping);
			try_map_resource(descriptor_binding.name, 1, descriptor_binding, pssl_reflection.resources, resource_mapping);
			try_map_resource(descriptor_binding.name, 1, descriptor_binding, pssl_reflection.rw_resources, rw_resource_mapping);
		} else if (descriptor_binding.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER) {
			// sampler
			const bool is_array = descriptor_binding.count > 1;
			char id_buffer[4];
			for (uint32_t i = 0; i < descriptor_binding.count; ++i) {
				String name = descriptor_binding.name;
				if (is_array) {
					sprintf(id_buffer, "%d", i);
					name = name + "[" + id_buffer + "]";
				}
				try_map_resource(name, i, descriptor_binding, pssl_reflection.samplers, sampler_mapping);
			}
		} else {
			ERR_FAIL_V_MSG(false, "Unsupported descriptor_type");
		}
	}

	uint32_t push_constant = UINT32_MAX;

	if (spirv_reflection.push_constant) {
		const String push_constant_name = spirv_reflection.push_constant->name;
		for (uint32_t i = 0; i < pssl_reflection.constant_buffers.size(); ++i) {
			const PSSLReflection::Resource &pssl_resource = pssl_reflection.constant_buffers[i];
			if (push_constant_name == pssl_resource.name) {
				ERR_FAIL_COND_V_MSG(spirv_reflection.push_constant->size != pssl_resource.size, "", "Push constant struct-size mismatch between SpirV and PSSL");

				if (pssl_resource.bind_point >= constant_buffer_mapping.size())
					constant_buffer_mapping.resize(pssl_resource.bind_point + 1);

				push_constant = (pssl_resource.bind_point << 28) | pssl_resource.size;
				constant_buffer_mapping.write[pssl_resource.bind_point] = 0; // push constant is a special case
				break;
			}
		}
	}

	spvReflectDestroyShaderModule(&spirv_reflection.module);

	ERR_FAIL_COND_V(pssl_reflection.constant_buffers.size() != constant_buffer_mapping.size(), "");
	ERR_FAIL_COND_V(pssl_reflection.resources.size() != resource_mapping.size(), "");
	ERR_FAIL_COND_V(pssl_reflection.rw_resources.size() != rw_resource_mapping.size(), "");
	ERR_FAIL_COND_V(pssl_reflection.samplers.size() != sampler_mapping.size(), "");

	struct Metadata {
		uint32_t push_constant;
		uint32_t num_constant_buffers;
		uint32_t *constant_buffer_mapping;
		uint32_t num_resources;
		uint32_t *resource_mapping;
		uint32_t num_rw_resources;
		uint32_t *rw_resource_mapping;
		uint32_t num_samplers;
		uint32_t *sampler_mapping;

		uint32_t num_semantic_map;
		uint32_t *semantic_map;
		uint32_t num_semantic_indices;
		uint8_t *semantic_indices;
	};

	Metadata metadata;
	metadata.num_constant_buffers = constant_buffer_mapping.size();
	metadata.constant_buffer_mapping = constant_buffer_mapping.ptrw();

	metadata.num_resources = resource_mapping.size();
	metadata.resource_mapping = resource_mapping.ptrw();

	metadata.num_rw_resources = rw_resource_mapping.size();
	metadata.rw_resource_mapping = rw_resource_mapping.ptrw();

	metadata.num_samplers = sampler_mapping.size();
	metadata.sampler_mapping = sampler_mapping.ptrw();

	metadata.num_semantic_indices = pssl_reflection.semantic_indices.size();
	metadata.semantic_indices = pssl_reflection.semantic_indices.ptrw();

	metadata.num_semantic_map = pssl_reflection.semantic_map.size();
	metadata.semantic_map = pssl_reflection.semantic_map.ptrw();

	metadata.push_constant = push_constant;

	// serialize metadata
	Vector<uint8_t> blob;
	blob.resize(2048 * sizeof(uint32_t));
	{
		uint32_t *ptr32 = (uint32_t *)blob.ptrw();
		*ptr32 = metadata.push_constant;
		++ptr32;
		*ptr32 = metadata.num_constant_buffers;
		++ptr32;
		for (uint32_t i = 0; i < metadata.num_constant_buffers; ++i) {
			*ptr32 = metadata.constant_buffer_mapping[i];
			++ptr32;
		}
		*ptr32 = metadata.num_resources;
		++ptr32;
		for (uint32_t i = 0; i < metadata.num_resources; ++i) {
			*ptr32 = metadata.resource_mapping[i];
			++ptr32;
		}
		*ptr32 = metadata.num_rw_resources;
		++ptr32;
		for (uint32_t i = 0; i < metadata.num_rw_resources; ++i) {
			*ptr32 = metadata.rw_resource_mapping[i];
			++ptr32;
		}
		*ptr32 = metadata.num_samplers;
		++ptr32;
		for (uint32_t i = 0; i < metadata.num_samplers; ++i) {
			*ptr32 = metadata.sampler_mapping[i];
			++ptr32;
		}
		*ptr32 = metadata.num_semantic_map;
		++ptr32;
		for (uint32_t i = 0; i < metadata.num_semantic_map; ++i) {
			*ptr32 = metadata.semantic_map[i];
			++ptr32;
		}
		*ptr32 = metadata.num_semantic_indices;
		++ptr32;
		uint8_t *ptr8 = (uint8_t *)ptr32;
		for (uint32_t i = 0; i < metadata.num_semantic_indices; ++i) {
			*ptr8 = metadata.semantic_indices[i];
			++ptr8;
		}
		const uint32_t total_size = ptr8 - blob.ptrw();
		blob.resize(total_size);
	}

	std::string metadata_string;
	metadata_string.append((const char *)blob.ptrw(), blob.size());

	return metadata_string;
}
