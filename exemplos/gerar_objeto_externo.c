#include "sefirah/compilador.h"

#include <stdio.h>
#include <string.h>

static bool emitir(SefFuncaoIr *funcao, uint32_t bloco, SefInstrucaoIr instrucao, SefErro *erro) {
    return sef_bloco_ir_emitir(funcao, bloco, instrucao, erro);
}

static int falhar(const SefErro *erro) {
    fprintf(stderr, "Erro: %s\n", erro->mensagem);
    return 1;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fputs("uso: exemplo_gerar_objeto_externo <elf|coff|macho> <x64|arm64> <saida>\n", stderr);
        return 2;
    }

    SefErro erro;
    SefFuncaoIr funcao;
    sef_funcao_ir_iniciar(&funcao, "CHAMAR-DOBRO", 1, 2);
    uint32_t bloco, externa;
    bool sucesso =
        sef_funcao_ir_adicionar_bloco(&funcao, &bloco, &erro) &&
        sef_funcao_ir_adicionar_externa_i64(&funcao, "dobrar_i64", NULL, &externa, &erro) &&
        emitir(&funcao, bloco, (SefInstrucaoIr){SEF_IR_PARAMETRO, 0, 0, 0, 0, 0, 0}, &erro) &&
        emitir(&funcao, bloco, (SefInstrucaoIr){SEF_IR_CHAMAR_EXTERNA_I64, 1, 0, 0, externa, 0, 0},
               &erro) &&
        emitir(&funcao, bloco, (SefInstrucaoIr){SEF_IR_RETORNAR_I64, 0, 1, 0, 0, 0, 0}, &erro);

    SefCodigoNativo codigo;
    sef_codigo_nativo_iniciar(&codigo);
    if (sucesso && strcmp(argv[2], "x64") == 0) {
        SefAbiX64 abi = strcmp(argv[1], "coff") == 0 ? SEF_ABI_X64_WINDOWS : SEF_ABI_X64_SYSV;
        sucesso = sef_funcao_ir_emitir_x64(&funcao, abi, &codigo, &erro);
    } else if (sucesso && strcmp(argv[2], "arm64") == 0) {
        sucesso = sef_funcao_ir_emitir_aarch64(&funcao, &codigo, &erro);
    } else if (sucesso) {
        fputs("arquitetura deve ser x64 ou arm64\n", stderr);
        sucesso = false;
        erro.ocorreu = false;
    }

    if (sucesso && strcmp(argv[1], "elf") == 0)
        sucesso = sef_codigo_nativo_gravar_elf(&codigo, "chamar_dobro", argv[3], &erro);
    else if (sucesso && strcmp(argv[1], "coff") == 0)
        sucesso = sef_codigo_nativo_gravar_coff(&codigo, "chamar_dobro", argv[3], &erro);
    else if (sucesso && strcmp(argv[1], "macho") == 0)
        sucesso = sef_codigo_nativo_gravar_macho(&codigo, "chamar_dobro", argv[3], &erro);
    else if (sucesso) {
        fputs("formato deve ser elf, coff ou macho\n", stderr);
        sucesso = false;
        erro.ocorreu = false;
    }

    sef_codigo_nativo_liberar(&codigo);
    sef_funcao_ir_liberar(&funcao);
    if (!sucesso)
        return erro.ocorreu ? falhar(&erro) : 2;
    printf("Objeto %s/%s salvo em %s\n", argv[1], argv[2], argv[3]);
    return 0;
}
