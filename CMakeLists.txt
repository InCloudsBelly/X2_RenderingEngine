cmake_minimum_required(VERSION 3.9.1)

project(X2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

set(PROJECT_OUTPUT_DIR "${CMAKE_BINARY_DIR}/../bin")
set(PROJECT_BIN_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_DIR})
set(PROJECT_SOURCE_DIR "${CMAKE_SOURCE_DIR}/Engine")
set(PROJECT_LIBRARIES_DIR "${CMAKE_SOURCE_DIR}/3rdParty")
set(RESOURCE_DIR  "${CMAKE_SOURCE_DIR}/Resources")

# 递归查找文件夹下的 .h .hpp. ini 文件保存到 HEADER_FILES
file(GLOB_RECURSE HEADER_FILES  ${PROJECT_SOURCE_DIR}/*.h ${PROJECT_SOURCE_DIR}/*.hpp ${PROJECT_SOURCE_DIR}/*.ini) 

# 递归查找文件夹下的 *.cpp *.c 文件保存到 SOURCE_FILES
file(GLOB_RECURSE SOURCE_FILES ${PROJECT_SOURCE_DIR}/*.cpp ${PROJECT_SOURCE_DIR}/*.c)

# 将 HEDADER_FILES 和 SOURCE_FILES  保存到 AllFile 变量
set(AllFile ${HEADER_FILES} ${SOURCE_FILES})

# 对 AllFile 变量里面的所有文件分类(保留资源管理器的目录结构)
source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${AllFile})

if (CMAKE_BUILD_TYPE MATCHES Debug)
   set(CMAKE_CXX_FLAGS_DEBUG_INIT "-Wall")
else ()
   set(CMAKE_CXX_FLAGS_RELEASE_INIT "-Wall")
   add_definitions(-DRELEASE_MODE_ON=0)
endif ()



#####################################Libs######################################


# - Vulkan
find_package(Vulkan REQUIRED FATAL_ERROR)

# - GLFW
set(GLFW_DIR "${PROJECT_LIBRARIES_DIR}/glfw")
add_subdirectory(${GLFW_DIR} EXCLUDE_FROM_ALL)

# - GLM
set(GLM_DIR "${PROJECT_LIBRARIES_DIR}/glm")
add_subdirectory(${GLM_DIR} EXCLUDE_FROM_ALL)

# -STB(Image Loader) -> Header file that contains all the code.
set(STB_DIR "${PROJECT_LIBRARIES_DIR}/stb_image")
add_subdirectory(${STB_DIR} EXCLUDE_FROM_ALL)


# -ImGUI
# (compiles it as an static library)
set (IMGUI_DIR "${PROJECT_LIBRARIES_DIR}/imgui")
file(GLOB IMGUI_SOURCES
   ${IMGUI_DIR}/*.cpp
   ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp 
   ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp 
)
add_library(imgui ${IMGUI_SOURCES})
target_include_directories(
   imgui
   PUBLIC
      ${IMGUI_DIR}
      ${IMGUI_DIR}/backends
      ${Vulkan_INCLUDE_DIRS}
      ${GLFW_INCLUDE_DIRS}
)    
target_link_libraries(
    imgui
PUBLIC
    glfw 
    ${Vulkan_LIBRARIES}
)

# -Assimp
set(ASSIMP_DIR "${PROJECT_LIBRARIES_DIR}/assimp")
option(ASSIMP_BUILD_TESTS OFF)
add_subdirectory(${ASSIMP_DIR})

# - PThreads
set(THREADS_PREFER_PTRHEAD_FLAG ON)
find_package(Threads REQUIRED)


# - VMA
set(VMA_DIR "${PROJECT_LIBRARIES_DIR}/VulkanMemoryAllocator")
add_subdirectory(${VMA_DIR} EXCLUDE_FROM_ALL)

set(SPIRV_Cross_DIR "${PROJECT_LIBRARIES_DIR}/SPIRV-Cross")
add_subdirectory(${SPIRV_Cross_DIR} EXCLUDE_FROM_ALL)

set(SHADERC_DIR "${PROJECT_LIBRARIES_DIR}/shaderc")
set(SHADERC_SKIP_TESTS ON)
add_subdirectory(${SHADERC_DIR} EXCLUDE_FROM_ALL)

set(YAML_DIR "${PROJECT_LIBRARIES_DIR}/yaml-cpp")
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(${YAML_DIR} EXCLUDE_FROM_ALL)

set(OPTOCL_DIR "${PROJECT_LIBRARIES_DIR}/optick")
add_subdirectory(${OPTOCL_DIR} EXCLUDE_FROM_ALL)

set(MSDF_FONT_DIR "${PROJECT_LIBRARIES_DIR}/msdf-atlas-gen")
add_subdirectory(${MSDF_FONT_DIR} EXCLUDE_FROM_ALL)





set_target_properties(glfw assimp imgui UpdateAssimpLibsDebugSymbolsAndDLLs zlibstatic yaml-cpp OptickCore msdf-atlas-gen shaderc spirv-cross-core spirv-cross-glsl VulkanMemoryAllocator
    PROPERTIES 
    FOLDER 
    "ThirdParty"
)


################################Definitions####################################

add_definitions(-DPROJECT_ROOT="${CMAKE_SOURCE_DIR}/")
add_definitions(-DX2_DEBUG)
add_definitions(-DX2_TRACK_MEMORY)

#################################Executable####################################

add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})

target_include_directories(
   ${PROJECT_NAME}
   PUBLIC
      "${VMA_DIR}/include"
      "${Vulkan_INCLUDE_DIRS}"
      "${PROJECT_LIBRARIES_DIR}"
      "${GLFW_INCLUDE_DIRS}"
      "${IMGUI_DIR}"
      "${VOLK_DIR}"
      "${SHADERC_DIR}/libshaderc_util/include"
      "${SHADERC_DIR}/libshaderc/include"
      "${SHADERC_DIR}/glslc/src"
      "${YAML_DIR}/include"
      "${OPTOCL_DIR}/src"
      "${MSDF_FONT_DIR}/msdf-atlas-gen"
   PRIVATE
      "${PROJECT_SOURCE_DIR}"
)

target_link_libraries(${PROJECT_NAME} 
PRIVATE
    VulkanMemoryAllocator
PUBLIC
   glfw
   ${Vulkan_LIBRARIES}
   Threads::Threads  # Needed by GLFW.
   glm
   stb_image
   assimp
   imgui
   shaderc 
   yaml-cpp
   OptickCore
   msdf-atlas-gen
   spirv-cross-core
   spirv-cross-glsl
   PRIVATE
   ${CMAKE_DL_LIBS}
)

target_precompile_headers(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/Engine/Precompiled.h)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
# CMAKE_DL_LIBS -> is the library libdl which helps to link dynamic
# libraries. We need it in order to use Vulkan Loader.