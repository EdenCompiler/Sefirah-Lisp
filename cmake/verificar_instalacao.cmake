if(NOT DEFINED SEFIRAH_PREFIXO)
    message(FATAL_ERROR "SEFIRAH_PREFIXO was not provided")
endif()

if(WIN32)
    set(SEFIRAH_EXTENSAO_EXECUTAVEL ".exe")
else()
    set(SEFIRAH_EXTENSAO_EXECUTAVEL "")
endif()

set(SEFIRAH_ARQUIVOS_PUBLICOS
    "bin/sefirah${SEFIRAH_EXTENSAO_EXECUTAVEL}"
    "bin/sefirah_ide${SEFIRAH_EXTENSAO_EXECUTAVEL}"
    "include/sefirah/runtime.h"
    "include/sefirah/compilador.h"
    "include/sefirah/gui.h"
    "include/ide/ide.h"
    "share/doc/SefirahLisp/docs-en/manual.md"
    "share/doc/SefirahLisp/docs-ptbr/manual.md"
)

foreach(SEFIRAH_ARQUIVO IN LISTS SEFIRAH_ARQUIVOS_PUBLICOS)
    if(NOT EXISTS "${SEFIRAH_PREFIXO}/${SEFIRAH_ARQUIVO}")
        message(FATAL_ERROR "missing installed artifact: ${SEFIRAH_ARQUIVO}")
    endif()
endforeach()

if(EXISTS "${SEFIRAH_PREFIXO}/include/sefirah/interno.h")
    message(FATAL_ERROR "private interno.h header leaked into the installed SDK")
endif()

message(STATUS "Sefirah public installation verified at ${SEFIRAH_PREFIXO}")
