#include <SDL3/SDL.h>
#include <epoxy/gl.h>

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <mutex>
#include <thread>

#include "debug_settings.h"
#include "gloffscreen.h"
#include "gui.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"
#include "renderer.h"

#define ASSERT_NO_GL_ERROR()                                                                   \
  do {                                                                                         \
    GLenum error = glGetError();                                                               \
    if (error != GL_NO_ERROR) {                                                                \
      fprintf(stderr, "OpenGL error: 0x%X (%d) at %s:%d\n", error, error, __FILE__, __LINE__); \
      assert(!"OpenGL error detected");                                                        \
    }                                                                                          \
  } while (0)

static void RenderUI(ImGuiIO& io, uint32_t rendered_texture, float thread_fps) {
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
    ImGui::Text("UI FPS: %.1f", io.Framerate);
    ImGui::Text("Thread FPS: %.1f", thread_fps);
  }
  ImGui::End();

  DrawFramerateOverlayWindow();
  DrawDebugSettingsWindow();

  // Draw the rendered texture from the other thread
  if (rendered_texture) {
    ImGui::GetBackgroundDrawList()->AddImage((ImTextureID)(intptr_t)rendered_texture, ImVec2(0, 0),
                                             ImVec2(io.DisplaySize.x, io.DisplaySize.y), ImVec2(0, 1), ImVec2(1, 0));
  }

  // Rendering
  ImGui::Render();
  glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
  glClearColor(0.1f, 0.1f, 0.1f, 1.00f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

struct RenderThreadData {
  std::mutex mutex;
  std::condition_variable cv;
  std::atomic<bool> quit{false};
  std::atomic<bool> frame_ready{false};
  std::atomic<int> cube_count{kInitialCubeCount};
  std::atomic<float> actual_fps{0.0f};
  int width = 1280;
  int height = 720;
  uint32_t texture_id = 0;
  GloContext* ctx;
};

void RenderThreadFunc(RenderThreadData* data) {
  glo_set_current(data->ctx);

  Renderer renderer;
  renderer.Init(data->cube_count);

  uint64_t last_fps_update = SDL_GetTicksNS();
  int frame_count = 0;

  while (!data->quit) {
    uint64_t loop_start = SDL_GetTicksNS();

    int w, h, count;
    {
      std::unique_lock<std::mutex> lock(data->mutex);
      // Wait for the main thread to be ready for a new frame
      data->cv.wait(lock, [&] { return !data->frame_ready || data->quit; });
      if (data->quit) break;

      w = data->width;
      h = data->height;
      count = data->cube_count;
    }

    renderer.CreateFBO(w, h);
    renderer.UpdateInstanceCount(count);

    float time = static_cast<float>(SDL_GetTicks()) / 1000.0f;
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)w / (float)h, 0.1f, 1000.0f);
    glm::mat4 view =
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    renderer.Render(view, projection, time);
    glFinish();  // Ensure rendering is complete before hand-off

    {
      std::unique_lock<std::mutex> lock(data->mutex);
      data->texture_id = renderer.GetTexture();
      data->frame_ready = true;
      data->cv.notify_one();
    }

    // Throttling
    if (g_debug_hackery_settings.thread_frequency_ns > 0) {
      uint64_t now = SDL_GetTicksNS();
      uint64_t elapsed = now - loop_start;
      if (elapsed < static_cast<uint64_t>(g_debug_hackery_settings.thread_frequency_ns)) {
        SDL_DelayPrecise(g_debug_hackery_settings.thread_frequency_ns - elapsed);
      }
    }

    // FPS calculation
    frame_count++;
    uint64_t now = SDL_GetTicksNS();
    if (now - last_fps_update >= kOneSecondNanos) {
      data->actual_fps = static_cast<float>(frame_count) /
                         (static_cast<float>(now - last_fps_update) / static_cast<float>(kOneSecondNanos));
      frame_count = 0;
      last_fps_update = now;
    }
  }

  glo_set_current(nullptr);
}

static void MainLoop(SDL_Window* window, std::thread& render_thread, RenderThreadData& thread_data, ImGuiIO& io) {
  // Main loop
  bool done = false;

  uint64_t next_poll = 0;
  uint64_t next_render = 0;

  while (!done) {
    uint64_t now = SDL_GetTicksNS();

    if (!g_debug_hackery_settings.poll_frequency_ns || now >= next_poll) {
      SDL_Event event;
      while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        if (event.type == SDL_EVENT_QUIT) done = true;
        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
          done = true;
        if (event.type == SDL_EVENT_WINDOW_RESIZED) {
          std::unique_lock<std::mutex> lock(thread_data.mutex);
          thread_data.width = event.window.data1;
          thread_data.height = event.window.data2;
        }
      }
      next_poll = g_debug_hackery_settings.poll_frequency_ns ? now + g_debug_hackery_settings.poll_frequency_ns : 0;
    }

    if (!g_debug_hackery_settings.ui_render_frequency_ns || now >= next_render) {
      thread_data.cube_count = g_debug_hackery_settings.cube_count;

      static uint32_t tex = 0;
      {
        std::unique_lock<std::mutex> lock(thread_data.mutex);
        if (thread_data.frame_ready) {
          tex = thread_data.texture_id;
        }
      }

      RenderUI(io, tex, thread_data.actual_fps);

      SDL_GL_SwapWindow(window);
      ASSERT_NO_GL_ERROR();

      {
        std::unique_lock<std::mutex> lock(thread_data.mutex);
        if (thread_data.frame_ready) {
          thread_data.frame_ready = false;
          thread_data.cv.notify_one();
        }
      }

      next_render =
          g_debug_hackery_settings.ui_render_frequency_ns ? now + g_debug_hackery_settings.ui_render_frequency_ns : 0;
    }

    uint64_t deadline = std::min(next_poll, next_render);
    now = 0;
    if (deadline > 0) {
      now = SDL_GetTicksNS();
      if (now < deadline) {
        SDL_DelayPrecise(deadline - now);
      }
    }

    if (!now && g_debug_hackery_settings.yield_in_event_loop_milliseconds) {
      SDL_Delay(g_debug_hackery_settings.yield_in_event_loop_milliseconds);
    }
  }
}

int main(int, char**) {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
    printf("Error: SDL_Init(): %s\n", SDL_GetError());
    return -1;
  }

  const char* glsl_version = "#version 400";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

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
  SDL_GL_SetSwapInterval(1);

  SDL_ShowWindow(window);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
  io.IniFilename = nullptr;

  ImGui::StyleColorsDark();

  ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
  ImGui_ImplOpenGL3_Init(glsl_version);

  g_debug_hackery_settings.Update();
  RenderThreadData thread_data;
  thread_data.width = 1280;
  thread_data.height = 720;
  thread_data.cube_count = g_debug_hackery_settings.cube_count;
  thread_data.ctx = glo_context_create();
  if (!thread_data.ctx) {
    printf("Failed to create offscreen GL context\n");
    return -1;
  }
  glo_set_current(nullptr);
  SDL_GL_MakeCurrent(window, gl_context);

  std::thread render_thread(RenderThreadFunc, &thread_data);

  MainLoop(window, render_thread, thread_data, io);

  // Cleanup
  {
    std::unique_lock<std::mutex> lock(thread_data.mutex);
    thread_data.quit = true;
    thread_data.cv.notify_all();
  }
  render_thread.join();

  glo_context_destroy(thread_data.ctx);

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();

  SDL_GL_DestroyContext(gl_context);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
