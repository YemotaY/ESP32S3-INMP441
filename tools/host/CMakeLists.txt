# Host-only tooling (not built into firmware).
add_library(wavtool STATIC wav.c)
target_include_directories(wavtool PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(wav_simulate wav_simulate.c)
target_link_libraries(wav_simulate PRIVATE core wavtool m)
