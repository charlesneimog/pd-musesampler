cmake_minimum_required(VERSION 3.15)

project(musesampler~)

if (NOT DEFINED NEIMOG_LIBRARY)
    set(PDCMAKE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Resources  CACHE PATH "Path to pd.cmake")
    include(${PDCMAKE_DIR}/pd.cmake)
endif()

#╭──────────────────────────────────────╮
#│          MuseScore Headers           │
#╰──────────────────────────────────────╯

if (NOT DEFINED MUSESCORE_VERSION)
    set(MUSESCORE_VERSION "v4.3.0")
    message(STATUS "Using apitypes.h and libhandler.h from Musescore version: ${MUSESCORE_VERSION}")
endif()

set(MUSESCORE_APITYPES ${CMAKE_CURRENT_SOURCE_DIR}/Sources/Musescore/apitypes.h)
set(MUSESCORE_LIBHANDLER ${CMAKE_CURRENT_SOURCE_DIR}/Sources/Musescore/libhandler.h)

function(CommentLine file line)
    file(READ ${file} FILE_CONTENTS)
    string(REPLACE "${line}" "// ${line}" FILE_CONTENTS "${FILE_CONTENTS}")
    file(WRITE ${file} "${FILE_CONTENTS}")
endfunction()

function(ReplaceLine file line new_line)
    file(READ ${file} FILE_CONTENTS)
    string(REPLACE "${line}" "${new_line}" FILE_CONTENTS "${FILE_CONTENTS}")
    file(WRITE ${file} "${FILE_CONTENTS}")
endfunction()

if (NOT EXISTS ${MUSESCORE_APITYPES})
    message(STATUS "Downloading Musescore API types")
    if (NOT EXISTS ${CMAKE_BINARY_DIR}/Sources/Musescore)
        message(STATUS "Creating Musescore directory")
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Sources/Musescore)
    endif()
    file(DOWNLOAD https://raw.githubusercontent.com/musescore/MuseScore/${MUSESCORE_VERSION}/src/framework/musesampler/internal/apitypes.h ${MUSESCORE_APITYPES})
    file(DOWNLOAD https://raw.githubusercontent.com/musescore/MuseScore/${MUSESCORE_VERSION}/src/framework/musesampler/internal/libhandler.h ${MUSESCORE_LIBHANDLER})

    # MuseScore Api Types Changes
    CommentLine(${MUSESCORE_APITYPES} "#include \"log.h\"")
    CommentLine(${MUSESCORE_APITYPES} "#include \"dlib.h\"")

    # libhandler
    ReplaceLine(${MUSESCORE_LIBHANDLER} "#include \"types/version.h\"" "#include \"../pd-helpers.h\"")
    ReplaceLine(${MUSESCORE_LIBHANDLER} "MuseSamplerLibHandler(const io::path_t& path)" "MuseSamplerLibHandler(std::string path)")
    CommentLine(${MUSESCORE_LIBHANDLER} "const framework::Version minimumSupported{ 0, 2, 2 };")
    CommentLine(${MUSESCORE_LIBHANDLER} "#include \"types/version.h\"")
    CommentLine(${MUSESCORE_LIBHANDLER} "#include \"dlib.h\"")
    ReplaceLine(${MUSESCORE_LIBHANDLER} "framework::Version current(getVersionMajor()," "Version current(getVersionMajor(),")

endif()

#╭──────────────────────────────────────╮
#│           PureData Object            │
#╰──────────────────────────────────────╯

if (NEIMOG_LIBRARY)
    add_library(pd-musesampler STATIC ${FOLLOWER})
    set_target_properties(pd-musesampler PROPERTIES CXX_STANDARD 20)
else()
    set_pd_external_path(${CMAKE_CURRENT_SOURCE_DIR}/musesampler~)
    file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Sources/*.cpp)
    add_pd_external(musesampler~ ${SOURCES})
    set_target_properties(musesampler_tilde PROPERTIES CXX_STANDARD 20)
endif()








