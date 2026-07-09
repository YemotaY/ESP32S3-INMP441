# SimonSays server: host C program (STT/intent/debounce). Portable logic in serverlib,
# thin POSIX socket glue in the executable.
add_library(serverlib STATIC
    src/http_ingest.c
    src/intent.c
    src/debounce.c
    src/vad.c
    src/stt_stub.c
    src/stt_whisper.c
    src/session.c
    src/config.c
)
target_include_directories(serverlib PUBLIC include)

# Optional whisper.cpp STT backend. Enable with -DSIMONSAYS_WITH_WHISPER=ON.
# Point CMake at your whisper.cpp install via -DWHISPER_ROOT=/path or CMAKE_PREFIX_PATH.
option(SIMONSAYS_WITH_WHISPER "Link the whisper.cpp STT backend" OFF)
if(SIMONSAYS_WITH_WHISPER)
    find_path(WHISPER_INCLUDE_DIR whisper.h HINTS ${WHISPER_ROOT} PATH_SUFFIXES include)
    find_library(WHISPER_LIBRARY NAMES whisper HINTS ${WHISPER_ROOT} PATH_SUFFIXES lib)
    if(NOT WHISPER_INCLUDE_DIR OR NOT WHISPER_LIBRARY)
        message(FATAL_ERROR
            "SIMONSAYS_WITH_WHISPER=ON but whisper.h / libwhisper not found. "
            "Set -DWHISPER_ROOT=/path/to/whisper.cpp install prefix.")
    endif()
    target_compile_definitions(serverlib PUBLIC SIMONSAYS_WITH_WHISPER)
    target_include_directories(serverlib PUBLIC ${WHISPER_INCLUDE_DIR})
    target_link_libraries(serverlib PUBLIC ${WHISPER_LIBRARY})
    message(STATUS "SimonSays: whisper.cpp STT backend enabled (${WHISPER_LIBRARY})")
endif()

add_executable(simonsays-server src/main.c)
target_link_libraries(simonsays-server PRIVATE serverlib)
