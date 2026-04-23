#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#else
#include <SDL3/SDL_opengl.h>
#endif

struct InstanceData {
    glm::vec3 position;
    glm::vec3 rotationAxis;
    float rotationSpeed;
    float phase;
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    void Init(int instanceCount);
    void Render(const glm::mat4& view, const glm::mat4& projection, float time);

private:
    uint32_t shaderProgram;
    uint32_t VAO, VBO, EBO, instanceVBO;
    int numInstances;

    void CompileShaders();
    void SetupGeometry();
};
