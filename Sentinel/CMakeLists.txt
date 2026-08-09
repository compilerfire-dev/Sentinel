add_executable(Sentinel
    main.cpp
    src/Application.cpp
    src/CommandProcessor.cpp
    src/FuzzySearch.cpp
    src/Legend.cpp
    src/Task.cpp
    src/TaskManager.cpp
)

target_include_directories(Sentinel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CURSES_INCLUDE_DIRS}
)

target_link_libraries(Sentinel PRIVATE
    ${CURSES_LIBRARIES}
    SentinelShared
)

target_compile_options(Sentinel PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)
