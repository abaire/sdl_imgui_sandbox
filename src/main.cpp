#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#endif

#include <SDL3/SDL.h>

#include <cstdio>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"

#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL3/SDL_opengles2.h>
#elif defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#else
#include <SDL3/SDL_opengl.h>
#endif

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "Renderer.h"

static void DrawFramerateOverlayWindow() {
  const float PAD = 20.0f;
  const float FLICKER_RECT_W = 8.f;
  const float FLICKER_RECT_H = 16.f;
  const float TRACK_OFFSET_Y = FLICKER_RECT_H + 4;
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImVec2 work_pos = viewport->WorkPos;
  ImVec2 work_size = viewport->WorkSize;

  // Pin window to the bottom right corner
  ImVec2 window_pos(work_pos.x + work_size.x - PAD, work_pos.y + work_size.y - PAD - TRACK_OFFSET_Y);
  ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, ImVec2(1.0f, 1.0f));
  ImGui::SetNextWindowBgAlpha(0.8f);

  ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                           ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;

  if (ImGui::Begin("HostVsyncTest", nullptr, flags)) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    p.y += TRACK_OFFSET_Y;

    const int steps = 60;
    const float block_w = 4.0f;
    const float block_h = 16.0f;
    const float track_w = steps * block_w;
    const float spacing = 4.0f;

    double t = ImGui::GetTime();
    int idx_60 = (int)(t * 60.0) % steps;
    int idx_30 = (int)(t * 30.0) % (steps / 2);

    draw_list->AddRectFilled(p, ImVec2(p.x + track_w, p.y + block_h), IM_COL32(50, 50, 50, 255));
    draw_list->AddRectFilled(ImVec2(p.x, p.y + block_h + spacing), ImVec2(p.x + track_w, p.y + block_h * 2 + spacing),
                             IM_COL32(50, 50, 50, 255));

    ImVec2 p60(p.x + (idx_60 * block_w), p.y);
    draw_list->AddRectFilled(p60, ImVec2(p60.x + block_w, p60.y + block_h), IM_COL32(255, 0, 0, 255));

    ImVec2 p30(p.x + (idx_30 * block_w * 2.0f), p.y + block_h + spacing);
    draw_list->AddRectFilled(p30, ImVec2(p30.x + block_w * 2.0f, p30.y + block_h), IM_COL32(0, 255, 0, 255));

    ImVec2 flicker_fusion(p.x + track_w - FLICKER_RECT_W * 2.f, p.y - TRACK_OFFSET_Y);
    draw_list->AddRectFilled(flicker_fusion,
                             ImVec2(flicker_fusion.x + FLICKER_RECT_W, flicker_fusion.y + FLICKER_RECT_H),
                             IM_COL32(0xBA, 0xBA, 0, 0xFF));
    draw_list->AddRectFilled(ImVec2(flicker_fusion.x + FLICKER_RECT_W, flicker_fusion.y),
                             ImVec2(flicker_fusion.x + FLICKER_RECT_W * 2.f, flicker_fusion.y + FLICKER_RECT_H),
                             (idx_60 & 0x01) ? IM_COL32(0xFF, 0, 0, 0xFF) : IM_COL32(0, 0xFF, 0, 0xFF));

    // Reserve space to ensure the window encapsulates the manually drawn
    // primitives
    ImGui::Dummy(ImVec2(track_w, (block_h * 2) + spacing + TRACK_OFFSET_Y));
  }
  ImGui::End();
}

static void RenderFrame(Renderer& renderer, ImGuiIO& io) {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();

  // Show framerate in upper right corner
  ImGuiWindowFlags fps_window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                      ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 10.0f, 10.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
  ImGui::SetNextWindowBgAlpha(0.65f);
  if (ImGui::Begin("FPS Overlay", nullptr, fps_window_flags)) {
    ImGui::Text("FPS: %.1f", io.Framerate);
  }
  ImGui::End();

  DrawFramerateOverlayWindow();

  // Rendering
  ImGui::Render();
  glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
  glClearColor(0.1f, 0.1f, 0.1f, 1.00f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Render our 3D scene before ImGui draw data
  float time = SDL_GetTicks() / 1000.0f;
  glm::mat4 projection = glm::perspective(glm::radians(45.0f), io.DisplaySize.x / io.DisplaySize.y, 0.1f, 1000.0f);
  glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
  renderer.Render(view, projection, time);

  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// Main code
int main(int, char**) {
  // Setup SDL
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
    printf("Error: SDL_Init(): %s\n", SDL_GetError());
    return -1;
  }

  // Decide GL+GLSL versions
#if defined(__APPLE__)
  // GL 3.2 Core + GLSL 150
  const char* glsl_version = "#version 150";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);  // Always required on Mac
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
  // GL 3.0 + GLSL 130
  const char* glsl_version = "#version 130";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

  // Create window with graphics context
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  Uint32 window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;
  SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL3+OpenGL3 example", 1280, 720, window_flags);
  if (window == nullptr) {
    printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
    return -1;
  }
  SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
  SDL_GLContext gl_context = SDL_GL_CreateContext(window);
  if (gl_context == nullptr) {
    printf("Error: SDL_GL_CreateContext(): %s\n", SDL_GetError());
    return -1;
  }
  SDL_GL_MakeCurrent(window, gl_context);
  SDL_GL_SetSwapInterval(1);  // Enable vsync
  SDL_ShowWindow(window);

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls
  io.IniFilename = nullptr;                              // Disable imgui.ini generation

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();

  // Setup Platform/Renderer backends
  ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
  ImGui_ImplOpenGL3_Init(glsl_version);

  // Initialize Renderer after ImGui_ImplOpenGL3_Init
  // so that imgui's internal GL loader is initialized.
  Renderer renderer;
  renderer.Init(10000);  // 10,000 spinning cubes

  // Main loop
  bool done = false;
  while (!done) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT) done = true;
      if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
        done = true;
    }

    RenderFrame(renderer, io);
    SDL_GL_SwapWindow(window);
  }

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();

  SDL_GL_DestroyContext(gl_context);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
