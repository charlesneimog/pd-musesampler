cmake_minimum_required(VERSION 3.15)
include(${CMAKE_CURRENT_SOURCE_DIR}/Resources/pd.build/pd.cmake) # pd.build
project(pd-musesampler)

set_pd_external_path(${CMAKE_CURRENT_SOURCE_DIR}/pd-musesampler)

file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Sources/*.cpp)

add_pd_external(pd-musesampler musesampler~ ${SOURCES})


