#include "debug_settings.h"

#include <SDL3/SDL.h>

#include <algorithm>

DebugHackerySettings g_debug_hackery_settings = {
    .target_poll_fps = 0,
    .target_ui_render_fps = 0,
    .target_thread_fps = 60,
    .yield_in_event_loop_milliseconds = 0,
    .flush_instead_of_finish = false,
    .fence_sync = false,
    .vsync = true,
    .cube_count = kInitialCubeCount,
    .poll_frequency_ns = 0,
    .ui_render_frequency_ns = 0,
    .thread_frequency_ns = 0,
};

void DebugHackerySettings::Update() {
  poll_frequency_ns = target_poll_fps ? kOneSecondNanos / static_cast<int64_t>(target_poll_fps) : 0;
  ui_render_frequency_ns = target_ui_render_fps ? kOneSecondNanos / static_cast<int64_t>(target_ui_render_fps) : 0;
  thread_frequency_ns = target_thread_fps ? kOneSecondNanos / static_cast<int64_t>(target_thread_fps) : 0;
}

void DebugHackerySettings::ApplyDebugSettings(DebugHackerySettings& new_state) {
  *this = new_state;
  Update();
  SDL_GL_SetSwapInterval(vsync ? 1 : 0);
}
