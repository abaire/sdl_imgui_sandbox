#include <epoxy/gl.h>
#include <SDL3/SDL.h>

#include <cstdio>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "Renderer.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"
#include "gloffscreen.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

static constexpr int kInitialCubeCount = 10000;
static constexpr int kMaxCubeCount = 50000000;

typedef struct DebugHackerySettings {
  int target_poll_fps;
  int target_render_fps;
  int yield_in_event_loop_milliseconds;
  bool flush_instead_of_finish;
  bool fence_sync;
  int cube_count;

  int64_t poll_frequency_ns;
  int64_t render_frequency_ns;
} DebugHackerySettings;

static DebugHackerySettings g_debug_hackery_settings = {
    .cube_count = kInitialCubeCount,
};

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

static void ApplyDebugSettings(DebugHackerySettings& new_state) {
  static constexpr int64_t kOneSecondNanos = 1000000000;
  g_debug_hackery_settings = new_state;

  g_debug_hackery_settings.poll_frequency_ns =
      new_state.target_poll_fps ? kOneSecondNanos / static_cast<int64_t>(new_state.target_poll_fps) : 0;
  g_debug_hackery_settings.render_frequency_ns =
      new_state.target_render_fps ? kOneSecondNanos / static_cast<int64_t>(new_state.target_render_fps) : 0;
}

static void DrawDebugSettingsWindow() {
  static DebugHackerySettings local_state;
  static bool initialized = false;

  if (!initialized) {
    local_state = g_debug_hackery_settings;
    initialized = true;
  }

  ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImVec2 pos(viewport->WorkPos.x + 10.0f, viewport->WorkPos.y + viewport->WorkSize.y - 10.0f);
  ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(0.0f, 1.0f));
  ImGui::SetNextWindowBgAlpha(0.9f);

  ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove;

  if (ImGui::Begin("Debug Hackery", nullptr, flags)) {
    ImGui::Text("Debug Hackery");
    ImGui::Separator();

    if (ImGui::BeginTable("##hackery_table", 2)) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Cube count");
      ImGui::TableNextColumn();
      ImGui::SetNextItemWidth(80.0f);
      if (ImGui::InputInt("##Cube count", &local_state.cube_count, 0)) {
        if (local_state.cube_count < 0) {
          local_state.cube_count = 0;
        } else if (local_state.cube_count > kMaxCubeCount) {
          local_state.cube_count = kMaxCubeCount;
        }
      }

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Poll FPS");
      ImGui::TableNextColumn();
      ImGui::SetNextItemWidth(60.0f);
      if (ImGui::InputInt("##Poll FPS", &local_state.target_poll_fps, 0)) {
        if (local_state.target_poll_fps > 600) local_state.target_poll_fps = 600;
      }

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Render FPS");
      ImGui::TableNextColumn();
      ImGui::SetNextItemWidth(60.0f);
      if (ImGui::InputInt("##Render FPS", &local_state.target_render_fps, 0)) {
        if (local_state.target_render_fps > 600) local_state.target_render_fps = 600;
      }

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Yield ms");
      ImGui::TableNextColumn();
      ImGui::SetNextItemWidth(60.0f);
      if (ImGui::InputInt("##Yield ms", &local_state.yield_in_event_loop_milliseconds, 0)) {
        if (local_state.yield_in_event_loop_milliseconds > 66) local_state.yield_in_event_loop_milliseconds = 66;
      }

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("glFlush instead of glFinish");
      ImGui::TableNextColumn();
      ImGui::Checkbox("##glFlush instead of glFinish", &local_state.flush_instead_of_finish);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Fence sync");
      ImGui::TableNextColumn();
      ImGui::Checkbox("##Fence sync", &local_state.fence_sync);

      ImGui::EndTable();
    }

    bool is_modified =
        (local_state.cube_count != g_debug_hackery_settings.cube_count) ||
        (local_state.target_poll_fps != g_debug_hackery_settings.target_poll_fps) ||
        (local_state.target_render_fps != g_debug_hackery_settings.target_render_fps) ||
        (local_state.yield_in_event_loop_milliseconds != g_debug_hackery_settings.yield_in_event_loop_milliseconds) ||
        (local_state.flush_instead_of_finish != g_debug_hackery_settings.flush_instead_of_finish) ||
        (local_state.fence_sync != g_debug_hackery_settings.fence_sync);

    ImGui::BeginDisabled(!is_modified);

    if (ImGui::Button("Apply")) {
      ApplyDebugSettings(local_state);
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      local_state = g_debug_hackery_settings;
    }

    ImGui::EndDisabled();
  }
  ImGui::End();
}

static void RenderFrame(ImGuiIO& io, uint32_t rendered_texture) {
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
  DrawDebugSettingsWindow();

  // Rendering
  ImGui::Render();
  glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
  glClearColor(0.1f, 0.1f, 0.1f, 1.00f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Draw the rendered texture from the other thread
  if (rendered_texture) {
    ImGui::GetBackgroundDrawList()->AddImage((ImTextureID)(intptr_t)rendered_texture, ImVec2(0, 0),
                                            ImVec2(io.DisplaySize.x, io.DisplaySize.y));
  }

  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

struct RenderThreadData {
  std::mutex mutex;
  std::condition_variable cv;
  std::atomic<bool> quit{false};
  std::atomic<bool> frame_ready{false};
  std::atomic<int> cube_count{kInitialCubeCount};
  int width = 1280;
  int height = 720;
  uint32_t texture_id = 0;
};

void RenderThreadFunc(RenderThreadData* data) {
  GloContext* ctx = glo_context_create();
  if (!ctx) {
    printf("Failed to create offscreen GL context\n");
    return;
  }

  Renderer renderer;
  renderer.Init(data->cube_count);

  while (!data->quit) {
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
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    renderer.Render(view, projection, time);
    glFinish();  // Ensure rendering is complete before hand-off

    {
      std::unique_lock<std::mutex> lock(data->mutex);
      data->texture_id = renderer.GetTexture();
      data->frame_ready = true;
      data->cv.notify_one();
    }
  }

  glo_context_destroy(ctx);
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
  // GL 3.3 + GLSL 130
  const char* glsl_version = "#version 130";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
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

  // Start render thread
  RenderThreadData thread_data;
  thread_data.width = 1280;
  thread_data.height = 720;
  thread_data.cube_count = g_debug_hackery_settings.cube_count;
  std::thread render_thread(RenderThreadFunc, &thread_data);

  // Main loop
  bool done = false;

  uint64_t next_poll = 0;

  while (!done) {
    uint64_t now = 0;
    if (g_debug_hackery_settings.poll_frequency_ns > 0) {
      now = SDL_GetTicksNS();
    }

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
      next_poll = now + g_debug_hackery_settings.poll_frequency_ns;
    }

    // Update thread data from UI settings
    thread_data.cube_count = g_debug_hackery_settings.cube_count;

    uint32_t tex = 0;
    {
      std::unique_lock<std::mutex> lock(thread_data.mutex);
      // Main thread grabs the texture if it's ready
      if (thread_data.frame_ready) {
        tex = thread_data.texture_id;
      }
    }

    RenderFrame(io, tex);

    {
      // Lock while swapping and signaling the render thread
      std::unique_lock<std::mutex> lock(thread_data.mutex);
      SDL_GL_SwapWindow(window);
      if (thread_data.frame_ready) {
        thread_data.frame_ready = false;
        thread_data.cv.notify_one();
      }
    }
    assert(glGetError() == GL_NO_ERROR);

    if (g_debug_hackery_settings.poll_frequency_ns > 0) {
      int64_t deadline = next_poll;
      if (now < deadline) {
        SDL_DelayPrecise(deadline - now);
      }
    } else if (g_debug_hackery_settings.yield_in_event_loop_milliseconds) {
      SDL_Delay(g_debug_hackery_settings.yield_in_event_loop_milliseconds);
    }
  }

  // Cleanup
  {
    std::unique_lock<std::mutex> lock(thread_data.mutex);
    thread_data.quit = true;
    thread_data.cv.notify_all();
  }
  render_thread.join();
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();

  SDL_GL_DestroyContext(gl_context);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
