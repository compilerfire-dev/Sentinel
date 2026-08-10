add_executable(SentinelTasks
    main.cpp
    src/Application.cpp
    src/Legend.cpp
    src/TaskDataStore.cpp
    src/TaskTree.cpp
)

target_include_directories(SentinelTasks PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CURSES_INCLUDE_DIRS}
)

target_link_libraries(SentinelTasks PRIVATE
    ${CURSES_LIBRARIES}
    SentinelShared
)

target_compile_options(SentinelTasks PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

if(BUILD_TESTING)
    add_executable(SentinelTasksTreeTest
        tests/TaskTreeTest.cpp
        src/TaskTree.cpp
    )
    target_include_directories(SentinelTasksTreeTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(SentinelTasksTreeTest PRIVATE SentinelShared)
    target_compile_options(SentinelTasksTreeTest PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
    add_test(NAME SentinelTasks.TreeModel COMMAND SentinelTasksTreeTest)
endif()
