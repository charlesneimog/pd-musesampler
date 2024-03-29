cmake_minimum_required(VERSION 3.15)
include(${CMAKE_CURRENT_SOURCE_DIR}/Resources/pd.build/pd.cmake) # pd.build
project(pd-musesampler)

#╭──────────────────────────────────────╮
#│          MuseScore Headers           │
#╰──────────────────────────────────────╯

# check if MUSESCORE_VERSION is set
if (NOT DEFINED MUSESCORE_VERSION)
    set(MUSESCORE_VERSION "v4.2.1")
    message(STATUS "Using apitypes.h from Musescore version: ${MUSESCORE_VERSION}")
endif()
set(MUSESCORE_API_TYPES ${CMAKE_CURRENT_SOURCE_DIR}/Sources/Musescore/apitypes.h)

if (NOT EXISTS ${MUSESCORE_API_TYPES})
    message(STATUS "Downloading Musescore API types")
    if (NOT EXISTS ${CMAKE_BINARY_DIR}/Sources/Musescore)
        message(STATUS "Creating Musescore directory")
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Sources/Musescore)
    endif()
    file(DOWNLOAD https://raw.githubusercontent.com/musescore/MuseScore/${MUSESCORE_VERSION}/src/framework/musesampler/internal/apitypes.h ${MUSESCORE_API_TYPES})

    # Remove #include "log.h"
    file(READ ${MUSESCORE_API_TYPES} MUSESCORE_API_TYPES_CONTENTS)
    string(REGEX REPLACE "#include[ \t]+\"log.h\"" "// #include \"log.h\"" MUSESCORE_API_TYPES_CONTENTS "${MUSESCORE_API_TYPES_CONTENTS}")
    file(WRITE ${MUSESCORE_API_TYPES} "${MUSESCORE_API_TYPES_CONTENTS}")
endif()

#╭──────────────────────────────────────╮
#│           PureData Object            │
#╰──────────────────────────────────────╯

set_pd_external_path(${CMAKE_CURRENT_SOURCE_DIR}/musesampler~)
file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Sources/*.cpp)
add_pd_external(pd-musesampler musesampler~ ${SOURCES})
set_target_properties(pd-musesampler PROPERTIES CXX_STANDARD 11)


# check if build is for release
if (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "")
    # add_custom_command(TARGET pd-musesampler POST_BUILD
    #     COMMAND ${CMAKE_COMMAND} -E copy
    #     ${CMAKE_CURRENT_SOURCE_DIR}/Resources/arabesque-C.mid
    #     ${CMAKE_CURRENT_SOURCE_DIR}/musesampler~/
    # )
    add_custom_command(TARGET pd-musesampler POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/Resources/musesampler~-help.pd
        ${CMAKE_CURRENT_SOURCE_DIR}/musesampler~/
    )
else()
    add_definitions(-DDEBUG)
    add_custom_command(TARGET pd-musesampler POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/musesampler~/musesampler~-help.pd
        ${CMAKE_CURRENT_SOURCE_DIR}/Resources/
    )

endif()





