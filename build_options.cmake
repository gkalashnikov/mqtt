macro (set_build_options PROJECTS_ROOT_DIR)

    include(${PROJECTS_ROOT_DIR}/processor_bit.cmake)
    include(${PROJECTS_ROOT_DIR}/target_os.cmake)

    include(build_location)
    set (macro_args ${ARGN})
    list(LENGTH macro_args num_args)
    if (${num_args} GREATER 0)
        list(GET macro_args 0 SUB_BULID_FOLDER)
        set_build_location(${PROJECTS_ROOT_DIR} ${SUB_BULID_FOLDER})
    else()
        set_build_location(${PROJECTS_ROOT_DIR})
    endif ()

    include(version)
    gen_version_file(${PROJECTS_ROOT_DIR})

    add_compile_definitions(V_COMPANY_NAME="Company")

    if (MSVC)
      add_compile_definitions(_CRT_SECURE_NO_WARNINGS _CRT_SECURE_NO_DEPRECATE)
    elseif (MINGW)
      add_compile_definitions(STRSAFE_NO_DEPRECATE)
    elseif(CMAKE_COMPILER_IS_GNUCXX)
      add_compile_options(-Wno-unused-function -Wno-unused-result)
    endif()

endmacro(set_build_options)
