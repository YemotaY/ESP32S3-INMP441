# Host unit tests.
add_library(testsupport STATIC mock_power.c)
target_include_directories(testsupport PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(testsupport PUBLIC core)

set(SUITES test_ringbuf test_power test_fsm test_app)

foreach(suite ${SUITES})
    add_executable(${suite} ${suite}.c)
    target_link_libraries(${suite} PRIVATE core testsupport)
    add_test(NAME ${suite} COMMAND ${suite})
endforeach()
