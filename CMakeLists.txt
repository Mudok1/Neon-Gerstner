cmake_minimum_required(VERSION 3.20)
project(NeonGerstner VERSION 1.0 LANGUAGES CXX)

# Exportar comandos de compilacion para que el editor encuentre los headers
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# C++17 estandar
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Buscar paquetes instalados con vcpkg
find_package(glad CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

# Crear el ejecutable
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/AudioCapture.cpp
)

# Linkear librerias
target_link_libraries(${PROJECT_NAME} PRIVATE
    glad::glad
    glfw
    glm::glm
    Ole32
    Avrt
)

# Copiar shaders al directorio de build
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets/shaders
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
)
