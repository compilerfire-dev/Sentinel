add_executable(SentinelEditor
    main.cpp
    src/Application.cpp
    src/EditorBuffer.cpp
    src/FuzzySearch.cpp
    src/LineNumberGutter.cpp
    src/TypingMetrics.cpp
)

target_include_directories(SentinelEditor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(SentinelEditor PRIVATE
    PkgConfig::GTK3
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

    add_executable(SentinelEditorTypingMetricsTest
        tests/TypingMetricsTest.cpp
        src/TypingMetrics.cpp
    )
    target_include_directories(SentinelEditorTypingMetricsTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_compile_options(SentinelEditorTypingMetricsTest PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
    add_test(NAME SentinelEditor.TypingMetrics COMMAND SentinelEditorTypingMetricsTest)

    add_executable(SentinelEditorFuzzySearchTest
        tests/FuzzySearchTest.cpp
        src/FuzzySearch.cpp
    )
    target_include_directories(SentinelEditorFuzzySearchTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_compile_options(SentinelEditorFuzzySearchTest PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
    add_test(NAME SentinelEditor.FuzzySearch COMMAND SentinelEditorFuzzySearchTest)
endif()
