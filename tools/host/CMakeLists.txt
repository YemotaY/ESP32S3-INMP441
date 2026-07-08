# Host-only tooling (not built into firmware).
add_library(wavtool STATIC wav.c)
target_include_directories(wavtool PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(wav_simulate wav_simulate.c)
target_link_libraries(wav_simulate PRIVATE core wavtool m)

# POSIX socket transport + PCM source, shared by the client demo and the e2e test.
add_library(nettool STATIC posix_transport.c)
target_include_directories(nettool PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(nettool PUBLIC core)

add_executable(simonsays-client simonsays_client.c)
target_link_libraries(simonsays-client PRIVATE core nettool wavtool)
