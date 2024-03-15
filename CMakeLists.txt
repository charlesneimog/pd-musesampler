# Define your standard CMake header (for example):
cmake_minimum_required(VERSION 3.5)
include(${CMAKE_CURRENT_SOURCE_DIR}/Resources/pd.build/pd.cmake)

project(pd-musesampler)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG_MODE")

set_pd_external_path(${CMAKE_CURRENT_SOURCE_DIR}/${project_name})

set(PD_FLOATSIZE64 "OFF")

#╭──────────────────────────────────────╮
#│             Definitions              │
#╰──────────────────────────────────────╯
add_definitions(-DNO_QT_SUPPORT)



#╭──────────────────────────────────────╮
#│             Pd External              │
#╰──────────────────────────────────────╯
# Get all cpp files inside ${CMAKE_SOURCE_DIR}/Sources/ and add them to the project
file(GLOB EXTERNAL_SOURCES 
                        "${CMAKE_SOURCE_DIR}/Sources/*.cpp"
                        # "${CMAKE_SOURCE_DIR}/Sources/musescore/*.cpp"
                        # "${CMAKE_SOURCE_DIR}/Sources/musesampler/*.cpp"
)

add_pd_external(pd-musesampler musesampler~ ${EXTERNAL_SOURCES})
# target_include_directories(pd-musesampler PRIVATE "${CMAKE_SOURCE_DIR}/Libraries/utfcpp/source/")
# target_include_directories(pd-musesampler PRIVATE "${CMAKE_SOURCE_DIR}/Sources/musescore/")


    # (pd-musesampler PRIVATE "${CMAKE_SOURCE_DIR}/Libraries/utfcpp/source/")


# target_link_libraries(pd-musesampler PRIVATE simpl)
