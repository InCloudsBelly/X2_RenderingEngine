find_package(Vulkan COMPONENTS glslangValidator)
find_program(GLSLANG_VALIDATOR_EXECUTABLE glslangValidator.exe)
find_program(glslc_executable NAMES glslc HINTS Vulkan::glslc)

file (GLOB SHADERS
   ${SHADER_SOURCE_DIR}/*.vert
   ${SHADER_SOURCE_DIR}/*.frag
   ${SHADER_SOURCE_DIR}/*.comp
)
 set(SPV_SHADERS "")

foreach(SHADER IN LISTS SHADERS)
   get_filename_component(FILENAME ${SHADER} NAME)
   get_filename_component(NAME_WITHOUT_EXT  ${SHADER} NAME_WE)

   get_filename_component(TMP_EXTENSION ${SHADER} EXT)
   string(REPLACE "." "" EXTENSION ${TMP_EXTENSION})

   set(OUTPUT_FILENAME "${NAME_WITHOUT_EXT}.${EXTENSION}")

   set(SPV_FILE "${SHADER_DIR}/Spv/${OUTPUT_FILENAME}.spv")
   #add_custom_command(
   #   OUTPUT ${SPV_FILE}
   #   COMMAND ${glslc_executable} -o ${SPV_FILE} ${SHADER}
   #   DEPENDS ${SHADER}
   #)

   execute_process(COMMAND ${GLSLANG_VALIDATOR_EXECUTABLE} -t -V ${SHADER} -I${SHADER_SOURCE_DIR}/Utils/ -o ${SPV_FILE} )

   message(" Converting ${SPV_FILE}")
   #message(" ${GLSLANG_VALIDATOR_EXECUTABLE} -t -V ${SHADER} -I${SHADER_SOURCE_DIR}/Utils/ -o ${SPV_FILE}")
   list(APPEND SPV_SHADERS ${SPV_FILE})
endforeach()

add_custom_target(Shaders 
        DEPENDS ${SPV_SHADERS} SOURCES ${SHADERS})