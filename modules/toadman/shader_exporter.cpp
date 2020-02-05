#include "shader_exporter.h"
#include "servers/visual/rasterizer_rd/rasterizer_storage_rd.h"
#include "servers/visual/rasterizer_rd/rasterizer_canvas_rd.h"
#include "servers/visual/rasterizer_rd/rasterizer_scene_forward_rd.h"
#include "servers/visual/visual_server_globals.h"
#include "core/string_builder.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"

#include <spirv_hlsl.hpp>

#include <sstream>

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
    RasterizerSceneForwardRD* scene = (RasterizerSceneForwardRD*)VSG::scene_render;

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

                // Compile to PSSL (SpirV -> HLSL -> PSSL)
                std::string hlsl_source = compile_spirv_to_hlsl(spirv_data, data_size);
                std::string pssl_source = hlsl_to_pssl(hlsl_source);

                // Generate filename for this data                
                String name = shaders[i]->get_name();                
                char file_name[256];
                sprintf(file_name, "res://.shaders/%ws_%d_%d.hlsl", name.ptr(), j, k);

                // Write the compiled data to file
                FileAccess* fa = FileAccess::open(file_name, FileAccess::WRITE);
                fa->store_buffer((uint8_t*)pssl_source.c_str(), pssl_source.size());
                fa->close();
                memdelete(fa);
            }   
        }
    }
}

std::string ShaderExporter::compile_spirv_to_hlsl(uint32_t* spirv_data, size_t size) {
    spirv_cross::CompilerHLSL hlsl(spirv_data, size);

    spirv_cross::CompilerHLSL::Options opts;
    opts.shader_model = 50;
    opts.point_size_compat = true;
    opts.point_coord_compat = true;
    hlsl.set_hlsl_options(opts);

    return hlsl.compile();
}

std::string ShaderExporter::hlsl_to_pssl(const std::string& hlsl) {

    // Extract each line from the input, then transform each
    // line, one by one.
    // When done, compose the source from all the seperate lines
    // again.
    std::string output;
    std::istringstream stream(hlsl);
    std::string line;
    while (std::getline(stream, line)) {
        // Do some formatting of line here...

        output.append(line);
        output.append("\n");
    }

    return output;
}
