#pragma once

#include <string>

class ShaderExporter {
public:
    ShaderExporter();
    ~ShaderExporter();

    void export_shaders();

private:
    std::string compile_spirv_to_hlsl(uint32_t* spirv_data, size_t size);
    std::string hlsl_to_pssl(const std::string& hlsl);
};