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

    add_compile_definitions(V_COMPANY_NAME="Proxia")

    add_compile_definitions(
        COMMON_4=1
        IOT_PLATFORM
        SINGLE_STORER_THREAD=1
        USE_IP_ADDRESSES
        )

    add_compile_definitions(
        LOG_LEVEL_BY_PRIORITET=1
        LOG_JUSTIFIED=1
        LOG_FILE_NAME_WIDTH=20
        LOG_CATEGORY_WIDTH=16
        LOG_CATEGORY_ENABLED=0
        )

    add_compile_definitions(
        LEFT_BOUND_TIME_4772=10800000
        RIGHT_BOUND_TIME_4772=181440000
        )

    if (${TARGET_OS} STREQUAL "linux-arm")
        add_compile_definitions(
            ONLY_LINUX_ARM=1
            UNC_L_BUILD=1
            USE_ONE_DEVICE_CONFIG=1
            LOADING_METER_DRIVERS_NOT_ALLOWED_ANYWAY=1
            )
    endif ()

    if (MSVC)
      add_compile_definitions(_CRT_SECURE_NO_WARNINGS _CRT_SECURE_NO_DEPRECATE)
    elseif (MINGW)
      add_compile_definitions(STRSAFE_NO_DEPRECATE)
    elseif(CMAKE_COMPILER_IS_GNUCXX)
      add_compile_options(-Wno-unused-function -Wno-unused-result)
    endif()

endmacro(set_build_options)
