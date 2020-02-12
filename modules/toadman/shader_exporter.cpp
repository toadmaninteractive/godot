#include "shader_exporter.h"
#include "servers/visual/rasterizer_rd/rasterizer_storage_rd.h"
#include "servers/visual/rasterizer_rd/rasterizer_canvas_rd.h"
#include "servers/visual/rasterizer_rd/rasterizer_scene_high_end_rd.h"
#include "servers/visual/visual_server_globals.h"
#include "core/string_builder.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"

#include <spirv_hlsl.hpp>
#include <shader.h>
#include <shader/wave_psslc.h>

#include <regex>

enum ShaderType { VS = 0, HS, DS, GS, PS, CS, N_SHADER_TYPES };

struct CompilationConfiguration {
    bool unroll;
};

unsigned enable_stage(ShaderType stage)
{
    return 1 << stage;
}

ShaderExporter::ShaderExporter() {

}

ShaderExporter::~ShaderExporter() {

}

void ShaderExporter::export_shaders() {
    // Create the shader-folder if it does not exist
    DirAccess *da = DirAccess::open("res://");
    if (da->change_dir(".shaders") != OK) {
        Error err = da->make_dir(".shaders");
        if (err) {
            memdelete(da);
            ERR_FAIL_MSG("Failed to create 'res://.shaders' folder.");
        }
    }
    memdelete(da);

    RasterizerStorageRD* storage = (RasterizerStorageRD*)VSG::storage;
    RasterizerCanvasRD* canvas = (RasterizerCanvasRD*)VSG::canvas_render;
    RasterizerSceneHighEndRD* scene = (RasterizerSceneHighEndRD*)VSG::scene_render;

    // Fetch all the builtin shaders
    Vector<ShaderRD*> shaders;
    storage->get_shaders(shaders);
    canvas->get_shaders(shaders);
    scene->get_shaders(shaders);

    // Iterate through all shaders, variants and stages
    //      Variant = Unique set of defines
    //      Stage = Vertex, Fragment or Compute shader
	for (int i = 0; i < shaders.size(); ++i) {
        int num_variants = shaders[i]->all_stages.size();
        for (int j = 0; j < num_variants; ++j) {
            int num_stages = shaders[i]->all_stages[j].size();
            for (int k = 0; k < num_stages; ++k) {
                // Get the data for this variant and stage of the shader
                const RD::ShaderStageData& stage_data = shaders[i]->all_stages[j][k];
                size_t data_size = stage_data.spir_v.size() / sizeof(uint32_t);
                uint32_t* spirv_data = (uint32_t*)stage_data.spir_v.read().ptr();
                int stage_id = stage_data.shader_stage;
    
                // Compile to PSSL (SpirV -> HLSL -> PSSL)
                CompilationConfiguration config;
                config.unroll = false;
 
                std::string hlsl_source = spirv_to_hlsl(spirv_data, data_size);
                std::string pssl_source = hlsl_to_pssl(hlsl_source, config);

                // Generate filename for this data                
                String name = shaders[i]->get_name();                
                char file_name[256];
                sprintf(file_name, "res://.shaders/%ws_%d_%d.hlsl", name.ptr(), j, stage_id);

                // Write the compiled data to file
                FileAccess* fa = FileAccess::open(file_name, FileAccess::WRITE);
                fa->store_buffer((uint8_t*)pssl_source.c_str(), pssl_source.size());
                fa->close();
                memdelete(fa);
				
                // Compile the PSSL to binary format
                compile_pssl(file_name, stage_id, config);
            }   
        }
    }
}

std::string ShaderExporter::spirv_to_hlsl(uint32_t* spirv_data, size_t size) {
    spirv_cross::CompilerHLSL hlsl(spirv_data, size);

    spirv_cross::CompilerHLSL::Options opts;
    opts.shader_model = 50;
    opts.point_size_compat = true;
    opts.point_coord_compat = true;
    hlsl.set_hlsl_options(opts);

    return hlsl.compile();
}


#define SHADER_REPLACE(from, to) output = std::regex_replace(output, std::regex(from), to)
std::string ShaderExporter::hlsl_to_pssl(const std::string& hlsl, CompilationConfiguration& config) {
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
    SHADER_REPLACE("RWTexture3D", "RW_Texture3D");
    SHADER_REPLACE("RWTexture2D", "RW_Texture2D");
    SHADER_REPLACE("SampleLevel", "SampleLOD");
    SHADER_REPLACE("Buffer<", "RegularBuffer<");    // '<' is a hack since it would modify ConstantBuffer
    SHADER_REPLACE("RWByteAddressBuffer", "RW_ByteBuffer");
    SHADER_REPLACE("TextureCubeArray", "TextureCube_Array");
    SHADER_REPLACE("nointerpolation", "nointerp");
    SHADER_REPLACE("numthreads", "NUM_THREADS");
    SHADER_REPLACE("SV_DispatchThreadID", "S_DISPATCH_THREAD_ID");
    SHADER_REPLACE("<unorm ", "<");

    // Remove registers
    SHADER_REPLACE(" : register\\(.*\\)", "");

    // Check if we can find any static arrays for Textures which would cause us to
    // have to unroll all loops in the shader.
    if (std::regex_search(output, std::regex("Texture2D<.*> .*\\[.*\\];"))) {
        config.unroll = true;
    }

    return output;
}

void ShaderExporter::compile_pssl(const char* file_path, int stage, const CompilationConfiguration& config) {
    // Get the type of stage for this file

    sce::Shader::Wave::Psslc::Options options;
    sce::Shader::Wave::Psslc::initializeOptions(&options, SCE_WAVE_API_VERSION);

    options.mainSourceFile = file_path;
    options.entryFunctionName = "main";
    options.unrollAllLoops = config.unroll;

    static uint32_t suppressed_warnings[] = {
			20088, // unreferenced local variable
		};
    static uint32_t n_suppressed_warnings = sizeof(suppressed_warnings) / sizeof(suppressed_warnings[0]);

    options.suppressedWarningsCount = n_suppressed_warnings;
	options.suppressedWarnings = suppressed_warnings;

    if (stage == RD::ShaderStage::SHADER_STAGE_VERTEX) {
        options.targetProfile = sce::Shader::Wave::Psslc::kTargetProfileVsVs;
    }
    else if (stage == RD::ShaderStage::SHADER_STAGE_FRAGMENT) {
        options.targetProfile = sce::Shader::Wave::Psslc::kTargetProfilePs;
    } 
    else if (stage == RD::ShaderStage::SHADER_STAGE_COMPUTE) {
        options.targetProfile = sce::Shader::Wave::Psslc::kTargetProfileCs;
    }
    else {        
        ERR_FAIL();
    }

    sce::Shader::Wave::Psslc::CallbackList callbacks;
	sce::Shader::Wave::Psslc::initializeCallbackList(&callbacks, sce::Shader::Wave::Psslc::kCallbackDefaultsTrivial);

    auto open_file = [](const char *fileName,
			const sce::Shader::Wave::Psslc::SourceLocation *includedFrom,
			const sce::Shader::Wave::Psslc::OptionsBase *compileOptions,
			void *user_data,
			const char **errorString)
		{
            auto src_file = (sce::Shader::Wave::Psslc::SourceFile*)memalloc(sizeof(sce::Shader::Wave::Psslc::SourceFile));
            
            FileAccess* fa = FileAccess::open(fileName, FileAccess::READ);
            int len = fa->get_len();
            uint8_t* data = (uint8_t*)memalloc(len * sizeof(uint8_t));
            fa->get_buffer(data, len);
            fa->close();
            memdelete(fa);


            src_file->fileName = fileName;
            src_file->size = len;
            src_file->text = (const char*)data;

			return src_file;
		};

    auto release_file = [](
        const sce::Shader::Wave::Psslc::SourceFile *file, 
        const sce::Shader::Wave::Psslc::OptionsBase *compileOptions, 
        void *user_data)
		{
            memfree((void*)file->text);
            memfree((void*)file);
		};

    callbacks.openFile = open_file;
    callbacks.releaseFile = release_file;

    const sce::Shader::Wave::Psslc::Output *output = sce::Shader::Wave::Psslc::run(&options, &callbacks);

    if (output->diagnosticCount > 0) {
        printf("Errors in shader: %s\n", file_path);

        for (int i = 0; i < output->diagnosticCount; ++i) {
            if (output->diagnostics[i].level == 0) {
                printf("\t[I] ");
            }
            else if (output->diagnostics[i].level == 1) {
                printf("\t[W] ");
            }
            else if (output->diagnostics[i].level == 2) {
                printf("\t[E] ");
            }
            if (output->diagnostics[i].location) {
                printf("(%d, %d): ", output->diagnostics[i].location->lineNumber, output->diagnostics[i].location->columnNumber);
            }

            printf("[%d] %s\n", output->diagnostics[i].code, output->diagnostics[i].message);
        }        
    }
}
