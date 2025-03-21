#pragma once
#include "axmol.h"

#include <vector>
#include <string>
#include <chrono>

class SimpleShader
{
private:
    static std::string defaultVert;
    static std::string fragHead;

    int currentTextureSlot;
    std::map<std::string, int> textureToSlot;

public:
    ax::backend::Program* program;
    ax::backend::ProgramState *programState;

    SimpleShader(const std::string& vertSource, const std::string& fragSource);
    ~SimpleShader();

    static SimpleShader* createWithFragmentShader(const std::string& fragShaderPath);
    static SimpleShader* createWithVertexAndFragmentShader(const std::string& vertShaderPath, const std::string& fragShaderPath);

    template <typename T>
    void setUniform(std::string uniform, T value);
    void setUniform(std::string uniform, ax::Texture2D* value);
};

template<typename T>
void SimpleShader::setUniform(std::string uniform, T value)
{
    auto uniformLocation = programState->getUniformLocation(uniform);
    programState->setUniform(uniformLocation, &value, sizeof(value));
}


class SimpleShaderManager
{
private:
    unsigned long long baseTime;
    std::vector<SimpleShader*> shaders;

    SimpleShaderManager();
public:
    static SimpleShaderManager* getInstance();

    void updateShaderTime();

    friend SimpleShader;
};

