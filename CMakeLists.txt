cmake_minimum_required(VERSION 3.9.1)

project(X2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

set(PROJECT_OUTPUT_DIR "${CMAKE_BINARY_DIR}/../bin")
set(PROJECT_BIN_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_DIR})
set(PROJECT_SOURCE_DIR "${CMAKE_SOURCE_DIR}/X2Engine")
set(SHADER_DIR "${CMAKE_SOURCE_DIR}/Assets/Shader")
set(SHADER_SOURCE_DIR "${SHADER_DIR}/SourceCode")
set(PROJECT_LIBRARIES_DIR "${CMAKE_SOURCE_DIR}/3rdParty")
set(MODEL_DIR "${CMAKE_SOURCE_DIR}/Assets/Model")
set(SKYBOX_DIR  "${CMAKE_SOURCE_DIR}/Assets/SkyBox")

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

# - RTTR
set(RTTR_DIR "${PROJECT_LIBRARIES_DIR}/rttr")
add_subdirectory(${RTTR_DIR} EXCLUDE_FROM_ALL)

# - VMA
set(VMA_DIR "${PROJECT_LIBRARIES_DIR}/VulkanMemoryAllocator")
add_subdirectory(${VMA_DIR} EXCLUDE_FROM_ALL)

# - JSON
set(JSON_DIR "${PROJECT_LIBRARIES_DIR}/NlohmannJson")
add_subdirectory(${JSON_DIR} EXCLUDE_FROM_ALL)


set_target_properties(glfw assimp imgui UpdateAssimpLibsDebugSymbolsAndDLLs VulkanMemoryAllocator zlibstatic
    PROPERTIES 
    FOLDER 
    "ThirdParty"
)

##################################Shaders######################################

# - Compilation of shaders
add_subdirectory(${SHADER_DIR})



################################Definitions####################################

add_definitions(-DTEXTURES_DIR="${TEXTURES_DIR}/")
add_definitions(-DMODEL_DIR="${MODEL_DIR}/")
add_definitions(-DSKYBOX_DIR="${SKYBOX_DIR}/")
add_definitions(-DSHADER_DIR="${SHADER_DIR}/")

#################################Executable####################################

add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})

target_include_directories(
   ${PROJECT_NAME}
   PUBLIC
      "${Vulkan_INCLUDE_DIRS}"
      "${GLFW_INCLUDE_DIRS}"
      "${PROJECT_LIBRARIES_DIR}/rttr"
      ${NLOHMANN_JSON_INCLUDE_BUILD_DIR}
   PRIVATE
      "${PROJECT_SOURCE_DIR}"
)
message(" NLOHMANN_JSON ${NLOHMANN_JSON_INCLUDE_BUILD_DIR}")

target_link_libraries(${PROJECT_NAME} 
   PUBLIC
   glfw 
   ${Vulkan_LIBRARIES}
   Threads::Threads  # Needed by GLFW.
   glm
   stb_image
   assimp
   imgui
   VulkanMemoryAllocator 
   rttr
   nlohmann_json::nlohmann_json
   PRIVATE
   ${CMAKE_DL_LIBS}
)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
# CMAKE_DL_LIBS -> is the library libdl which helps to link dynamic
# libraries. We need it in order to use Vulkan Loader.