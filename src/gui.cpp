#include "gui.h"

#include "imgui.h"

#include "debug_settings.h"

void DrawFramerateOverlayWindow() {
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


void DrawDebugSettingsWindow() {
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
      ImGui::Text("UI FPS");
      ImGui::TableNextColumn();
      ImGui::SetNextItemWidth(60.0f);
      if (ImGui::InputInt("##UI FPS", &local_state.target_ui_render_fps, 0)) {
        if (local_state.target_ui_render_fps > 600) local_state.target_ui_render_fps = 600;
      }

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Thread FPS");
      ImGui::TableNextColumn();
      ImGui::SetNextItemWidth(60.0f);
      if (ImGui::InputInt("##Thread FPS", &local_state.target_thread_fps, 0)) {
        if (local_state.target_thread_fps > 600) local_state.target_thread_fps = 600;
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

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Vsync");
      ImGui::TableNextColumn();
      ImGui::Checkbox("##Vsync", &local_state.vsync);

      ImGui::EndTable();
    }

    bool is_modified =
        (local_state.cube_count != g_debug_hackery_settings.cube_count) ||
        (local_state.target_poll_fps != g_debug_hackery_settings.target_poll_fps) ||
        (local_state.target_ui_render_fps != g_debug_hackery_settings.target_ui_render_fps) ||
        (local_state.target_thread_fps != g_debug_hackery_settings.target_thread_fps) ||
        (local_state.yield_in_event_loop_milliseconds != g_debug_hackery_settings.yield_in_event_loop_milliseconds) ||
        (local_state.flush_instead_of_finish != g_debug_hackery_settings.flush_instead_of_finish) ||
        (local_state.fence_sync != g_debug_hackery_settings.fence_sync) ||
        (local_state.vsync != g_debug_hackery_settings.vsync);

    ImGui::BeginDisabled(!is_modified);

    if (ImGui::Button("Apply")) {
      g_debug_hackery_settings.ApplyDebugSettings(local_state);
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      local_state = g_debug_hackery_settings;
    }

    ImGui::EndDisabled();
  }
  ImGui::End();
}
