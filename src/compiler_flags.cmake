

#external code added if exists
if(EXISTS my_compiler_flags.cmake ) 
      include(my_compiler_flags.cmake)
else()

	if (CMAKE_BUILD_TYPE STREQUAL "Release" )
		# Release build, all compilers
		add_definitions(-DNDEBUG)
		add_definitions(-DQT_NO_DEBUG)
	endif()

	if (WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
		# mingw
		target_link_options(${TARGET_PROJECT} PRIVATE -lKernel32 -lpsapi -lBcrypt)
	endif()
	
	if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
		# for gcc
		target_compile_options(${TARGET_PROJECT} PRIVATE -march=native -fopenmp -fPIC -mno-bmi2 -mno-fma -mno-avx -std=gnu++14 -Wno-maybe-uninitialized)
		target_link_options(${TARGET_PROJECT} PRIVATE -lgomp -lz)
		
		if (CMAKE_BUILD_TYPE STREQUAL "Release" )
			# gcc release
			target_compile_options(${TARGET_PROJECT} PRIVATE -O3 -ftree-vectorize )
		endif()
	endif()
	
	if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
		# for msvc
		add_definitions(-DNOMINMAX)
		#if (CMAKE_BUILD_TYPE STREQUAL "Release" )
			#target_compile_options(${TARGET_PROJECT} PRIVATE /O2 /MP)
		#else()
			target_compile_options(${TARGET_PROJECT} PRIVATE /MP)
		#endif()
		target_link_libraries(${TARGET_PROJECT} PRIVATE opengl32)
	endif()
	
	# add openmp for all
	find_package(OpenMP)
	if (OPENMP_FOUND)
		set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
		set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
		set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
	endif()
	
	
	if (WIN32)
		add_definitions("-DNOMINMAX")
		add_definitions("-DWIN64")
		add_definitions("-DDISABLE_WINRT_DEPRECATION")
		add_definitions("-D_WINDOWS")
		add_definitions("-DUNICODE")
		add_definitions("-D_USE_MATH_DEFINES")
		add_definitions("-D_WIN32")
	endif()
	
	if(UNIX AND NOT CMAKE_BUILD_TYPE STREQUAL Debug)
		target_link_libraries(${TARGET_PROJECT} PUBLIC z)
	endif()
	
endif() #end exists( my_compiler_flags.cmake ) 