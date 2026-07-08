# Host unit tests.
add_library(testsupport STATIC mock_power.c)
target_include_directories(testsupport PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(testsupport PUBLIC core)

set(SUITES test_ringbuf test_power test_fsm test_app
           test_quant test_kernels test_dsp test_kws)

foreach(suite ${SUITES})
    add_executable(${suite} ${suite}.c)
    target_link_libraries(${suite} PRIVATE core testsupport)
    add_test(NAME ${suite} COMMAND ${suite})
endforeach()

# WAV suite additionally needs the host wav tooling.
add_executable(test_wav test_wav.c)
target_link_libraries(test_wav PRIVATE core testsupport wavtool)
add_test(NAME test_wav COMMAND test_wav)

# C<->Python parity: runs the committed golden fixture through the C engine.
add_executable(test_parity test_parity.c)
target_include_directories(test_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/generated)
target_link_libraries(test_parity PRIVATE core testsupport)
add_test(NAME test_parity COMMAND test_parity)

# Python framework tests (self-checking scripts). Wired in only if python3 is available.
find_program(PYTHON3_EXECUTABLE NAMES python3 python)
if(PYTHON3_EXECUTABLE)
    set(PY_TESTS test_quant test_layers_gradcheck test_train_learns
                 test_quantize_codegen test_parity_regen)
    foreach(pyt ${PY_TESTS})
        add_test(NAME py_${pyt}
                 COMMAND ${PYTHON3_EXECUTABLE}
                         ${CMAKE_SOURCE_DIR}/tests/py/${pyt}.py)
        set_tests_properties(py_${pyt} PROPERTIES
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/py)
    endforeach()
endif()
