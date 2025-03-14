set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(PREDEFINED_TARGETS_FOLDER "CustomTargets")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable Hot Reload for MSVC compilers if supported.
if(POLICY CMP0141)
	cmake_policy(SET CMP0141 NEW)
	set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
		"$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif(POLICY CMP0141)

# Specify C++ standard
set(CMAKE_CXX_STANDARD            20  )
set(CMAKE_CXX_STANDARD_REQUIRED   ON  )
set(CMAKE_CXX_EXTENSIONS          OFF )
set(CMAKE_MACOSX_RPATH            OFF )
set(CMAKE_THREAD_PREFER_PTHREAD   ON  )
set(THREADS_PREFER_PTHREAD_FLAG   ON  )

enable_language(CXX) # Need for CMAKE_MSVC_RUNTIME_LIBRARY, and at least cmake=3.15

if(NOT APPLE)
	set(CMAKE_CXX_CLANG_TIDY  clang-tidy)		# buggy piece of trash
endif(NOT APPLE)

# Add our tests to default build
set(BPE_TESTS 1)
