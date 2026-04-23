add_library(gloffscreen STATIC common.c sdl.c)

target_include_directories(gloffscreen PUBLIC .)

target_link_libraries(gloffscreen PUBLIC SDL3::SDL3-static epoxy::epoxy)
