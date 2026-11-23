cmake_minimum_required(VERSION 3.25)

project(CppVisualIDE LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

if(WIN32)
    set(APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/installer/app.ico")
    configure_file(src/app.rc.in ${CMAKE_CURRENT_BINARY_DIR}/app.rc @ONLY)
endif()

qt_add_executable(CppVisualIDE WIN32
    resources.qrc
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/CodeEditor.cpp
    src/CodeEditor.h
    src/SyntaxHighlighter.cpp
    src/SyntaxHighlighter.h
    src/FormModel.cpp
    src/FormModel.h
    src/Project.cpp
    src/Project.h
    src/CodeGenerator.cpp
    src/CodeGenerator.h
    src/FormCanvas.cpp
    src/FormCanvas.h
    src/FormDesigner.cpp
    src/FormDesigner.h
    src/LspClient.cpp
    src/LspClient.h
    src/UiForm.cpp
    src/UiForm.h
)

if(WIN32)
    target_sources(CppVisualIDE PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/app.rc)
endif()

target_include_directories(CppVisualIDE PRIVATE src)
target_link_libraries(CppVisualIDE PRIVATE Qt6::Widgets)

if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt6 windeployqt
        HINTS "C:/msys64/mingw64/bin"
    )
    find_program(POWERSHELL_EXECUTABLE NAMES powershell.exe pwsh.exe)
    if(NOT WINDEPLOYQT_EXECUTABLE OR NOT POWERSHELL_EXECUTABLE)
        message(WARNING "windeployqt ou PowerShell não encontrado: DLLs não serão copiadas automaticamente.")
    else()
    add_custom_command(TARGET CppVisualIDE POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations --no-opengl-sw --no-system-d3d-compiler
                "$<TARGET_FILE:CppVisualIDE>"
        COMMAND "${POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass
                -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/deploy-runtime.ps1"
                "$<TARGET_FILE:CppVisualIDE>" -IncludeDesigner
        COMMENT "Copiando DLLs do Qt e dependências do MinGW..."
        VERBATIM
    )
    endif()
endif()

if(MSVC)
    target_compile_options(CppVisualIDE PRIVATE /W4)
else()
    target_compile_options(CppVisualIDE PRIVATE -Wall -Wextra)
endif()
