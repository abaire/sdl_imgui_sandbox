#include "debug_settings.h"

DebugHackerySettings g_debug_hackery_settings = {
    .target_poll_fps = 0,
    .target_ui_render_fps = 0,
    .target_thread_fps = 60,
    .yield_in_event_loop_milliseconds = 0,
    .flush_instead_of_finish = false,
    .fence_sync = false,
    .cube_count = kInitialCubeCount,
    .poll_frequency_ns = 0,
    .ui_render_frequency_ns = 0,
    .thread_frequency_ns = 0,
};
