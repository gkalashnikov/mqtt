macro (gen_version_file PROJECTS_ROOT_DIR)

    set(PROJECTS_ROOT_DIR ${PROJECTS_ROOT_DIR})
    string(TOUPPER "${PROJECT_NAME}" PROJECT_NAME_UPPER)

    find_package(Git)
    if (GIT_FOUND)
          execute_process(
                          COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
                          WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
                          OUTPUT_VARIABLE PROJECT_GIT_REVISION
                          ERROR_QUIET
                          OUTPUT_STRIP_TRAILING_WHITESPACE
                          )
    else()
          message(WARNING "git not found")
    endif()

    string(TIMESTAMP PROJECT_BUILD_TIMESTAMP)

    configure_file(${PROJECTS_ROOT_DIR}/version.h.in ${PROJECT_SOURCE_DIR}/version.h NEWLINE_STYLE LF)

    if (WIN32)
        get_target_property(target_type "${PROJECT_NAME}" TYPE)
        if (target_type STREQUAL "EXECUTABLE" OR target_type STREQUAL "SHARED_LIBRARY")
            configure_file(${PROJECTS_ROOT_DIR}/resource.rc.in ${PROJECT_SOURCE_DIR}/resource.rc NEWLINE_STYLE LF)
        endif()
    endif ()

endmacro(gen_version_file)
