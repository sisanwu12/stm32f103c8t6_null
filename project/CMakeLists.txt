# 这里要求的 CMake 版本与 flake.nix 中提供的 cmake 版本保持一致。
cmake_minimum_required(VERSION 4.1.2)

# 定义项目名称，并声明当前工程使用 C 与 ASM 语言。
project(null_proj LANGUAGES C ASM)

# 导出 compile_commands.json，方便 Nix 环境中的 clangd 做跳转、补全与诊断。
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# 显式启用 C23 标准。
set(CMAKE_C_STANDARD 23)
# 要求编译器必须真正支持 C23，而不是自动退回到更老的标准。
set(CMAKE_C_STANDARD_REQUIRED ON)
# 关闭 GNU 扩展，尽量按纯 C23 标准编译。
set(CMAKE_C_EXTENSIONS OFF)

# 统一整理工程中的关键路径，后面引用文件时都走这些变量。
get_filename_component(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(LIB_DIR "${ROOT_DIR}/Libraries")
set(CMSIS_DIR "${ROOT_DIR}/CMSIS")
set(CMSIS_CORE_INC_DIR "${CMSIS_DIR}/Core/Include")
set(CMSIS_DEVICE_INC_DIR "${CMSIS_DIR}/Device/ST/STM32F1xx/Include")
set(DRIVER_DIR "${LIB_DIR}/driver")
set(USER_DIR "${ROOT_DIR}/user")
set(RTOS_DIR "${ROOT_DIR}/RTOS")
set(RTOS_INC_DIR "${RTOS_DIR}/include")
set(RTOS_PORT_DIR "${RTOS_DIR}/portable")
set(LINKER_SCRIPT "${LIB_DIR}/stm32f103c8t6.ld")

# 在配置阶段打印出当前实际使用的工具链信息，便于确认确实来自 Nix dev shell。
message(STATUS "Nix toolchain C compiler : ${CMAKE_C_COMPILER}")
message(STATUS "Nix toolchain ASM compiler : ${CMAKE_ASM_COMPILER}")
message(STATUS "Recommended generator from flake.nix : Ninja")
message(STATUS "C23 enabled : C standard = C${CMAKE_C_STANDARD}, extensions = ${CMAKE_C_EXTENSIONS}")
message(STATUS "compile_commands.json export : ${CMAKE_EXPORT_COMPILE_COMMANDS}")

# Cortex-M3 + Thumb 指令集是 STM32F103C8T6 的基础目标架构配置。
set(MCU_FLAGS
    -mcpu=cortex-m3
    -mthumb
    -mfloat-abi=soft
)

# 这些选项只给 C 源码使用：
# -Os：优先缩小代码体积
# -g：保留调试信息
# -ffreestanding：告诉编译器这是裸机环境，不是假定存在宿主操作系统
# -ffunction-sections/-fdata-sections：把函数和数据拆段，方便链接时做垃圾回收
set(COMMON_COMPILE_OPTIONS
    ${MCU_FLAGS}
    -Os
    -g
    -Wall
    -Wextra
    -ffreestanding
    -ffunction-sections
    -fdata-sections
)

# 当前工程目录结构已经调整过，这里按目录自动收集源码，避免每次新增文件都手工维护列表。
file(GLOB LIBRARY_SOURCES CONFIGURE_DEPENDS "${LIB_DIR}/*.c")
file(GLOB DRIVER_SOURCES CONFIGURE_DEPENDS "${DRIVER_DIR}/*.c")
file(GLOB RTOS_CORE_SOURCES CONFIGURE_DEPENDS "${RTOS_DIR}/*.c")
file(GLOB RTOS_PORT_SOURCES CONFIGURE_DEPENDS "${RTOS_PORT_DIR}/*.c")
file(GLOB APP_SOURCES CONFIGURE_DEPENDS "${USER_DIR}/*.c")

list(SORT LIBRARY_SOURCES)
list(SORT DRIVER_SOURCES)
list(SORT RTOS_CORE_SOURCES)
list(SORT RTOS_PORT_SOURCES)
list(SORT APP_SOURCES)

# 启动文件负责中断向量表、复位入口、.data 拷贝与 .bss 清零；
# 其余 Libraries/*.c 视作系统支持文件，例如 system_stm32f103c8t6.c。
set(STARTUP_SOURCES "")
set(SYSTEM_SOURCES "")
foreach(source IN LISTS LIBRARY_SOURCES)
    if(source MATCHES "/startup_[^/]+\\.c$")
        list(APPEND STARTUP_SOURCES "${source}")
    else()
        list(APPEND SYSTEM_SOURCES "${source}")
    endif()
endforeach()

# RTOS 内核与端口层源码统一在这里维护，确保这些源文件也进入 compile_commands.json。
set(RTOS_SOURCES
    ${RTOS_CORE_SOURCES}
    ${RTOS_PORT_SOURCES}
)

# 如果应用层源码列表为空，就在配置阶段直接报错，避免生成一个不可链接的工程。
if(NOT APP_SOURCES)
    message(FATAL_ERROR
        "APP_SOURCES is empty. Add the application source file that defines main(void) "
        "to APP_SOURCES in project/CMakeLists.txt."
    )
endif()

# 如果当前目录结构里没有找到任何源码，也在配置阶段尽早报错。
set(PROJECT_SOURCES
    ${STARTUP_SOURCES}
    ${SYSTEM_SOURCES}
    ${DRIVER_SOURCES}
    ${RTOS_SOURCES}
    ${APP_SOURCES}
)

if(NOT PROJECT_SOURCES)
    message(FATAL_ERROR
        "No source files were found. Check the current project layout under "
        "Libraries/, Libraries/driver/, RTOS/, RTOS/portable/, and user/."
    )
endif()

# 生成最终固件目标：把启动文件、系统文件、驱动文件、RTOS 文件和应用文件一起链接成一个 ELF。
add_executable(null_proj
    ${PROJECT_SOURCES}
)

# 这里集中设置固件目标的属性：
# - OUTPUT_NAME/SUFFIX：控制输出文件名为 null_proj.elf
# - LINK_DEPENDS：链接脚本改动后自动触发重新链接
# - C_STANDARD 等：再次在目标级别明确要求 C23
set_target_properties(null_proj PROPERTIES
    OUTPUT_NAME "null_proj"
    SUFFIX ".elf"
    LINK_DEPENDS "${LINKER_SCRIPT}"
    C_STANDARD 23
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

# 传入精确芯片型号宏，供 CMSIS 设备头和条件编译使用。
target_compile_definitions(null_proj PRIVATE STM32F103xB)

# 为编译器补充头文件搜索路径。
target_include_directories(null_proj PRIVATE
    "${LIB_DIR}"
    "${CMSIS_CORE_INC_DIR}"
    "${CMSIS_DEVICE_INC_DIR}"
    "${DRIVER_DIR}"
    "${RTOS_INC_DIR}"
    "${RTOS_PORT_DIR}"
    "${USER_DIR}"
)

# 这里用了 CMake 的生成器表达式：
# - $<$<COMPILE_LANGUAGE:C>:...>   表示后面的选项只作用于 C 源文件
# - $<$<COMPILE_LANGUAGE:ASM>:...> 表示后面的选项只作用于汇编源文件
target_compile_options(null_proj PRIVATE
    $<$<COMPILE_LANGUAGE:C>:${COMMON_COMPILE_OPTIONS}>
    $<$<COMPILE_LANGUAGE:ASM>:${MCU_FLAGS}>
)

# 统一约定构建产物的路径与名称。
set(MAP_FILE "${CMAKE_BINARY_DIR}/null_proj.map")
set(BIN_FILE "${CMAKE_BINARY_DIR}/null_proj.bin")
set(HEX_FILE "${CMAKE_BINARY_DIR}/null_proj.hex")

# 链接阶段的关键参数说明：
# -T<ldscript>        指定 STM32F103C8T6 的链接脚本
# -nostartfiles       不使用宿主平台默认启动文件，改用我们自己的 startup 文件
# --specs=nosys.specs 使用裸机环境的最小 syscall 桩，避免依赖宿主系统
# --gc-sections       配合前面的拆段选项，去掉没有被引用的代码和数据
# -Map                额外生成 map 文件，方便分析段布局和符号分布
target_link_options(null_proj PRIVATE
    ${MCU_FLAGS}
    -T${LINKER_SCRIPT}
    -nostartfiles
    --specs=nosys.specs
    -Wl,--gc-sections
    -Wl,-Map,${MAP_FILE}
)

# 构建完成后，把 ELF 再转换成常见的烧录文件格式：
# - binary：纯裸二进制
# - ihex：Intel HEX 格式
# 同时打印 size，快速查看镜像体积。
add_custom_command(TARGET null_proj POST_BUILD
    COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:null_proj>" "${BIN_FILE}"
    COMMAND "${CMAKE_OBJCOPY}" -O ihex "$<TARGET_FILE:null_proj>" "${HEX_FILE}"
    COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:null_proj>"
    COMMENT "Generating binary, hex, and size reports"
)
