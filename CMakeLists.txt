cmake_minimum_required(VERSION 3.16)
project(isp_3a_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ISP3A_BUILD_DOCS "Build Doxygen documentation target" OFF)

find_package(OpenCV QUIET)

if(NOT OpenCV_FOUND)
	find_package(PkgConfig QUIET)
	if(PkgConfig_FOUND)
		pkg_check_modules(OPENCV4 QUIET opencv4)
		if(OPENCV4_FOUND)
			set(OpenCV_FOUND TRUE)
			set(OpenCV_INCLUDE_DIRS ${OPENCV4_INCLUDE_DIRS})
			set(OpenCV_LIBS ${OPENCV4_LIBRARIES})
		endif()
	endif()
endif()

add_executable(
	isp_3a_demo
	main.cpp
	src/isp3a/simulation.cpp
	src/isp3a/image_mode.cpp
)

target_include_directories(isp_3a_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(OpenCV_FOUND)
	target_link_libraries(isp_3a_demo PRIVATE ${OpenCV_LIBS})
	target_include_directories(isp_3a_demo PRIVATE ${OpenCV_INCLUDE_DIRS})
	target_compile_definitions(isp_3a_demo PRIVATE ISP3A_HAVE_OPENCV=1)
else()
	message(WARNING "OpenCV not found. Image mode will be disabled; build still supports simulation mode.")
endif()

if(ISP3A_BUILD_DOCS)
	find_package(Doxygen QUIET)
	if(DOXYGEN_FOUND)
		add_custom_target(
			doc
			COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
			COMMENT "Generating Doxygen HTML documentation"
			VERBATIM
		)
	else()
		message(WARNING "Doxygen not found. Install doxygen to enable the 'doc' target.")
	endif()
endif()

