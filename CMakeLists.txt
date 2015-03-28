cmake_minimum_required( VERSION 2.8.8 )

project( ccc )

set( CMAKE_AUTOMOC TRUE )
set( CMAKE_INCLUDE_CURRENT_DIR TRUE )

file( GLOB INCLUDES "src/*.h" )
file( GLOB SOURCES "src/*.cpp" )

if( CMAKE_COMPILER_IS_GNUCXX )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11" )
endif()

add_executable( ccc ${INCLUDES} ${SOURCES} )