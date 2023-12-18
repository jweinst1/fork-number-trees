# Project <proj> main cmake file

cmake_minimum_required(VERSION 3.3)

project(sntrees VERSION 0.0.1)


if(DEFINED ENV{SN_TREE_HOME})
     message("-- Will install SN_TREE_HOME binaries and libraries to location: " $ENV{SN_TREE_HOME})
	 set(CMAKE_INSTALL_PREFIX $ENV{SN_TREE_HOME})
else(DEFINED ENV{SN_TREE_HOME})
     message("-- Will install SN_TREE_HOME binaries and libraries to location: " ${CMAKE_CURRENT_BINARY_DIR})
	 set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_BINARY_DIR})
endif(DEFINED ENV{SN_TREE_HOME})


# Disable some windows specific warnings for using more modern c functions
if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif(MSVC)

include(CheckCSourceRuns)

check_c_source_runs("int main(void){short int number = 0x1; char *numPtr = (char*)&number; return (numPtr[0] == 1) ? 0 : 3;}" USE_LITTLE_ENDIAN)
check_c_source_runs("int main(void){short int number = 0x1; char *numPtr = (char*)&number; return (numPtr[0] == 0) ? 0: 1 ;}" USE_BIG_ENDIAN)

if(USE_LITTLE_ENDIAN)
    message(STATUS "Detected little endian architecture")
    add_definitions(-DFLOTON_ENDIAN_LIT)
else()
    if(USE_BIG_ENDIAN)
        message(STATUS "Detetcted big endian architecture")
        add_definitions(-DFLOTON_ENDIAN_BIG)
    else()
        message(FATAL "Cannot determine endian type")
    endif()
endif()

if(UNIX)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package( Threads REQUIRED )
endif()

# Options
option(WITH_testing "Allows tests to be run" ON)

if(WITH_testing MATCHES ON)
   enable_testing()
endif(WITH_testing MATCHES ON)
