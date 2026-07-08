# Portable application core, reused verbatim by the host test build.
idf_component_register(
    SRCS
        "src/ringbuf.c"
        "src/power.c"
        "src/fsm.c"
        "src/app.c"
        "src/nn/quant.c"
        "src/nn/kernels.c"
        "src/dsp/fft.c"
        "src/dsp/melspec.c"
        "src/kws.c"
        "src/kws_model.c"
    INCLUDE_DIRS
        "include"
)
