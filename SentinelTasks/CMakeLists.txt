add_executable(SentinelTasks
    main.cpp
    src/Application.cpp
)

target_include_directories(SentinelTasks PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CURSES_INCLUDE_DIRS}
)

target_link_libraries(SentinelTasks PRIVATE ${CURSES_LIBRARIES})

target_compile_options(SentinelTasks PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)
