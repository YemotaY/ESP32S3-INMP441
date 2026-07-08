# SimonSays server: host C program (STT/intent/debounce). Portable logic in serverlib,
# thin POSIX socket glue in the executable.
add_library(serverlib STATIC
    src/http_ingest.c
    src/intent.c
    src/debounce.c
    src/vad.c
    src/stt_stub.c
    src/session.c
    src/config.c
)
target_include_directories(serverlib PUBLIC include)

add_executable(simonsays-server src/main.c)
target_link_libraries(simonsays-server PRIVATE serverlib)
