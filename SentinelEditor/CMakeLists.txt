add_executable(SentinelEditor
    main.cpp
    src/Application.cpp
    src/EditorBuffer.cpp
)

target_include_directories(SentinelEditor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CURSES_INCLUDE_DIRS}
)

target_link_libraries(SentinelEditor PRIVATE
    ${CURSES_LIBRARIES}
)

target_compile_options(SentinelEditor PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

if(BUILD_TESTING)
    add_executable(SentinelEditorBufferTest
        tests/EditorBufferTest.cpp
        src/EditorBuffer.cpp
    )
    target_include_directories(SentinelEditorBufferTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_compile_options(SentinelEditorBufferTest PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
    add_test(NAME SentinelEditor.Buffer COMMAND SentinelEditorBufferTest)
endif()
