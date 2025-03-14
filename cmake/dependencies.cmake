# dependencies.cmake : CMake of the project for T2Si Engine dependencies

include(FetchContent)

# Add Google Test

if(BPE_TESTS)
	set(CMAKE_FOLDER "googletest")
	FetchContent_Declare(
		googletest
		GIT_REPOSITORY    https://github.com/google/googletest.git
		GIT_TAG           v1.15.2
	)
	# For Windows: Prevent overriding the parent project's compiler/linker settings
	set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
	FetchContent_MakeAvailable(googletest)
	unset(CMAKE_FOLDER)

	# Disable clang-tidy for google-tests sources
	set_target_properties(gtest       PROPERTIES CXX_CLANG_TIDY "")
	set_target_properties(gtest_main  PROPERTIES CXX_CLANG_TIDY "")
	set_target_properties(gmock       PROPERTIES CXX_CLANG_TIDY "")
	set_target_properties(gmock_main  PROPERTIES CXX_CLANG_TIDY "")
	# Reset default compile flags for google-tests sources
	if(WIN32)
		set_target_properties(gtest       PROPERTIES MY_VS_WARNING_LEVEL 0)
		set_target_properties(gtest_main  PROPERTIES MY_VS_WARNING_LEVEL 0)
		set_target_properties(gmock       PROPERTIES MY_VS_WARNING_LEVEL 0)
		set_target_properties(gmock_main  PROPERTIES MY_VS_WARNING_LEVEL 0)
		target_compile_options(gtest       PRIVATE /analyze-)
		target_compile_options(gtest_main  PRIVATE /analyze-)
		target_compile_options(gmock       PRIVATE /analyze-)
		target_compile_options(gmock_main  PRIVATE /analyze-)
	endif(WIN32)
endif()
