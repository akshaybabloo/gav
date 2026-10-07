set(GAV_LINT_CPP_SOURCES "")
set(_gav_vendored_dir "${CMAKE_SOURCE_DIR}/vcpkg-ports")
set(_gav_lint_targets appgav)
if (TARGET gav_tests)
    list(APPEND _gav_lint_targets gav_tests)
endif ()
foreach (_target IN LISTS _gav_lint_targets)
    get_target_property(_sources ${_target} SOURCES)
    get_target_property(_source_dir ${_target} SOURCE_DIR)
    foreach (_source IN LISTS _sources)
        if (_source MATCHES "\\$<" OR NOT _source MATCHES "\\.(cpp|h|mm)$")
            continue()
        endif ()
        cmake_path(ABSOLUTE_PATH _source BASE_DIRECTORY "${_source_dir}" NORMALIZE)
        cmake_path(IS_PREFIX CMAKE_BINARY_DIR "${_source}" _in_binary_dir)
        cmake_path(IS_PREFIX CMAKE_SOURCE_DIR "${_source}" _in_source_dir)
        cmake_path(IS_PREFIX _gav_vendored_dir "${_source}" _in_vendored_dir)
        if (_in_binary_dir OR _in_vendored_dir OR NOT _in_source_dir)
            continue()
        endif ()
        list(APPEND GAV_LINT_CPP_SOURCES "${_source}")
    endforeach ()
endforeach ()

set(GAV_PLATFORM_ONLY_SOURCES
        # Builds on Windows only; lint runs on Linux.
        "${CMAKE_SOURCE_DIR}/mediasession_windows.cpp"
        # Builds on macOS only; lint runs on Linux.
        "${CMAKE_SOURCE_DIR}/mediasession_macos.mm"
)
list(REMOVE_ITEM GAV_LINT_CPP_SOURCES ${GAV_PLATFORM_ONLY_SOURCES})
list(REMOVE_DUPLICATES GAV_LINT_CPP_SOURCES)

set(GAV_FORMAT_CPP_FILES
        ${GAV_LINT_CPP_SOURCES}
        ${GAV_PLATFORM_ONLY_SOURCES}
        "${CMAKE_SOURCE_DIR}/mediasession_mpris.cpp"
)
list(REMOVE_DUPLICATES GAV_FORMAT_CPP_FILES)

set(GAV_LINT_QML_FILES "")
foreach (_file IN LISTS GAV_QML_FILES)
    list(APPEND GAV_LINT_QML_FILES "${CMAKE_SOURCE_DIR}/${_file}")
endforeach ()

set(GAV_CLANG_FORMAT "" CACHE FILEPATH "clang-format binary to use instead of the pinned one run through uvx")
set(GAV_CLANG_TIDY "" CACHE FILEPATH "clang-tidy binary to use instead of the pinned one run through uvx")

set(_gav_lint_dir "${CMAKE_BINARY_DIR}/lint")
set(_gav_lint_config "${_gav_lint_dir}/config.cmake")
set(_gav_lint_script "${CMAKE_CURRENT_LIST_DIR}/lint-run.cmake")
set(_gav_lint_requirements "${CMAKE_CURRENT_LIST_DIR}/lint-requirements.txt")

set(_gav_tidy_headers ${GAV_FORMAT_CPP_FILES})
list(FILTER _gav_tidy_headers INCLUDE REGEX "\\.h$")
set(_gav_tidy_sources "")
set(_gav_tidy_directories "")
set(_gav_tidy_databases "")
set(_gav_tidy_results "")
foreach (_source IN LISTS GAV_LINT_CPP_SOURCES)
    if (NOT _source MATCHES "\\.cpp$")
        continue()
    endif ()
    cmake_path(RELATIVE_PATH _source BASE_DIRECTORY "${CMAKE_SOURCE_DIR}" OUTPUT_VARIABLE _name)
    string(REPLACE "/" "_" _directory "${_name}")
    set(_directory "${_gav_lint_dir}/tidy/${_directory}")
    add_custom_command(
            OUTPUT "${_directory}/findings.txt"
            COMMAND "${CMAKE_COMMAND}"
            "-DGAV_LINT_CONFIG=${_gav_lint_config}"
            -DGAV_LINT_ACTION=tidy
            "-DGAV_LINT_FILE=${_source}"
            "-DGAV_LINT_DIRECTORY=${_directory}"
            -P "${_gav_lint_script}"
            DEPENDS "${_source}" ${_gav_tidy_headers} "${_directory}/compile_commands.json"
            "${CMAKE_SOURCE_DIR}/.clang-tidy" "${_gav_lint_requirements}" "${_gav_lint_script}"
            COMMENT "clang-tidy ${_name}"
            VERBATIM
    )
    list(APPEND _gav_tidy_sources "${_source}")
    list(APPEND _gav_tidy_directories "${_directory}")
    list(APPEND _gav_tidy_databases "${_directory}/compile_commands.json")
    list(APPEND _gav_tidy_results "${_directory}/findings.txt")
endforeach ()

add_custom_command(
        OUTPUT ${_gav_tidy_databases}
        COMMAND "${CMAKE_COMMAND}" "-DGAV_LINT_CONFIG=${_gav_lint_config}" -DGAV_LINT_ACTION=compile-commands -P "${_gav_lint_script}"
        DEPENDS "${CMAKE_BINARY_DIR}/compile_commands.json" "${_gav_lint_script}"
        COMMENT "Splitting the compile commands"
        VERBATIM
)

if (TARGET Qt6::qmlformat)
    set(_gav_qmlformat "$<TARGET_FILE:Qt6::qmlformat>")
else ()
    set(_gav_qmlformat "")
endif ()
if (TARGET Qt6::qmllint)
    set(_gav_qmllint "$<TARGET_FILE:Qt6::qmllint>")
else ()
    set(_gav_qmllint "")
endif ()

file(GENERATE OUTPUT "${_gav_lint_config}" CONTENT "set(GAV_SOURCE_DIR \"${CMAKE_SOURCE_DIR}\")
set(GAV_BINARY_DIR \"${CMAKE_BINARY_DIR}\")
set(GAV_LINT_DIR \"${_gav_lint_dir}\")
set(GAV_LINT_REQUIREMENTS \"${_gav_lint_requirements}\")
set(GAV_CLANG_FORMAT \"${GAV_CLANG_FORMAT}\")
set(GAV_CLANG_TIDY \"${GAV_CLANG_TIDY}\")
set(GAV_QMLFORMAT \"${_gav_qmlformat}\")
set(GAV_QMLLINT \"${_gav_qmllint}\")
set(GAV_QMLLINT_ARGUMENTS \"${CMAKE_BINARY_DIR}/.rcc/qmllint/appgav.rsp\")
set(GAV_FORMAT_CPP_FILES \"${GAV_FORMAT_CPP_FILES}\")
set(GAV_LINT_QML_FILES \"${GAV_LINT_QML_FILES}\")
set(GAV_TIDY_SOURCES \"${_gav_tidy_sources}\")
set(GAV_TIDY_DIRECTORIES \"${_gav_tidy_directories}\")
")

function(gav_add_lint_target name action comment)
    add_custom_target(${name}
            COMMAND "${CMAKE_COMMAND}" "-DGAV_LINT_CONFIG=${_gav_lint_config}" -DGAV_LINT_ACTION=${action} -P "${_gav_lint_script}"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            COMMENT "${comment}"
            VERBATIM
            ${ARGN}
    )
endfunction()

gav_add_lint_target(format format "Formatting C++ and QML sources")
gav_add_lint_target(format-cpp format-cpp "Formatting C++ sources")
gav_add_lint_target(format-qml format-qml "Formatting QML sources")
gav_add_lint_target(format-check format-check "Checking formatting")

gav_add_lint_target(lint-versions versions "Lint tool versions")
gav_add_lint_target(lint-cpp lint-cpp "Collecting clang-tidy findings" DEPENDS ${_gav_tidy_results})
gav_add_lint_target(lint-qml lint-qml "Running qmllint")
gav_add_lint_target(lint lint "Collecting lint findings" DEPENDS ${_gav_tidy_results})

add_dependencies(lint-cpp lint-versions ${_gav_lint_targets})
add_dependencies(lint-qml appgav)
add_dependencies(lint lint-versions ${_gav_lint_targets})
