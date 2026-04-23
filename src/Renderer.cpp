#include "Renderer.h"

#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <random>

const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

// Instanced attributes
layout (location = 2) in vec3 aInstancePos;
layout (location = 3) in vec3 aInstanceAxis;
layout (location = 4) in float aInstanceSpeed;
layout (location = 5) in float aInstancePhase;

uniform mat4 view;
uniform mat4 projection;
uniform float time;

out vec3 FragColor_in;

mat4 rotationMatrix(vec3 axis, float angle) {
    axis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;

    return mat4(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,  0.0,
                oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,  0.0,
                oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c,           0.0,
                0.0,                                0.0,                                0.0,                                1.0);
}

void main()
{
    float angle = aInstancePhase + time * aInstanceSpeed;
    mat4 rot = rotationMatrix(aInstanceAxis, angle);
    vec4 rotatedPos = rot * vec4(aPos, 1.0);
    vec4 worldPos = rotatedPos + vec4(aInstancePos, 0.0);
    gl_Position = projection * view * worldPos;
    FragColor_in = aColor;
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
in vec3 FragColor_in;

void main()
{
    FragColor = vec4(FragColor_in, 1.0);
}
)";

Renderer::Renderer()
    : shaderProgram(0),
      VAO(0),
      VBO(0),
      EBO(0),
      instanceVBO(0),
      FBO(0),
      texture(0),
      RBO(0),
      numInstances(0),
      width(0),
      height(0) {}

Renderer::~Renderer() {
  if (VAO) glDeleteVertexArrays(1, &VAO);
  if (VBO) glDeleteBuffers(1, &VBO);
  if (EBO) glDeleteBuffers(1, &EBO);
  if (instanceVBO) glDeleteBuffers(1, &instanceVBO);
  if (shaderProgram) glDeleteProgram(shaderProgram);
  if (FBO) glDeleteFramebuffers(1, &FBO);
  if (texture) glDeleteTextures(1, &texture);
  if (RBO) glDeleteRenderbuffers(1, &RBO);
}

void Renderer::CompileShaders() {
  auto compileShader = [](uint32_t type, const char* source) -> uint32_t {
    uint32_t shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
      glGetShaderInfoLog(shader, 512, NULL, infoLog);
      std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    return shader;
  };

  uint32_t vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
  uint32_t fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

  shaderProgram = glCreateProgram();
  glAttachShader(shaderProgram, vertexShader);
  glAttachShader(shaderProgram, fragmentShader);
  glLinkProgram(shaderProgram);

  int success;
  char infoLog[512];
  glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
  if (!success) {
    glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
    std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
  }

  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);
}

void Renderer::SetupGeometry() {
  // Cube geometry
  float vertices[] = {
      // positions          // colors
      -0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f,  // front-bottom-left (red)
      0.5f,  -0.5f, -0.5f, 0.0f, 1.0f, 0.0f,  // front-bottom-right (green)
      0.5f,  0.5f,  -0.5f, 0.0f, 0.0f, 1.0f,  // front-top-right (blue)
      -0.5f, 0.5f,  -0.5f, 1.0f, 1.0f, 0.0f,  // front-top-left (yellow)
      -0.5f, -0.5f, 0.5f,  1.0f, 0.0f, 1.0f,  // back-bottom-left (magenta)
      0.5f,  -0.5f, 0.5f,  0.0f, 1.0f, 1.0f,  // back-bottom-right (cyan)
      0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  // back-top-right (white)
      -0.5f, 0.5f,  0.5f,  0.5f, 0.5f, 0.5f   // back-top-left (gray)
  };

  unsigned int indices[] = {// Front face
                            0, 1, 2, 2, 3, 0,
                            // Back face
                            4, 5, 6, 6, 7, 4,
                            // Left face
                            4, 0, 3, 3, 7, 4,
                            // Right face
                            1, 5, 6, 6, 2, 1,
                            // Top face
                            3, 2, 6, 6, 7, 3,
                            // Bottom face
                            4, 5, 1, 1, 0, 4};

  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);
  glGenBuffers(1, &EBO);

  glBindVertexArray(VAO);

  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

  // vertex positions
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
  // vertex colors
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
}

void Renderer::Init(int instanceCount) {
  CompileShaders();
  SetupGeometry();
  UpdateInstanceCount(instanceCount);
}

void Renderer::UpdateInstanceCount(int instanceCount) {
  if (numInstances == instanceCount) {
    return;
  }
  numInstances = instanceCount;

  std::vector<InstanceData> instanceData(numInstances);
  std::mt19937 rng(1337);
  std::uniform_real_distribution<float> posDist(-50.0f, 50.0f);
  std::uniform_real_distribution<float> axisDist(-1.0f, 1.0f);
  std::uniform_real_distribution<float> speedDist(0.5f, 5.0f);
  std::uniform_real_distribution<float> phaseDist(0.0f, 6.2831853f);

  for (int i = 0; i < numInstances; ++i) {
    instanceData[i].position = glm::vec3(posDist(rng), posDist(rng), posDist(rng) - 60.0f);
    glm::vec3 axis = glm::normalize(glm::vec3(axisDist(rng), axisDist(rng), axisDist(rng)));
    if (glm::length(axis) < 0.001f) axis = glm::vec3(0, 1, 0);  // fallback
    instanceData[i].rotationAxis = axis;
    instanceData[i].rotationSpeed = speedDist(rng);
    instanceData[i].phase = phaseDist(rng);
  }

  if (instanceVBO == 0) {
    glGenBuffers(1, &instanceVBO);
  }
  glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
  glBufferData(GL_ARRAY_BUFFER, numInstances * sizeof(InstanceData), instanceData.data(), GL_STATIC_DRAW);

  glBindVertexArray(VAO);
  // Instance positions
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)offsetof(InstanceData, position));
  glVertexAttribDivisor(2, 1);

  // Instance rotation axis
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)offsetof(InstanceData, rotationAxis));
  glVertexAttribDivisor(3, 1);

  // Instance rotation speed
  glEnableVertexAttribArray(4);
  glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)offsetof(InstanceData, rotationSpeed));
  glVertexAttribDivisor(4, 1);

  // Instance phase
  glEnableVertexAttribArray(5);
  glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)offsetof(InstanceData, phase));
  glVertexAttribDivisor(5, 1);

  glBindVertexArray(0);
}
 
void Renderer::CreateFBO(int w, int h) {
  if (width == w && height == h && FBO != 0) return;
 
  width = w;
  height = h;
 
  if (FBO) glDeleteFramebuffers(1, &FBO);
  if (texture) glDeleteTextures(1, &texture);
  if (RBO) glDeleteRenderbuffers(1, &RBO);
 
  glGenFramebuffers(1, &FBO);
  glBindFramebuffer(GL_FRAMEBUFFER, FBO);
 
  // Create texture
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
 
  // Create renderbuffer for depth and stencil
  glGenRenderbuffers(1, &RBO);
  glBindRenderbuffer(GL_RENDERBUFFER, RBO);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, RBO);
 
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    std::cerr << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
  }
 
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::Render(const glm::mat4& view, const glm::mat4& projection, float time) {
  if (FBO == 0) return;
 
  glBindFramebuffer(GL_FRAMEBUFFER, FBO);
  glViewport(0, 0, width, height);
  glClearColor(0.1f, 0.1f, 0.1f, 1.00f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
 
  // Enable depth testing for 3D rendering
  glEnable(GL_DEPTH_TEST);
 
  glUseProgram(shaderProgram);
 
  // Set uniforms
  glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
  glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
  glUniform1f(glGetUniformLocation(shaderProgram, "time"), time);
 
  glBindVertexArray(VAO);
  glDrawElementsInstanced(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0, numInstances);
  glBindVertexArray(0);
 
  glDisable(GL_DEPTH_TEST);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
