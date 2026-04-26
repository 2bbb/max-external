# bbb_add_external — Max/MSP external を min-api でビルドする共通 macro
#
# usage (in source/projects/<name>/CMakeLists.txt):
#   bbb_add_external(
#       [DEPS lib1 lib2 ...]          # target_link_libraries に渡す依存
#       [INCLUDES dir1 dir2 ...]      # target_include_directories に渡す追加パス
#       [SOURCES file1.cpp ...]       # 追加ソース (省略時: *.cpp を自動収集)
#       [RPATH path]                  # BUILD_RPATH / INSTALL_RPATH を設定
#       [NO_HELP_COPY]                # help ファイルの自動コピーを無効化
#   )
#
# 前提:
#   - PROJECT_NAME が external 名と一致していること
#     (CMakeLists.txt と同名ディレクトリに配置すれば自動設定される)
#   - ルート CMakeLists.txt で以下のいずれかが設定済みであること:
#     a) C74_MIN_API_DIR 変数
#     b) deps/min-api/ が存在する (自動推測する)
#   - C74_LIBRARY_OUTPUT_DIRECTORY が設定済みであること (省略時は <root>/externals)

macro(bbb_add_external)
    cmake_parse_arguments(BBB_ARG
        "NO_HELP_COPY"
        "RPATH"
        "DEPS;INCLUDES;SOURCES"
        ${ARGN}
    )

    # --- min-api path resolution ---
    if(NOT DEFINED C74_MIN_API_DIR)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../../deps/min-api")
            set(C74_MIN_API_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../deps/min-api")
        elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../../extern/min-api")
            set(C74_MIN_API_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../extern/min-api")
        else()
            message(FATAL_ERROR "bbb_add_external: C74_MIN_API_DIR not set and min-api not found")
        endif()
    endif()

    # --- output directory ---
    if(NOT DEFINED C74_LIBRARY_OUTPUT_DIRECTORY)
        # walk up to find project root (heuristic: look for package-info.json or CMakeLists.txt with project())
        set(C74_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../../externals")
    endif()

    # --- universal binary (macOS) ---
    if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "macOS architecture" FORCE)
    endif()

    # --- collect sources ---
    if(BBB_ARG_SOURCES)
        set(_bbb_sources ${BBB_ARG_SOURCES})
    else()
        file(GLOB _bbb_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
    endif()

    # --- min-api pre-target ---
    include(${C74_MIN_API_DIR}/script/min-pretarget.cmake)

    # --- build library ---
    add_library(${PROJECT_NAME} MODULE ${_bbb_sources})

    # --- include directories ---
    target_include_directories(${PROJECT_NAME} PRIVATE ${C74_INCLUDES})
    if(BBB_ARG_INCLUDES)
        target_include_directories(${PROJECT_NAME} PRIVATE ${BBB_ARG_INCLUDES})
    endif()

    # --- link dependencies ---
    if(BBB_ARG_DEPS)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${BBB_ARG_DEPS})
    endif()

    # --- rpath (for externals that load shared libraries at runtime) ---
    if(BBB_ARG_RPATH)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            BUILD_RPATH "${BBB_ARG_RPATH}"
            INSTALL_RPATH "${BBB_ARG_RPATH}"
        )
    endif()

    # --- help file copy ---
    if(NOT BBB_ARG_NO_HELP_COPY)
        set(_bbb_help_src "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.maxhelp")
        set(_bbb_help_dst "${CMAKE_CURRENT_SOURCE_DIR}/../../../help/${PROJECT_NAME}.maxhelp")
        if(EXISTS "${_bbb_help_src}")
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_bbb_help_src}" "${_bbb_help_dst}"
            )
        endif()
    endif()

    # --- min-api post-target ---
    include(${C74_MIN_API_DIR}/script/min-posttarget.cmake)

    # --- cleanup: unset internal variables to avoid scope pollution (macro shares caller scope) ---
    unset(_bbb_sources)
    unset(_bbb_help_src)
    unset(_bbb_help_dst)
    unset(BBB_ARG_NO_HELP_COPY)
    unset(BBB_ARG_RPATH)
    unset(BBB_ARG_DEPS)
    unset(BBB_ARG_INCLUDES)
    unset(BBB_ARG_SOURCES)
    unset(BBB_ARG_UNPARSED_ARGUMENTS)
    unset(BBB_ARG_KEYWORDS_MISSING_VALUES)
endmacro()
