# Add all tests files to compilation
file(GLOB_RECURSE
	BPE_TESTS_SRC
	${CMAKE_SOURCE_DIR}/tests/*.cpp
)

# Create an executable for Tests
add_executable(bpe_tests
	${BPE_TESTS_SRC}
)

# Include header files directories
target_include_directories(bpe_tests  PRIVATE
	${CMAKE_SOURCE_DIR}/inc
	${CMAKE_SOURCE_DIR}/tests
)

# Link tests static dependencies
target_link_libraries(bpe_tests PRIVATE
	GTest::gtest_main
	bpe
)

# Definition
target_compile_definitions(bpe_tests  PRIVATE
	BPE_TESTS
)

include(GoogleTest)

gtest_discover_tests(bpe_tests
	DISCOVERY_MODE     PRE_TEST                       
	WORKING_DIRECTORY  ${CMAKE_SOURCE_DIR}/tests
)

add_definitions(-DTEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests")
