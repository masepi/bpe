
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	# Define a target property which will contain desired warning level
	# for a target (library or executable)
	define_property(TARGET PROPERTY MY_VS_WARNING_LEVEL
		BRIEF_DOCS "Warning level"
		FULL_DOCS "Warning level for Visual Studio compiler: 0, 1, 2, 3, 4"
		INHERITED # The property will be initialized from the directory property.
	)
	# Define a **directory** property with the same name
	# It will be used via inheritance and provide default value for the target property.
	define_property(DIRECTORY PROPERTY MY_VS_WARNING_LEVEL
		BRIEF_DOCS "Warning level"
		FULL_DOCS "Warning level for Visual Studio compiler: 0, 1, 2, 3, 4"
		INHERITED # The property will be initialized from the parent property.
	)
	# Set default value of the target property -- max warnings
	set_directory_properties(PROPERTIES  MY_VS_WARNING_LEVEL  4)


	# Link the cpp-runtime library statically of type, depending on config
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE INTERNAL "")

	# Add compiler options which depends on the target property
	add_compile_options(
		$<$<COMPILE_LANGUAGE:CXX>:/ZI>                                        # Keep debug information in PDB, and edit and continue
		$<$<COMPILE_LANGUAGE:CXX>:/EHsc>                                      # Safe Exception Handling C++ std and multi-threaded
		$<$<COMPILE_LANGUAGE:CXX>:/W$<TARGET_PROPERTY:MY_VS_WARNING_LEVEL>>   # Enable all warnings (default)
		$<$<COMPILE_LANGUAGE:CXX>:/WX>                                        # All warnings as errors
		$<$<COMPILE_LANGUAGE:CXX>:/permissive->                                # Additional warnings
		$<$<COMPILE_LANGUAGE:CXX>:/analyze>                                   # Enable static analyzer
		$<$<COMPILE_LANGUAGE:CXX>:/D_CRT_SECURE_NO_WARNINGS>                  # Disable warnings of c-runtime
		/wd6326
	)

	add_compile_options(
		$<$<CONFIG:Release>:/MT>          # Runtime library: Multi-threaded
		$<$<CONFIG:RelWithDebInfo>:/MT>   # Runtime library: Multi-threaded
		$<$<CONFIG:Debug>:/MTd>           # Runtime library: Multi-threaded Debug
	)

elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	# https://best.openssf.org/Compiler-Hardening-Guides/Compiler-Options-Hardening-Guide-for-C-and-C++.html
	# Recommended compiler options that enable strictly compile-time checks
	add_compile_options(
		-fvisibility=hidden                        # All functions are default hidden
		-Wall -Wextra                              # Enable warnings for constructs often associated with defects
		-pedantic -pedantic-errors                 # Enable extra warnings warnings
		-Werror                                    # Treat all or selected compiler warnings as errors. Use the blanket form -Werror only during development, not in source distribution.
		-Werror=format-security                    # Treat format strings that are not string literals and used without arguments as errors
		-Wformat                                   # Enable additional format function warnings
		-Wconversion -Wsign-conversion             # Enable implicit conversion warnings
		-Wimplicit-fallthrough                     # Warn when a switch case falls through
	)
else()
	message(FATAL_ERROR "Wrong compiler-id ${CMAKE_CXX_COMPILER_ID}")
endif()

# Allow asserts in release builds

# On non-Debug builds cmake automatically defines NDEBUG, so we explicitly undefine it:
if(CMAKE_BUILD_TYPE STREQUAL "Release")
	# NOTE: use `add_compile_options` rather than `add_definitions` since
	# `add_definitions` does not support generator expressions.
	add_compile_options($<$<OR:$<COMPILE_LANGUAGE:C>,$<COMPILE_LANGUAGE:CXX>>:-UNDEBUG>)

	# Also remove /D NDEBUG to avoid MSVC warnings about conflicting defines.
	string(REGEX REPLACE "(^| )[/-]D *NDEBUG($| )"  " "  CMAKE_CXX_FLAGS_RELEASE  "${CMAKE_CXX_FLAGS_RELEASE}" )
	string(REGEX REPLACE "(^| )[/-]D *NDEBUG($| )"  " "  CMAKE_C_FLAGS_RELEASE    "${CMAKE_C_FLAGS_RELEASE}"   )
endif()

