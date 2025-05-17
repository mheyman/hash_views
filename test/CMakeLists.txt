cmake_minimum_required(VERSION 3.28)
include (GNUInstallDirs)

find_package(doctest CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(daw-json-link REQUIRED)

add_executable(unit_tests)

target_sources(
	unit_tests 
	PRIVATE
		main.cpp
		unit_tests.cpp
)

target_compile_features(unit_tests PRIVATE cxx_std_23)
target_compile_options(unit_tests PRIVATE "$<$<C_COMPILER_FRONTEND_VARIANT:MSVC>:/utf-8>")
target_compile_options(unit_tests PRIVATE "$<$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>:/utf-8>")
target_compile_options(
	unit_tests
	INTERFACE
			$<$<AND:$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>,$<CXX_COMPILER_FRONTEND_VARIANT:GNU>>:
			    -g -Werror -Wall -Wextra -Wshadow  -Wnon-virtual-dtor -Wold-style-cast  -Wcast-align  -Wunused  -Woverloaded-virtual  -Wpedantic  -Wconversion  -Wsign-conversion  -Wnull-dereference  -Wdouble-promotion  -Wformat=2  -Wimplicit-fallthrough  -Wno-c++17-compat -Wno-c++17-compat-pedantic -Wno-c++98-compat -Wno-c++98-compat-pedantic>
			$<$<CXX_COMPILER_ID:GNU>:
				-g -Werror -Wall -Wextra -Wshadow  -Wnon-virtual-dtor -Wold-style-cast  -Wcast-align  -Wunused  -Woverloaded-virtual  -Wpedantic  -Wconversion  -Wsign-conversion  -Wnull-dereference  -Wdouble-promotion  -Wformat=2  -Wimplicit-fallthrough -Wmisleading-indentation -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast -Wsuggest-override -fconcepts-diagnostics-depth=5>
			$<$<AND:$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:MSVC>>,$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>:
				/WX /W4 /w14242 /w14254 /w14263 /w14265 /w14287 /we4289 /w14296 /w14311 /w14545 /w14546 /w14547 /w14549 /w14555 /w14619 /w14640 /w14826 /w14905  /w14906  /w14928  /permissive->
)
target_compile_options(
	unit_tests
	INTERFACE
		$<$<CXX_COMPILER_ID:MSVC>:/Zc:externC-> # clang-cl doesn't have this but it is needed for MSVC
)
target_compile_options(
	unit_tests
	INTERFACE
		$<$<OR:$<CXX_COMPILER_FRONTEND_VARIANT:Clang>,$<CXX_COMPILER_FRONTEND_VARIANT:AppleClang>,$<$<CXX_COMPILER_FRONTEND_VARIANT:GNU>>:-fstack-protector-strong -fcf-protection -fstack-clash-protection -fsanitize=undefined -fsanitize-minimal-runtime -fno-sanitize-recover=undefined>
		$<$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>:/sdl /DYNAMICBASE /guard:cf>
)
target_compile_definitions(
	unit_tests
	INTERFACE
		$<$<OR:$<CXX_COMPILER_FRONTEND_VARIANT:Clang>,$<CXX_COMPILER_FRONTEND_VARIANT:AppleClang>,$<$<CXX_COMPILER_FRONTEND_VARIANT:GNU>>:D_GLIBCXX_ASSERTIONS>
		$<$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>:>
)
target_link_options(
	unit_tests
	INTERFACE
		$<$<OR:$<CXX_COMPILER_FRONTEND_VARIANT:Clang>,$<CXX_COMPILER_FRONTEND_VARIANT:AppleClang>,$<$<CXX_COMPILER_FRONTEND_VARIANT:GNU>>:-fstack-protector-strong -fcf-protection -fstack-clash-protection -fsanitize=undefined -fsanitize-minimal-runtime -fno-sanitize-recover=undefined>
		$<$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>:/NXCOMPAT /CETCOMPAT>
)

target_link_libraries(
	unit_tests 
	PRIVATE
		sph-hash
		doctest::doctest
		fmt::fmt
		daw::daw-json-link
)

target_include_directories(unit_tests PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
