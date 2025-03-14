cmake_minimum_required (VERSION 3.26)
message(STATUS "cmake version ${CMAKE_VERSION}") 

project(bpe)

include(cmake/config.cmake)
include(cmake/compile_flags.cmake)
include(cmake/dependencies.cmake)

enable_testing()

set(BPE_SOURCES
	inc/bpe.h
	src/bpe.cpp
	inc/mapped_storages.h
	src/mapped_storages.cpp
	inc/to.h
)

# Static library sources
add_library(bpe STATIC
	${BPE_SOURCES}
)

target_include_directories(bpe PRIVATE
	inc
)

# Compile the unit-tests
if(BPE_TESTS)
	add_subdirectory(tests)
endif()


# INSTALL

# Exclude all dependencies from INSTALL
install(CODE "set(CMAKE_INSTALL_LOCAL_ONLY TRUE)" ALL_COMPONENTS)

# Install to current folder instead of system path
set(CMAKE_INSTALL_PREFIX .)

# Install by files
install(FILES  ${CMAKE_SOURCE_DIR}/inc/bpe.h  DESTINATION bin/${CMAKE_BUILD_TYPE}/inc)

# Install by targets
install(TARGETS
	bpe

	ARCHIVE        DESTINATION  bin/${CMAKE_BUILD_TYPE}/lib
	LIBRARY        DESTINATION  bin/${CMAKE_BUILD_TYPE}
	RUNTIME        DESTINATION  bin/${CMAKE_BUILD_TYPE}
)
