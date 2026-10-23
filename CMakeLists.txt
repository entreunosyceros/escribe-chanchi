cmake_minimum_required(VERSION 3.16)

project(MarkdownEditor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Buscar paquetes de Qt6. La ventana en uso no necesita WebEngine.
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network PrintSupport)

# Buscar librerías de sistema
find_package(PkgConfig REQUIRED)
pkg_check_modules(CMARK REQUIRED libcmark)

# Habilitar automoc para Qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Definir archivos fuente
set(SOURCES
    main.cpp
    markdow_editor.cpp
    markdown_formatter.cpp
    template_manager.cpp
)

# Definir archivos de cabecera
set(HEADERS
    markdow_editor.h
    markdown_formatter.h
    template_manager.h
)

# Definir archivos de recursos
set(RESOURCES
    resources.qrc
)

# Crear el ejecutable
add_executable(Escribe_Chanchi ${SOURCES} ${HEADERS} ${RESOURCES})

# Vincular librerías de Qt
target_link_libraries(Escribe_Chanchi Qt6::Core Qt6::Widgets Qt6::Network Qt6::PrintSupport ${CMARK_LIBRARIES})

# Agregar directorios de include para cmark
target_include_directories(Escribe_Chanchi PRIVATE ${CMARK_INCLUDE_DIRS})
