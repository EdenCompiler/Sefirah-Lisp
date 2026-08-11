cmake_minimum_required(VERSION 3.24)

project(SefirahLisp VERSION 0.0.1 LANGUAGES C)

include(GNUInstallDirs)

file(GLOB_RECURSE SEFIRAH_EXTENSOES_ANTIGAS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/exemplos/*.sef"
    "${CMAKE_SOURCE_DIR}/fonte/*.sef"
    "${CMAKE_SOURCE_DIR}/testes/*.sef"
)
if(SEFIRAH_EXTENSOES_ANTIGAS)
    message(FATAL_ERROR "Fontes Sefirah devem usar .lisp, nunca .sef: ${SEFIRAH_EXTENSOES_ANTIGAS}")
endif()

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(SEFIRAH_AVISOS_COMO_ERROS "Trata avisos do compilador como erros" OFF)
option(SEFIRAH_CONSTRUIR_TESTES "Constroi os testes automatizados" ON)

if(WIN32 AND CMAKE_CROSSCOMPILING AND NOT CMAKE_CROSSCOMPILING_EMULATOR)
    find_program(SEFIRAH_WINE_EXECUTAVEL wine)
    if(SEFIRAH_WINE_EXECUTAVEL)
        set(CMAKE_CROSSCOMPILING_EMULATOR "${SEFIRAH_WINE_EXECUTAVEL}")
    endif()
endif()

add_library(sefirah_compilador
    fonte/compilador/aarch64.c
    fonte/compilador/ir.c
    fonte/compilador/objeto_coff.c
    fonte/compilador/objeto_elf.c
    fonte/compilador/objeto_macho.c
    fonte/compilador/x64.c
)
target_include_directories(sefirah_compilador PUBLIC incluir)

add_library(sefirah_nucleo
    fonte/nucleo/ambiente.c
    fonte/nucleo/avaliador.c
    fonte/nucleo/biblioteca.c
    fonte/nucleo/codigo.c
    fonte/nucleo/compilacao.c
    fonte/nucleo/impressor.c
    fonte/nucleo/imagem.c
    fonte/nucleo/leitor.c
    fonte/nucleo/listas.c
    fonte/nucleo/primitivas.c
    fonte/nucleo/runtime.c
    fonte/nucleo/sequencias.c
    fonte/nucleo/tabela_hash.c
    fonte/nucleo/utf8.c
    fonte/nucleo/valores_multiplos.c
    fonte/nucleo/valor.c
)
target_include_directories(sefirah_nucleo PUBLIC incluir)
target_link_libraries(sefirah_nucleo PRIVATE sefirah_compilador ${CMAKE_DL_LIBS})

add_library(sefirah_graficos
    fonte/graficos/fonte_bitmap.c
    fonte/graficos/gui.c
    fonte/graficos/superficie.c
)
target_include_directories(sefirah_graficos PUBLIC incluir)

if(WIN32)
    set(SEFIRAH_FONTE_PLATAFORMA fonte/plataforma/janela_windows.c)
elseif(APPLE)
    set(SEFIRAH_FONTE_PLATAFORMA fonte/plataforma/janela_macos.c)
else()
    find_package(X11 REQUIRED)
    set(SEFIRAH_FONTE_PLATAFORMA fonte/plataforma/janela_x11.c)
endif()

add_library(sefirah_plataforma ${SEFIRAH_FONTE_PLATAFORMA})
target_include_directories(sefirah_plataforma PUBLIC incluir)
target_link_libraries(sefirah_plataforma PUBLIC sefirah_graficos)

if(WIN32)
    target_link_libraries(sefirah_plataforma PRIVATE user32 gdi32)
elseif(APPLE)
    find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
    find_library(COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)
    find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
    target_link_libraries(sefirah_plataforma PRIVATE
        ${APPKIT_FRAMEWORK} ${COREGRAPHICS_FRAMEWORK} ${FOUNDATION_FRAMEWORK} objc)
else()
    target_include_directories(sefirah_plataforma PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(sefirah_plataforma PRIVATE ${X11_LIBRARIES})
endif()

add_executable(sefirah fonte/cli/principal.c)
target_link_libraries(sefirah PRIVATE sefirah_nucleo)

add_library(sefirah_ide_nucleo fonte/ide/sessao.c)
target_include_directories(sefirah_ide_nucleo PUBLIC incluir)
target_link_libraries(sefirah_ide_nucleo PUBLIC sefirah_nucleo)

add_executable(sefirah_ide fonte/ide/principal.c fonte/ide/ide.c)
target_include_directories(sefirah_ide PRIVATE incluir)
target_link_libraries(sefirah_ide PRIVATE sefirah_ide_nucleo sefirah_plataforma)

foreach(alvo sefirah_compilador sefirah_nucleo sefirah_graficos sefirah_plataforma sefirah_ide_nucleo sefirah sefirah_ide)
    if(MSVC)
        target_compile_options(${alvo} PRIVATE /W4 /utf-8)
        if(SEFIRAH_AVISOS_COMO_ERROS)
            target_compile_options(${alvo} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${alvo} PRIVATE -Wall -Wextra -Wpedantic)
        if(SEFIRAH_AVISOS_COMO_ERROS)
            target_compile_options(${alvo} PRIVATE -Werror)
        endif()
    endif()
endforeach()

if(SEFIRAH_CONSTRUIR_TESTES)
    enable_testing()
    add_library(teste_ffi SHARED testes/biblioteca_ffi.c)
    add_executable(teste_nucleo testes/teste_nucleo.c)
    if(MSVC)
        target_compile_options(teste_nucleo PRIVATE /utf-8)
    endif()
    target_link_libraries(teste_nucleo PRIVATE sefirah_nucleo)
    add_test(NAME nucleo COMMAND teste_nucleo "./$<TARGET_FILE_NAME:teste_ffi>")
    set_tests_properties(nucleo PROPERTIES WORKING_DIRECTORY "$<TARGET_FILE_DIR:teste_ffi>")

    add_executable(teste_graficos testes/teste_graficos.c)
    target_link_libraries(teste_graficos PRIVATE sefirah_graficos)
    add_test(NAME graficos COMMAND teste_graficos)

    add_executable(teste_compilador testes/teste_compilador.c)
    target_link_libraries(teste_compilador PRIVATE sefirah_compilador)
    add_test(NAME compilador COMMAND teste_compilador)

    add_executable(teste_ide testes/teste_ide.c)
    target_link_libraries(teste_ide PRIVATE sefirah_ide_nucleo)
    add_test(NAME ide COMMAND teste_ide)

    add_test(NAME cli_sem_ide COMMAND sefirah --help)
    set_tests_properties(cli_sem_ide PROPERTIES PASS_REGULAR_EXPRESSION "compilar-elf")

    add_executable(exemplo_gerar_objeto_externo exemplos/integracao-c/gerar_objeto_externo.c)
    target_link_libraries(exemplo_gerar_objeto_externo PRIVATE sefirah_compilador)
    if(MSVC)
        target_compile_options(exemplo_gerar_objeto_externo PRIVATE /W4 /utf-8)
        if(SEFIRAH_AVISOS_COMO_ERROS)
            target_compile_options(exemplo_gerar_objeto_externo PRIVATE /WX)
        endif()
    else()
        target_compile_options(exemplo_gerar_objeto_externo PRIVATE -Wall -Wextra -Wpedantic)
        if(SEFIRAH_AVISOS_COMO_ERROS)
            target_compile_options(exemplo_gerar_objeto_externo PRIVATE -Werror)
        endif()
    endif()
endif()

install(TARGETS sefirah sefirah_ide sefirah_ide_nucleo sefirah_nucleo sefirah_compilador sefirah_graficos sefirah_plataforma
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY incluir/sefirah DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h")
install(DIRECTORY incluir/ide DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h")
install(FILES
    docs-ptbr/manual.md
    docs-ptbr/arquitetura.md
    docs-ptbr/compilador.md
    docs-ptbr/roteiro.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}/docs-ptbr
)
install(FILES
    docs-en/manual.md
    docs-en/architecture.md
    docs-en/compiler.md
    docs-en/roadmap.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}/docs-en
)
