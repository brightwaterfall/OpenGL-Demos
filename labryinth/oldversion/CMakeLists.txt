cmake_minimum_required(VERSION 3.10)
project(labryinth_oldversion LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(app test.cpp)

# test.cpp includes GL/glew.h, GLFW/glfw3.h and png.h, and nothing else. GLUT
# used to be required here and is not used by anything, which made this fail to
# configure on machines that only have GLEW, GLFW and libpng.
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)

# PNG, not png: the bundled module is FindPNG.cmake, so the lowercase spelling
# found nothing and left PNG_FOUND empty, skipping the include and link lines.
find_package(PNG REQUIRED)

find_package(glfw3 CONFIG QUIET)
if(NOT TARGET glfw AND NOT TARGET glfw3)
  find_package(glfw3 REQUIRED)
endif()

target_link_libraries(app PRIVATE OpenGL::GL GLEW::GLEW PNG::PNG)

if(TARGET glfw)
  target_link_libraries(app PRIVATE glfw)
else()
  target_link_libraries(app PRIVATE glfw3)
endif()

if(WIN32)
  # Was `raytracer`, a target this file never defines, so configuring on
  # Windows failed outright.
  target_compile_definitions(app PRIVATE GLEW_STATIC)
endif()
