#include "sefirah/compilador.h"

#include <stdio.h>

static int falhas = 0;

static int64_t dobrar_i64(int64_t valor) { return (int64_t)((uint64_t)valor * 2u); }

static void verificar(bool condicao, const char *mensagem) {
    if (!condicao) {
        fprintf(stderr, "FALHOU: %s\n", mensagem);
        falhas++;
    }
}

static bool emitir(SefFuncaoIr *funcao, uint32_t bloco, SefInstrucaoIr instrucao) {
    SefErro erro;
    if (sef_bloco_ir_emitir(funcao, bloco, instrucao, &erro))
        return true;
    fprintf(stderr, "emissao IR falhou: %s\n", erro.mensagem);
    return false;
}

static uint32_t ler_instrucao_aarch64(const SefCodigoNativo *codigo, size_t indice) {
    const unsigned char *p = codigo->bytes + indice * 4;
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8u) | ((uint32_t)p[2] << 16u) |
           ((uint32_t)p[3] << 24u);
}

static bool contem_instrucao_aarch64(const SefCodigoNativo *codigo, uint32_t procurada) {
    for (size_t i = 0; i < codigo->tamanho / 4; i++) {
        if (ler_instrucao_aarch64(codigo, i) == procurada)
            return true;
    }
    return false;
}

static bool objeto_elf_tem_maquina(const char *caminho, uint16_t maquina) {
    FILE *arquivo = fopen(caminho, "rb");
    if (arquivo == NULL)
        return false;
    unsigned char cabecalho[20];
    bool leu = fread(cabecalho, 1, sizeof(cabecalho), arquivo) == sizeof(cabecalho);
    fclose(arquivo);
    return leu && cabecalho[0] == 0x7f && cabecalho[1] == 'E' && cabecalho[2] == 'L' &&
           cabecalho[3] == 'F' && cabecalho[4] == 2 && cabecalho[5] == 1 &&
           ((uint16_t)cabecalho[18] | ((uint16_t)cabecalho[19] << 8u)) == maquina;
}

static bool objeto_coff_tem_maquina(const char *caminho, uint16_t maquina) {
    FILE *arquivo = fopen(caminho, "rb");
    if (arquivo == NULL)
        return false;
    unsigned char cabecalho[20];
    bool leu = fread(cabecalho, 1, sizeof(cabecalho), arquivo) == sizeof(cabecalho);
    fclose(arquivo);
    return leu && ((uint16_t)cabecalho[0] | ((uint16_t)cabecalho[1] << 8u)) == maquina &&
           cabecalho[2] == 1 && cabecalho[3] == 0;
}

static bool objeto_macho_tem_cpu(const char *caminho, uint32_t cpu) {
    FILE *arquivo = fopen(caminho, "rb");
    if (arquivo == NULL)
        return false;
    unsigned char cabecalho[32] = {0};
    bool leu = fread(cabecalho, 1, sizeof(cabecalho), arquivo) == sizeof(cabecalho);
    fclose(arquivo);
    uint32_t cpu_lido = (uint32_t)cabecalho[4] | ((uint32_t)cabecalho[5] << 8u) |
                        ((uint32_t)cabecalho[6] << 16u) | ((uint32_t)cabecalho[7] << 24u);
    return leu && cabecalho[0] == 0xcf && cabecalho[1] == 0xfa && cabecalho[2] == 0xed &&
           cabecalho[3] == 0xfe && cpu_lido == cpu && cabecalho[12] == 1;
}

int main(void) {
    SefErro erro;
    SefFuncaoIr fatorial;
    sef_funcao_ir_iniciar(&fatorial, "FATORIAL", 1, 7);
    uint32_t entrada, teste, corpo, fim;
    bool montou = sef_funcao_ir_adicionar_bloco(&fatorial, &entrada, &erro) &&
                  sef_funcao_ir_adicionar_bloco(&fatorial, &teste, &erro) &&
                  sef_funcao_ir_adicionar_bloco(&fatorial, &corpo, &erro) &&
                  sef_funcao_ir_adicionar_bloco(&fatorial, &fim, &erro);
    montou =
        montou &&
        emitir(&fatorial, entrada, (SefInstrucaoIr){SEF_IR_PARAMETRO, 0, 0, 0, 0, 0, 0}) &&
        emitir(&fatorial, entrada, (SefInstrucaoIr){SEF_IR_CONSTANTE_I64, 1, 0, 0, 1, 0, 0}) &&
        emitir(&fatorial, entrada, (SefInstrucaoIr){SEF_IR_SALTAR, 0, 0, 0, 0, teste, 0}) &&
        emitir(&fatorial, teste, (SefInstrucaoIr){SEF_IR_PHI, 2, 0, 4, 0, entrada, corpo}) &&
        emitir(&fatorial, teste, (SefInstrucaoIr){SEF_IR_PHI, 3, 1, 5, 0, entrada, corpo}) &&
        emitir(&fatorial, teste, (SefInstrucaoIr){SEF_IR_MENOR_OU_IGUAL_I64, 6, 2, 1, 0, 0, 0}) &&
        emitir(&fatorial, teste, (SefInstrucaoIr){SEF_IR_RAMIFICAR, 0, 6, 0, 0, fim, corpo}) &&
        emitir(&fatorial, corpo, (SefInstrucaoIr){SEF_IR_SUBTRAIR_I64, 4, 2, 1, 0, 0, 0}) &&
        emitir(&fatorial, corpo, (SefInstrucaoIr){SEF_IR_MULTIPLICAR_I64, 5, 3, 2, 0, 0, 0}) &&
        emitir(&fatorial, corpo, (SefInstrucaoIr){SEF_IR_SALTAR, 0, 0, 0, 0, teste, 0}) &&
        emitir(&fatorial, fim, (SefInstrucaoIr){SEF_IR_RETORNAR_I64, 0, 3, 0, 0, 0, 0});
    verificar(montou, "funcao IR foi montada");
    verificar(sef_funcao_ir_verificar(&fatorial, &erro), "IR SSA valida foi aceita");

    int64_t argumento = 6, resultado = 0;
    verificar(sef_funcao_ir_executar_i64(&fatorial, &argumento, 1, &resultado, &erro) &&
                  resultado == 720,
              "interpretador IR calculou fatorial de 6");
    argumento = 0;
    verificar(sef_funcao_ir_executar_i64(&fatorial, &argumento, 1, &resultado, &erro) &&
                  resultado == 1,
              "interpretador IR tratou caso base");

    SefCodigoNativo codigo;
    sef_codigo_nativo_iniciar(&codigo);
    verificar(sef_funcao_ir_emitir_x64(&fatorial, sef_abi_x64_hospedeiro(), &codigo, &erro),
              "backend emitiu codigo x86-64");
    verificar(codigo.tamanho > 0, "backend produziu bytes de maquina");
#if defined(__x86_64__) || defined(_M_X64)
    verificar(sef_codigo_nativo_preparar(&codigo, &erro),
              "codigo x86-64 recebeu memoria executavel W^X");
    argumento = 6;
    resultado = 0;
    verificar(sef_codigo_nativo_executar_i64(&codigo, &argumento, 1, &resultado, &erro) &&
                  resultado == 720,
              "codigo x86-64 calculou fatorial de 6");
    argumento = 0;
    verificar(sef_codigo_nativo_executar_i64(&codigo, &argumento, 1, &resultado, &erro) &&
                  resultado == 1,
              "codigo x86-64 tratou caso base");
#endif
    sef_codigo_nativo_liberar(&codigo);

    sef_codigo_nativo_iniciar(&codigo);
    verificar(sef_funcao_ir_emitir_x64(&fatorial, SEF_ABI_X64_SYSV, &codigo, &erro) &&
                  sef_codigo_nativo_gravar_elf(&codigo, "fatorial_x64", "teste-x64.o", &erro) &&
                  objeto_elf_tem_maquina("teste-x64.o", 62),
              "gravador produziu objeto ELF64 x86-64 System V");
    remove("teste-x64.o");
    verificar(sef_codigo_nativo_gravar_macho(&codigo, "fatorial_x64", "teste-x64-macho.o", &erro) &&
                  objeto_macho_tem_cpu("teste-x64-macho.o", 0x01000007u),
              "gravador produziu objeto Mach-O x86-64");
    remove("teste-x64-macho.o");
    sef_codigo_nativo_liberar(&codigo);

    sef_codigo_nativo_iniciar(&codigo);
    verificar(sef_funcao_ir_emitir_x64(&fatorial, SEF_ABI_X64_WINDOWS, &codigo, &erro) &&
                  sef_codigo_nativo_gravar_coff(&codigo, "fatorial_x64", "teste-x64.obj", &erro) &&
                  objeto_coff_tem_maquina("teste-x64.obj", 0x8664u),
              "gravador produziu objeto COFF AMD64 Microsoft");
    remove("teste-x64.obj");
    sef_codigo_nativo_liberar(&codigo);

    sef_codigo_nativo_iniciar(&codigo);
    verificar(sef_funcao_ir_emitir_aarch64(&fatorial, &codigo, &erro),
              "backend emitiu codigo AArch64");
    verificar(codigo.arquitetura == SEF_ARQUITETURA_AARCH64 && codigo.tamanho % 4 == 0,
              "codigo AArch64 possui arquitetura e alinhamento corretos");
    verificar(sef_codigo_nativo_gravar_elf(&codigo, "fatorial_aarch64", "teste-aarch64.o", &erro) &&
                  objeto_elf_tem_maquina("teste-aarch64.o", 183),
              "gravador produziu objeto ELF64 AArch64");
    remove("teste-aarch64.o");
    verificar(
        sef_codigo_nativo_gravar_coff(&codigo, "fatorial_aarch64", "teste-aarch64.obj", &erro) &&
            objeto_coff_tem_maquina("teste-aarch64.obj", 0xaa64u),
        "gravador produziu objeto COFF ARM64");
    remove("teste-aarch64.obj");
    verificar(sef_codigo_nativo_gravar_macho(&codigo, "fatorial_aarch64", "teste-aarch64-macho.o",
                                             &erro) &&
                  objeto_macho_tem_cpu("teste-aarch64-macho.o", 0x0100000cu),
              "gravador produziu objeto Mach-O ARM64");
    remove("teste-aarch64-macho.o");
    verificar(ler_instrucao_aarch64(&codigo, 0) == 0xa9bf7bfdu &&
                  ler_instrucao_aarch64(&codigo, 1) == 0x910003fdu &&
                  contem_instrucao_aarch64(&codigo, 0x9b017c00u) &&
                  contem_instrucao_aarch64(&codigo, 0x9a9fc7e0u) &&
                  contem_instrucao_aarch64(&codigo, 0xd65f03c0u),
              "AArch64 contem prologo, multiplicacao, comparacao e retorno");
#if defined(__aarch64__) || defined(_M_ARM64)
    verificar(sef_codigo_nativo_preparar(&codigo, &erro),
              "codigo AArch64 recebeu memoria executavel W^X");
    argumento = 6;
    verificar(sef_codigo_nativo_executar_i64(&codigo, &argumento, 1, &resultado, &erro) &&
                  resultado == 720,
              "codigo AArch64 calculou fatorial de 6");
#endif
    sef_codigo_nativo_liberar(&codigo);

    SefFuncaoIr escolher;
    sef_funcao_ir_iniciar(&escolher, "SOMAR-OU-SUBTRAIR", 2, 5);
    uint32_t decisao, somar, subtrair;
    bool montou_escolha =
        sef_funcao_ir_adicionar_bloco(&escolher, &decisao, &erro) &&
        sef_funcao_ir_adicionar_bloco(&escolher, &somar, &erro) &&
        sef_funcao_ir_adicionar_bloco(&escolher, &subtrair, &erro) &&
        emitir(&escolher, decisao, (SefInstrucaoIr){SEF_IR_PARAMETRO, 0, 0, 0, 0, 0, 0}) &&
        emitir(&escolher, decisao, (SefInstrucaoIr){SEF_IR_PARAMETRO, 1, 0, 0, 1, 0, 0}) &&
        emitir(&escolher, decisao, (SefInstrucaoIr){SEF_IR_MENOR_I64, 2, 0, 1, 0, 0, 0}) &&
        emitir(&escolher, decisao,
               (SefInstrucaoIr){SEF_IR_RAMIFICAR, 0, 2, 0, 0, somar, subtrair}) &&
        emitir(&escolher, somar, (SefInstrucaoIr){SEF_IR_SOMAR_I64, 3, 0, 1, 0, 0, 0}) &&
        emitir(&escolher, somar, (SefInstrucaoIr){SEF_IR_RETORNAR_I64, 0, 3, 0, 0, 0, 0}) &&
        emitir(&escolher, subtrair, (SefInstrucaoIr){SEF_IR_SUBTRAIR_I64, 4, 0, 1, 0, 0, 0}) &&
        emitir(&escolher, subtrair, (SefInstrucaoIr){SEF_IR_RETORNAR_I64, 0, 4, 0, 0, 0, 0});
    verificar(montou_escolha && sef_funcao_ir_verificar(&escolher, &erro),
              "IR com dois caminhos foi validada");
    sef_codigo_nativo_iniciar(&codigo);
    verificar(sef_funcao_ir_emitir_x64(&escolher, sef_abi_x64_hospedeiro(), &codigo, &erro),
              "backend emitiu ramificacao x86-64");
#if defined(__x86_64__) || defined(_M_X64)
    verificar(sef_codigo_nativo_preparar(&codigo, &erro),
              "ramificacao x86-64 recebeu memoria executavel");
    int64_t dois_argumentos[2] = {20, 22};
    verificar(sef_codigo_nativo_executar_i64(&codigo, dois_argumentos, 2, &resultado, &erro) &&
                  resultado == 42,
              "codigo nativo executou caminho de soma");
    dois_argumentos[0] = 50;
    dois_argumentos[1] = 8;
    verificar(sef_codigo_nativo_executar_i64(&codigo, dois_argumentos, 2, &resultado, &erro) &&
                  resultado == 42,
              "codigo nativo executou caminho de subtracao");
#endif
    sef_codigo_nativo_liberar(&codigo);
    sef_codigo_nativo_iniciar(&codigo);
    verificar(sef_funcao_ir_emitir_aarch64(&escolher, &codigo, &erro) &&
                  contem_instrucao_aarch64(&codigo, 0x8b010000u) &&
                  contem_instrucao_aarch64(&codigo, 0xcb010000u) &&
                  contem_instrucao_aarch64(&codigo, 0x9a9fa7e0u),
              "AArch64 emitiu soma, subtracao e comparacao estrita");
#if defined(__aarch64__) || defined(_M_ARM64)
    verificar(sef_codigo_nativo_preparar(&codigo, &erro),
              "ramificacao AArch64 recebeu memoria executavel");
    int64_t argumentos_aarch64[2] = {20, 22};
    verificar(sef_codigo_nativo_executar_i64(&codigo, argumentos_aarch64, 2, &resultado, &erro) &&
                  resultado == 42,
              "codigo AArch64 executou caminho de soma");
    argumentos_aarch64[0] = 50;
    argumentos_aarch64[1] = 8;
    verificar(sef_codigo_nativo_executar_i64(&codigo, argumentos_aarch64, 2, &resultado, &erro) &&
                  resultado == 42,
              "codigo AArch64 executou caminho de subtracao");
#endif
    sef_codigo_nativo_liberar(&codigo);
    sef_funcao_ir_liberar(&escolher);

    SefFuncaoIr chamada_externa;
    sef_funcao_ir_iniciar(&chamada_externa, "CHAMAR-DOBRO", 1, 2);
    uint32_t bloco_externo, simbolo_externo;
    bool montou_externa =
        sef_funcao_ir_adicionar_bloco(&chamada_externa, &bloco_externo, &erro) &&
        sef_funcao_ir_adicionar_externa_i64(&chamada_externa, "dobrar_i64", dobrar_i64,
                                            &simbolo_externo, &erro) &&
        emitir(&chamada_externa, bloco_externo,
               (SefInstrucaoIr){SEF_IR_PARAMETRO, 0, 0, 0, 0, 0, 0}) &&
        emitir(&chamada_externa, bloco_externo,
               (SefInstrucaoIr){SEF_IR_CHAMAR_EXTERNA_I64, 1, 0, 0, simbolo_externo, 0, 0}) &&
        emitir(&chamada_externa, bloco_externo,
               (SefInstrucaoIr){SEF_IR_RETORNAR_I64, 0, 1, 0, 0, 0, 0});
    argumento = 21;
    verificar(montou_externa && sef_funcao_ir_verificar(&chamada_externa, &erro) &&
                  sef_funcao_ir_executar_i64(&chamada_externa, &argumento, 1, &resultado, &erro) &&
                  resultado == 42,
              "IR chamou funcao C externa pelo interpretador");

    sef_codigo_nativo_iniciar(&codigo);
    verificar(
        sef_funcao_ir_emitir_x64(&chamada_externa, SEF_ABI_X64_SYSV, &codigo, &erro) &&
            codigo.quantidade_relocacoes == 1 &&
            codigo.relocacoes[0].tipo == SEF_RELOCACAO_CHAMADA_REL32_X64 &&
            codigo.bytes[codigo.relocacoes[0].deslocamento - 1u] == 0xe8 &&
            sef_codigo_nativo_gravar_elf(&codigo, "chamar_dobro", "teste-externa.o", &erro) &&
            sef_codigo_nativo_gravar_macho(&codigo, "chamar_dobro", "teste-externa-macho.o", &erro),
        "x86-64 e objetos Unix preservaram chamada externa");
    verificar(!sef_codigo_nativo_preparar(&codigo, &erro) && erro.ocorreu,
              "JIT recusou simbolo externo ainda nao resolvido");
    remove("teste-externa.o");
    remove("teste-externa-macho.o");
    sef_codigo_nativo_liberar(&codigo);

    sef_codigo_nativo_iniciar(&codigo);
    verificar(
        sef_funcao_ir_emitir_x64(&chamada_externa, SEF_ABI_X64_WINDOWS, &codigo, &erro) &&
            codigo.quantidade_relocacoes == 1 &&
            sef_codigo_nativo_gravar_coff(&codigo, "chamar_dobro", "teste-externa.obj", &erro),
        "COFF AMD64 preservou chamada externa Microsoft");
    remove("teste-externa.obj");
    sef_codigo_nativo_liberar(&codigo);

    sef_codigo_nativo_iniciar(&codigo);
    verificar(
        sef_funcao_ir_emitir_aarch64(&chamada_externa, &codigo, &erro) &&
            codigo.quantidade_relocacoes == 1 &&
            codigo.relocacoes[0].tipo == SEF_RELOCACAO_CHAMADA26_AARCH64 &&
            ler_instrucao_aarch64(&codigo, codigo.relocacoes[0].deslocamento / 4u) == 0x94000000u &&
            sef_codigo_nativo_gravar_elf(&codigo, "chamar_dobro", "teste-externa-arm.o", &erro) &&
            sef_codigo_nativo_gravar_coff(&codigo, "chamar_dobro", "teste-externa-arm.obj",
                                          &erro) &&
            sef_codigo_nativo_gravar_macho(&codigo, "chamar_dobro", "teste-externa-arm-macho.o",
                                           &erro),
        "AArch64 e objetos desktop preservaram chamada externa");
    remove("teste-externa-arm.o");
    remove("teste-externa-arm.obj");
    remove("teste-externa-arm-macho.o");
    sef_codigo_nativo_liberar(&codigo);
    sef_funcao_ir_liberar(&chamada_externa);

    fatorial.blocos[corpo].instrucoes[0].operando_a = 5;
    verificar(!sef_funcao_ir_verificar(&fatorial, &erro) && erro.ocorreu,
              "verificador rejeitou uso anterior a definicao SSA");
    fatorial.blocos[corpo].instrucoes[0].operando_a = 2;
    fatorial.blocos[entrada].instrucoes[1].destino = 0;
    verificar(!sef_funcao_ir_verificar(&fatorial, &erro) && erro.ocorreu,
              "verificador rejeitou duas definicoes do mesmo registrador SSA");
    sef_funcao_ir_liberar(&fatorial);

    if (falhas == 0)
        puts("compilador: todos os testes passaram");
    return falhas == 0 ? 0 : 1;
}
