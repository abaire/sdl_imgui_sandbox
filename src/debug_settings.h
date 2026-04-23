#ifndef SANDBOX_DEBUG_SETTINGS_H
#define SANDBOX_DEBUG_SETTINGS_H

#include <cstdint>

constexpr int kInitialCubeCount = 10000;
constexpr int kMaxCubeCount = 50000000;
constexpr int64_t kOneSecondNanos = 1000000000;

typedef struct DebugHackerySettings {
  int target_poll_fps;
  int target_ui_render_fps;
  int target_thread_fps;
  int yield_in_event_loop_milliseconds;
  bool flush_instead_of_finish;
  bool fence_sync;
  bool vsync;
  int cube_count;

  int64_t poll_frequency_ns;
  int64_t ui_render_frequency_ns;
  int64_t thread_frequency_ns;

  void Update();
  void ApplyDebugSettings(DebugHackerySettings& new_state);

} DebugHackerySettings;

extern DebugHackerySettings g_debug_hackery_settings;

#endif  // SANDBOX_DEBUG_SETTINGS_H
