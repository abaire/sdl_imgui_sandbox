#pragma once

#include <epoxy/gl.h>

#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

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
  void UpdateInstanceCount(int instanceCount);
  void Render(const glm::mat4& view, const glm::mat4& projection, float time);

  void CreateFBO(int width, int height);
  uint32_t GetTexture() const { return texture; }

 private:
  uint32_t shaderProgram;
  uint32_t VAO, VBO, EBO, instanceVBO;
  uint32_t FBO, texture, RBO;
  int numInstances;
  int width, height;

  void CompileShaders();
  void SetupGeometry();
};
