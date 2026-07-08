# Portable application core, reused verbatim by the host test build.
idf_component_register(
    SRCS
        "src/ringbuf.c"
        "src/power.c"
        "src/fsm.c"
        "src/app.c"
    INCLUDE_DIRS
        "include"
)
