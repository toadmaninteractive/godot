#pragma once

#include <string>

struct CompilationConfiguration;
class ShaderExporter {
public:
    ShaderExporter();
    ~ShaderExporter();

    void export_shaders();

private:
    std::string spirv_to_hlsl(const uint32_t* spirv_data, size_t size);
    std::string hlsl_to_pssl(const std::string& hlsl, CompilationConfiguration& config);
    std::string compile_pssl(const std::string& pssl, const CompilationConfiguration& config);
};