cmake_minimum_required(VERSION 3.16)

if(DEFINED DEPENDENCIES_DIR)
	include(${DEPENDENCIES_DIR}/modorganizer_super/cmake_common/mo2.cmake)
else()
	include(${CMAKE_CURRENT_LIST_DIR}/../cmake_common/mo2.cmake)
endif()

project(bsa_extractor)
add_subdirectory(src)

find_package(binary_io REQUIRED CONFIG)
find_package(bsa REQUIRED CONFIG)
target_link_libraries(
	"${PROJECT_NAME}"
	PRIVATE
		binary_io::binary_io
		bsa::bsa
)
