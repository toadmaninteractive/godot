#include "shader_exporter.h"

#include "core/io/json.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "core/string_builder.h"

#include "editor/editor_file_system.h"

#include "servers/rendering/rasterizer_rd/rasterizer_canvas_rd.h"
#include "servers/rendering/rasterizer_rd/rasterizer_scene_high_end_rd.h"
#include "servers/rendering/rasterizer_rd/rasterizer_storage_rd.h"
#include "servers/rendering/rendering_server_globals.h"

#include "thirdparty/spirv-reflect/spirv_reflect.h"

#include <gnm/constants.h>
#include <shader.h>
#include <shader/binary.h>
#include <shader/wave_psslc.h>

#include <spirv_hlsl.hpp>

#include <regex>

#if defined(MINGW_ENABLED) || defined(_MSC_VER)
#define sprintf sprintf_s
#endif

struct CompilationConfiguration {
	bool unroll;
	int stage;
};

struct PsslPermutation {
	uint32_t id;
	Vector<sce::Gnm::PsTargetOutputMode> formats;
};

struct PsslCompileResult {
	uint32_t permutation;
	std::string program;
	std::string sdb_ext;
	std::string sdb;
};

std::string spirv_to_hlsl(const Vector<uint8_t> &spirv);
std::string hlsl_to_pssl(const std::string &hlsl, CompilationConfiguration &config);
PsslCompileResult compile_pssl(const std::string &pssl, const CompilationConfiguration &config);
std::string build_metadata(const Vector<uint8_t> &spirv, const std::string &pssl_bytecode);
Error erase_files_recursive(DirAccess *da, Vector<String> &exclude_pattern);

bool potential_shader(const StringName file_type) {
	return file_type == "Shader" ||
		   file_type == "VisualShader" ||
		   file_type == "ShaderMaterial" ||
		   file_type == "PackedScene";
}

void generate_permutations_recursive(
		const uint32_t num_output_modes, const sce::Gnm::PsTargetOutputMode *output_modes,
		const uint32_t num_output_targets, const uint32_t output_target, Vector<PsslPermutation> &permutations, PsslPermutation &permutation) {
	if (output_target == num_output_targets) {
		uint32_t id = 0;
		for (uint32_t i = 0; i < num_output_targets; ++i) {
			uint32_t id_mask = permutation.formats[i];
			id_mask <<= i * 4; // 4 bit per target
			id |= id_mask;
		}
		permutation.id = id;
		permutations.push_back(permutation);
		return;
	}

	permutation.formats.resize(num_output_targets);

	for (uint32_t i = 0; i < num_output_modes; ++i) {
		permutation.formats.write[output_target] = output_modes[i];
		generate_permutations_recursive(num_output_modes, output_modes, num_output_targets, output_target + 1, permutations, permutation);
	}
};

void ShaderExporter::export_shaders() {
	// Create the shader-folder if it does not exist
	DirAccess *da = DirAccess::open("res://");
	da->make_dir_recursive(".shaders/debug");
	da->change_dir(".shaders");

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

	Vector<String> processed_shaders;

	// Now all shaders should have been compiled and we have the SPIR-V
	// code for all of them.
	// Now we just need to iterate over them, generate PSSL bytecode from them
	// and save them to disk.
	auto *node = RD::get_singleton()->shader_compile_cache.front();
	while (node != nullptr) {
		String hash = node->key();
		const RenderingDevice::ShaderCompileCacheEntry &entry = node->value();

		processed_shaders.push_back(hash);

		// Generate filename for shader program
		char file_name[256];
		sprintf(file_name, "res://.shaders/%ws.sp", hash.c_str());

		// Check if up-to-date
		if (FileAccess::exists(String(file_name))) {
			node = node->next();
			continue;
		}

		CompilationConfiguration config;
		config.unroll = false;
		config.stage = entry.stage;

		printf("Compiling %s\n", hash.utf8().ptr());

		// Compile to PSSL (SpirV -> HLSL -> PSSL -> Bytecode)
		std::string hlsl_source = spirv_to_hlsl(entry.program);
		std::string pssl_source = hlsl_to_pssl(hlsl_source, config);

		// Write shader sources (for debug)

		{
			char debug_file_name[256];
			{
				sprintf(debug_file_name, "res://.shaders/debug/%ws.glsl", hash.c_str());
				FileAccess *fa = FileAccess::open(debug_file_name, FileAccess::WRITE);
				fa->store_buffer((uint8_t *)entry.source_code.utf8().ptr(), entry.source_code.size());
				fa->close();
				memdelete(fa);
			}

			{
				sprintf(debug_file_name, "res://.shaders/debug/%ws.hlsl", hash.c_str());
				FileAccess *fa = FileAccess::open(debug_file_name, FileAccess::WRITE);
				fa->store_buffer((uint8_t *)hlsl_source.c_str(), hlsl_source.size());
				fa->close();
				memdelete(fa);
			}

			{
				sprintf(debug_file_name, "res://.shaders/debug/%ws.pssl", hash.c_str());
				FileAccess *fa = FileAccess::open(debug_file_name, FileAccess::WRITE);
				fa->store_buffer((uint8_t *)pssl_source.c_str(), pssl_source.size());
				fa->close();
				memdelete(fa);
			}
		}

		Vector<PsslCompileResult> pssl_compile_results;

		if (config.stage == RD::ShaderStage::SHADER_STAGE_FRAGMENT) {
			/*
				Brute force approach. Generate all potential permutations of output-target-format combinations.
				Until we introduce shader hinting of how output targets will be bound. Then only those -
				shaders would have to generate permutations.

				Total permutations: num_output_formats^num_output_targets

				Some Godot shaders support a variety of output-target-formats for a specific target-slot.
				These targets are bound in runtime. This is fully supported by Vulkan/Spirv.
				Example:
					Blur shader has a copy mode.
					This shader sometimes copies to a R32 target, other times a R8G8B8A8 target.

				This is not fully supported by PSSL shader. Assembly will change depending on output-target-formats.
				Therefore not all targets can be bound to shader compiled with FMT_FP16_ABGR (default).
				Example:
					Binding a R32 target to shader compiled as FMT_FP16_ABGR would fail validation on submit.
				
				There are some edge cases (might not have encountered them all):
				Example:
					UNORM and SNORM channel type is only supported when using a 16bit channel size
			*/

			// reflect output targets

			uint32_t output_target_count = 0;
			{
				SpvReflectShaderModule module;
				SpvReflectResult result = spvReflectCreateShaderModule(entry.program.size(), entry.program.ptr(), &module);
				ERR_FAIL_COND(result != SPV_REFLECT_RESULT_SUCCESS);
				Vector<SpvReflectInterfaceVariable *> output_variables;
				spvReflectEnumerateOutputVariables(&module, &output_target_count, NULL);
				spvReflectDestroyShaderModule(&module);
			}

			if (output_target_count) {
				static const sce::Gnm::PsTargetOutputMode output_modes[] = {
					sce::Gnm::kPsTargetOutputModeA16B16G16R16Float, // 1.0 quad/clock
					sce::Gnm::kPsTargetOutputModeR32, // 1.0 quad/clock
					//sce::Gnm::kPsTargetOutputModeA32B32G32R32, // 0.5 quad/clock (performance hit)
				};

				static const char *output_format_strings[] = {
					"FMT_FP16_ABGR", // kPsTargetOutputModeNoExports = 0,
					"FMT_32_R", // kPsTargetOutputModeR32 = 1,
					"FMT_32_GR", // kPsTargetOutputModeG32R32 = 2,
					"FMT_32_AR", // kPsTargetOutputModeA32R32 = 3,
					"FMT_FP16_ABGR", // kPsTargetOutputModeA16B16G16R16Float = 4, - default
					"FMT_UNORM16_ABGR", // kPsTargetOutputModeA16B16G16R16Unorm = 5,
					"FMT_SNORM16_ABGR", // kPsTargetOutputModeA16B16G16R16Snorm = 6,
					"FMT_UINT16_ABGR", // kPsTargetOutputModeA16B16G16R16Uint = 7,
					"FMT_SINT16_ABGR", // kPsTargetOutputModeA16B16G16R16Sint = 8,
					"FMT_32_ABGR", // kPsTargetOutputModeA32B32G32R32 = 9
				};

				const uint32_t num_output_modes = sizeof(output_modes) / sizeof(output_modes[0]);

				PsslPermutation stage;
				Vector<PsslPermutation> permutations;
				generate_permutations_recursive(num_output_modes, output_modes, output_target_count, 0, permutations, stage);

				char pragma_buffer[128];
				const char *pragma_pattern = "#pragma PSSL_target_output_format(target %lu %s)\n";
				for (uint32_t i = 0; i < permutations.size(); ++i) {
					std::string pssl_copy = pssl_source;
					const PsslPermutation &permutation = permutations[i];
					for (uint32_t j = 0; j < permutation.formats.size(); ++j) {
						sprintf_s(pragma_buffer, 128, pragma_pattern, j, output_format_strings[(uint32_t)permutation.formats[j]]);
						pssl_copy.insert(0, pragma_buffer);
					}

					PsslCompileResult result = compile_pssl(pssl_copy, config);
					if (result.program.size() == 0) {
						pssl_compile_results.clear();
						break;
					}
					result.permutation = permutation.id;
					pssl_compile_results.push_back(result);
				}
			} else {
				PsslCompileResult result = compile_pssl(pssl_source, config);
				if (result.program.size())
					pssl_compile_results.push_back(result);
			}
		} else {
			PsslCompileResult result = compile_pssl(pssl_source, config);
			if (result.program.size())
				pssl_compile_results.push_back(result);
		}

		if (pssl_compile_results.size() == 0) {
			node = node->next();
			continue;
		}

		// Write shader program

		{
			std::string metadata = build_metadata(entry.program, pssl_compile_results[0].program);
			if (metadata.size() == 0) {
				node = node->next();
				continue;
			}

			// File layout
			struct ShaderHeader {
				struct MetadataHeader {
					uint32_t size;
					uint32_t offset;
				};

				struct ProgramHeader {
					uint32_t size;
					uint32_t permutation;
					uint32_t offset;
				};

				MetadataHeader metadata_header;
				uint32_t num_program_headers;
				// program headers
				// metadata
				// program data
			};

			const uint32_t num_programs = pssl_compile_results.size();
			const uint32_t header_size = sizeof(ShaderHeader);
			const uint32_t program_header_size = sizeof(ShaderHeader::ProgramHeader);
			const uint32_t program_headers_size = program_header_size * num_programs;
			const uint32_t metadata_size = metadata.size();

			const uint32_t metadata_offset = header_size + program_headers_size;
			const uint32_t program_data_offset = metadata_offset + metadata_size;

			uint32_t total_programs_size = 0;
			for (uint32_t i = 0; i < num_programs; ++i)
				total_programs_size += pssl_compile_results[i].program.size();

			const size_t file_size = header_size + program_headers_size + metadata_size + total_programs_size;
			uint8_t *file_output = (uint8_t *)memalloc(file_size);

			// shader header

			ShaderHeader header;
			header.metadata_header.size = metadata_size;
			header.metadata_header.offset = metadata_offset;
			header.num_program_headers = num_programs;
			memcpy(file_output, &header, header_size);

			// metadata

			memcpy(file_output + metadata_offset, metadata.c_str(), metadata_size);

			// program-header and program-data

			uint32_t program_offset = 0;
			ShaderHeader::ProgramHeader program_header;
			for (uint32_t i = 0; i < num_programs; ++i) {
				program_header.size = pssl_compile_results[i].program.size();
				program_header.permutation = pssl_compile_results[i].permutation;
				program_header.offset = program_data_offset + program_offset;

				memcpy(file_output + header_size + (i * program_header_size), &program_header, program_header_size);
				memcpy(file_output + program_header.offset, pssl_compile_results[i].program.c_str(), program_header.size);
				program_offset += program_header.size;
			}

			// Write shader program to file
			FileAccess *fa = FileAccess::open(file_name, FileAccess::WRITE);
			fa->store_buffer(file_output, file_size);
			fa->close();

			memdelete(fa);
			memfree(file_output);
		}

		// Write shader debug database

		{
			for (uint32_t i = 0; i < pssl_compile_results.size(); ++i) {
				const PsslCompileResult &result = pssl_compile_results[i];
				const std::string &pssl_sdb = result.sdb;
				const std::string &pssl_sdb_ext = result.sdb_ext;

				sprintf(file_name, "res://.shaders/%ws%s", hash.c_str(), pssl_sdb_ext.c_str());

				FileAccess *fa = FileAccess::open(file_name, FileAccess::WRITE);
				fa->store_buffer((uint8_t *)pssl_sdb.c_str(), pssl_sdb.size());
				fa->close();

				memdelete(fa);
			}
		}

		node = node->next();
	}

	// Erase all files that was not processed during export
	erase_files_recursive(da, processed_shaders);
	memdelete(da);
}

std::string spirv_to_hlsl(const Vector<uint8_t> &spirv) {
	uint32_t *spirv_data = (uint32_t *)spirv.ptr();
	uint32_t spirv_size = spirv.size() / sizeof(uint32_t);

	spirv_cross::CompilerHLSL hlsl(spirv_data, spirv_size);

	spirv_cross::CompilerHLSL::Options opts;
	opts.shader_model = 50;
	opts.point_size_compat = true;
	opts.point_coord_compat = true;
	hlsl.set_hlsl_options(opts);

	std::string result;
	try {
		result = hlsl.compile();
	} catch (std::exception &e) {
		printf("SPIRV-Cross HLSL Compiler:\n");
		printf("\t[E] %s\n", e.what());
	}
	return result;
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
	SHADER_REPLACE("ByteAddressBuffer", "ByteBuffer");
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
	res.permutation = 0;

	// Get the type of stage for this file

	sce::Shader::Wave::Psslc::Options options;
	sce::Shader::Wave::Psslc::initializeOptions(&options, SCE_WAVE_API_VERSION);

	// Set the source-file to dummy since we will supply the
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
		printf("PSSL Compiler:\n");

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

std::string build_metadata(const Vector<uint8_t> &spirv, const std::string &pssl_bytecode) {
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

		SpvReflectResult result = spvReflectCreateShaderModule(spirv.size(), spirv.ptr(), &spirv_reflection.module);
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

Error erase_files_recursive(DirAccess *da, Vector<String> &exclude_pattern) {
	List<String> dirs;
	List<String> files;

	da->list_dir_begin();
	String n = da->get_next();
	while (n != String()) {

		if (n != "." && n != "..") {

			if (da->current_is_dir())
				dirs.push_back(n);
			else {
				bool include = true;
				for (uint32_t i = 0; i < exclude_pattern.size(); ++i) {
					if (n.find(exclude_pattern[i]) != -1) {
						include = false;
						break;
					}
				}
				if (include)
					files.push_back(n);
			}
		}

		n = da->get_next();
	}

	da->list_dir_end();

	for (List<String>::Element *E = dirs.front(); E; E = E->next()) {

		Error err = da->change_dir(E->get());
		if (err == OK) {

			err = erase_files_recursive(da, exclude_pattern);
			if (err) {
				da->change_dir("..");
				return err;
			}
			err = da->change_dir("..");
			if (err) {
				return err;
			}
		} else {
			return err;
		}
	}

	for (List<String>::Element *E = files.front(); E; E = E->next()) {

		Error err = da->remove(da->get_current_dir().plus_file(E->get()));
		if (err) {
			return err;
		}
	}

	return OK;
}