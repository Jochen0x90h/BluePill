cmake_minimum_required(VERSION 3.9)

# enable cross compiling
include(CMakeForceCompiler)
set(CMAKE_SYSTEM_NAME Generic)
CMAKE_FORCE_C_COMPILER(arm-none-eabi-gcc GNU)
CMAKE_FORCE_CXX_COMPILER(arm-none-eabi-g++ GNU)

# project
project(BluePill)

# place libopencm3 into project root directory and make it
# https://github.com/libopencm3
set(OPENCM3 "${CMAKE_CURRENT_SOURCE_DIR}/libopencm3")


# config
set(CPU_FLAGS 
	-mcpu=cortex-m3 # Target CPU
	-DSTM32F1 # Define the series of MCU we are using. This controls the include pathways inside libopencm3
	-mthumb # Use thumb mode (smaller instructions)
	-ggdb3 # Include additional debug info
	-ffunction-sections # Generate separate ELF section for each function
	-fdata-sections # Enable elf section per variable
)


# compiler options for all build types
add_definitions(-Wall)

include_directories(
	.
	${OPENCM3}/include
)

# executable
add_executable(bluepill
	main.c
)
target_compile_options(bluepill PUBLIC ${CPU_FLAGS})
set_target_properties(bluepill PROPERTIES
	LINK_FLAGS "--static -nostartfiles -Wl,-Map=main.map -Wl,--cref -Wl,--gc-sections -L${OPENCM3}/lib/stm32/f1 -L${OPENCM3}/lib -Tstm32f103x8.ld -Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group"
)

target_link_libraries(bluepill opencm3_stm32f1)
