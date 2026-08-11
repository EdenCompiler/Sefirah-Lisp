#include "sefirah/interno.h"

#include "sefirah/compilador.h"

#include <stdlib.h>
#include <string.h>

struct SefFuncaoCompilada {
    char *nome;
    SefFuncaoIr ir;
    SefCodigoNativo codigo;
    bool possui_codigo_nativo;
    SefRecursoBiblioteca *biblioteca_externa;
};

typedef struct ContextoCompilacao {
    SefRuntime *runtime;
    SefFuncaoIr *ir;
    SefValor *parametros;
    uint32_t quantidade_parametros;
    uint32_t proximo_registrador;
    uint32_t bloco_atual;
    SefErro *erro;
} ContextoCompilacao;

static SefValor primeiro(SefValor lista) { return lista->como.par.primeiro; }
static SefValor resto(SefValor lista) { return lista->como.par.resto; }

static bool quantidade_exata(SefRuntime *runtime, SefValor lista, size_t quantidade) {
    bool propria = false;
    return sef_lista_tamanho(runtime, lista, &propria) == quantidade && propria;
}

static bool novo_registrador(ContextoCompilacao *contexto, uint32_t *registrador) {
    if (contexto->proximo_registrador == UINT32_MAX) {
        sef_erro_definir(contexto->erro, 0, 0, "funcao excedeu o limite de registradores IR");
        return false;
    }
    *registrador = contexto->proximo_registrador++;
    return true;
}

static bool emitir(ContextoCompilacao *contexto, uint32_t bloco, SefInstrucaoIr instrucao) {
    return sef_bloco_ir_emitir(contexto->ir, bloco, instrucao, contexto->erro);
}

static bool compilar_expressao(ContextoCompilacao *contexto, SefValor forma, uint32_t *resultado);

static bool compilar_if(ContextoCompilacao *contexto, SefValor argumentos, uint32_t *resultado) {
    if (!quantidade_exata(contexto->runtime, argumentos, 3)) {
        sef_erro_definir(contexto->erro, 0, 0,
                         "compilador i64 exige IF com consequente e alternativa");
        return false;
    }
    SefValor teste = primeiro(argumentos);
    SefValor consequente = primeiro(resto(argumentos));
    SefValor alternativa = primeiro(resto(resto(argumentos)));
    uint32_t condicao;
    if (!compilar_expressao(contexto, teste, &condicao))
        return false;

    uint32_t bloco_origem = contexto->bloco_atual;
    uint32_t bloco_verdadeiro, bloco_falso, bloco_uniao;
    if (!sef_funcao_ir_adicionar_bloco(contexto->ir, &bloco_verdadeiro, contexto->erro) ||
        !sef_funcao_ir_adicionar_bloco(contexto->ir, &bloco_falso, contexto->erro) ||
        !sef_funcao_ir_adicionar_bloco(contexto->ir, &bloco_uniao, contexto->erro) ||
        !emitir(
            contexto, bloco_origem,
            (SefInstrucaoIr){SEF_IR_RAMIFICAR, 0, condicao, 0, 0, bloco_verdadeiro, bloco_falso}))
        return false;

    contexto->bloco_atual = bloco_verdadeiro;
    uint32_t valor_verdadeiro;
    if (!compilar_expressao(contexto, consequente, &valor_verdadeiro))
        return false;
    uint32_t predecessor_verdadeiro = contexto->bloco_atual;
    if (!emitir(contexto, predecessor_verdadeiro,
                (SefInstrucaoIr){SEF_IR_SALTAR, 0, 0, 0, 0, bloco_uniao, 0}))
        return false;

    contexto->bloco_atual = bloco_falso;
    uint32_t valor_falso;
    if (!compilar_expressao(contexto, alternativa, &valor_falso))
        return false;
    uint32_t predecessor_falso = contexto->bloco_atual;
    if (!emitir(contexto, predecessor_falso,
                (SefInstrucaoIr){SEF_IR_SALTAR, 0, 0, 0, 0, bloco_uniao, 0}) ||
        !novo_registrador(contexto, resultado))
        return false;

    contexto->bloco_atual = bloco_uniao;
    return emitir(contexto, bloco_uniao,
                  (SefInstrucaoIr){SEF_IR_PHI, *resultado, valor_verdadeiro, valor_falso, 0,
                                   predecessor_verdadeiro, predecessor_falso});
}

static bool compilar_operacao_binaria(ContextoCompilacao *contexto, SefValor operador,
                                      SefValor argumentos, uint32_t *resultado) {
    if (!quantidade_exata(contexto->runtime, argumentos, 2)) {
        sef_erro_definir(contexto->erro, 0, 0,
                         "compilador i64 aceita exatamente dois operandos por operacao");
        return false;
    }
    SefOperacaoIr operacao;
    if (sef_simbolo_tem_nome(operador, "+"))
        operacao = SEF_IR_SOMAR_I64;
    else if (sef_simbolo_tem_nome(operador, "-"))
        operacao = SEF_IR_SUBTRAIR_I64;
    else if (sef_simbolo_tem_nome(operador, "*"))
        operacao = SEF_IR_MULTIPLICAR_I64;
    else if (sef_simbolo_tem_nome(operador, "<"))
        operacao = SEF_IR_MENOR_I64;
    else if (sef_simbolo_tem_nome(operador, "<="))
        operacao = SEF_IR_MENOR_OU_IGUAL_I64;
    else {
        sef_erro_definir(contexto->erro, 0, 0, "operacao ainda nao suportada pelo compilador i64");
        return false;
    }
    uint32_t a, b;
    if (!compilar_expressao(contexto, primeiro(argumentos), &a) ||
        !compilar_expressao(contexto, primeiro(resto(argumentos)), &b) ||
        !novo_registrador(contexto, resultado))
        return false;
    return emitir(contexto, contexto->bloco_atual,
                  (SefInstrucaoIr){operacao, *resultado, a, b, 0, 0, 0});
}

static bool compilar_chamada_externa(ContextoCompilacao *contexto, SefValor argumentos,
                                     uint32_t *resultado) {
    bool propria = false;
    size_t quantidade = sef_lista_tamanho(contexto->runtime, argumentos, &propria);
    if (!propria || quantidade < 2 || quantidade > 3) {
        sef_erro_definir(contexto->erro, 0, 0, "EXTERNAL-I64 exige nome C e um ou dois argumentos");
        return false;
    }
    SefValor nome = primeiro(argumentos);
    if (nome->tipo != SEF_TIPO_TEXTO || nome->como.texto.tamanho == 0 ||
        memchr(nome->como.texto.dados, '\0', nome->como.texto.tamanho) != NULL) {
        sef_erro_definir(contexto->erro, 0, 0, "nome de EXTERNAL-I64 deve ser uma string C valida");
        return false;
    }
    uint32_t argumento_a, argumento_b = 0, externa;
    if (!compilar_expressao(contexto, primeiro(resto(argumentos)), &argumento_a) ||
        (quantidade == 3 &&
         !compilar_expressao(contexto, primeiro(resto(resto(argumentos))), &argumento_b)) ||
        !sef_funcao_ir_adicionar_externa_i64(contexto->ir, nome->como.texto.dados, NULL, &externa,
                                             contexto->erro) ||
        !novo_registrador(contexto, resultado))
        return false;
    return emitir(contexto, contexto->bloco_atual,
                  (SefInstrucaoIr){SEF_IR_CHAMAR_EXTERNA_I64, *resultado, argumento_a, argumento_b,
                                   externa, (uint32_t)(quantidade - 1), 0});
}

static bool compilar_expressao(ContextoCompilacao *contexto, SefValor forma, uint32_t *resultado) {
    if (forma->tipo == SEF_TIPO_INTEIRO) {
        return novo_registrador(contexto, resultado) &&
               emitir(contexto, contexto->bloco_atual,
                      (SefInstrucaoIr){SEF_IR_CONSTANTE_I64, *resultado, 0, 0, forma->como.inteiro,
                                       0, 0});
    }
    if (forma->tipo == SEF_TIPO_SIMBOLO) {
        for (uint32_t i = 0; i < contexto->quantidade_parametros; i++) {
            if (contexto->parametros[i] == forma) {
                *resultado = i;
                return true;
            }
        }
        sef_erro_definir(contexto->erro, 0, 0,
                         "compilador i64 encontrou variavel livre ou desconhecida");
        return false;
    }
    if (forma->tipo != SEF_TIPO_PAR) {
        sef_erro_definir(contexto->erro, 0, 0, "compilador i64 aceita apenas inteiros e formas");
        return false;
    }
    SefValor operador = primeiro(forma);
    if (operador->tipo != SEF_TIPO_SIMBOLO) {
        sef_erro_definir(contexto->erro, 0, 0, "operador compilado deve ser um simbolo");
        return false;
    }
    if (sef_simbolo_tem_nome(operador, "IF"))
        return compilar_if(contexto, resto(forma), resultado);
    if (sef_simbolo_tem_nome(operador, "EXTERNAL-I64"))
        return compilar_chamada_externa(contexto, resto(forma), resultado);
    return compilar_operacao_binaria(contexto, operador, resto(forma), resultado);
}

static bool copiar_parametros(SefRuntime *runtime, SefValor lista, SefValor **parametros,
                              uint32_t *quantidade, SefErro *erro) {
    bool propria = false;
    size_t total = sef_lista_tamanho(runtime, lista, &propria);
    if (!propria || total > UINT32_MAX) {
        sef_erro_definir(erro, 0, 0, "lista de parametros nao pode ser compilada para i64");
        return false;
    }
    SefValor *itens = total == 0 ? NULL : malloc(total * sizeof(*itens));
    if (total > 0 && itens == NULL) {
        sef_erro_definir(erro, 0, 0, "memoria insuficiente para parametros compilados");
        return false;
    }
    for (size_t i = 0; i < total; i++) {
        itens[i] = primeiro(lista);
        if (itens[i]->tipo != SEF_TIPO_SIMBOLO || sef_simbolo_tem_nome(itens[i], "&REST")) {
            free(itens);
            sef_erro_definir(erro, 0, 0, "compilador i64 exige parametros posicionais simples");
            return false;
        }
        for (size_t anterior = 0; anterior < i; anterior++) {
            if (itens[anterior] == itens[i]) {
                free(itens);
                sef_erro_definir(erro, 0, 0, "parametro duplicado na funcao compilada");
                return false;
            }
        }
        lista = resto(lista);
    }
    *parametros = itens;
    *quantidade = (uint32_t)total;
    return true;
}

static SefFuncaoCompilada *compilar_funcao_i64(SefRuntime *runtime, const char *nome,
                                               bool preparar_jit, SefErro *erro) {
    sef_erro_limpar(erro);
    if (runtime == NULL || nome == NULL || nome[0] == '\0') {
        sef_erro_definir(erro, 0, 0, "runtime ou nome ausente para compilacao");
        return NULL;
    }
    SefValor simbolo = sef_simbolo_internar(runtime, nome, strlen(nome), erro);
    SefValor valor;
    if (simbolo == NULL || !sef_ambiente_obter_funcao(runtime->ambiente_global, simbolo, &valor) ||
        valor->tipo != SEF_TIPO_FUNCAO || valor->como.funcao.macro) {
        sef_erro_definir(erro, 0, 0, "nome nao designa uma funcao Lisp compilavel");
        return NULL;
    }
    if (!quantidade_exata(runtime, valor->como.funcao.corpo, 1)) {
        sef_erro_definir(erro, 0, 0, "compilador i64 exige uma unica forma no corpo");
        return NULL;
    }
    SefValor *parametros = NULL;
    uint32_t quantidade_parametros = 0;
    if (!copiar_parametros(runtime, valor->como.funcao.parametros, &parametros,
                           &quantidade_parametros, erro))
        return NULL;

    SefFuncaoCompilada *compilada = calloc(1, sizeof(*compilada));
    if (compilada == NULL) {
        free(parametros);
        sef_erro_definir(erro, 0, 0, "memoria insuficiente para funcao compilada");
        return NULL;
    }
    compilada->nome = malloc(strlen(nome) + 1);
    if (compilada->nome == NULL) {
        free(parametros);
        free(compilada);
        sef_erro_definir(erro, 0, 0, "memoria insuficiente para nome compilado");
        return NULL;
    }
    strcpy(compilada->nome, nome);
    sef_funcao_ir_iniciar(&compilada->ir, compilada->nome, quantidade_parametros, 0);
    sef_codigo_nativo_iniciar(&compilada->codigo);
    uint32_t entrada;
    bool sucesso = sef_funcao_ir_adicionar_bloco(&compilada->ir, &entrada, erro);
    ContextoCompilacao contexto = {
        runtime, &compilada->ir, parametros, quantidade_parametros, quantidade_parametros, entrada,
        erro};
    for (uint32_t i = 0; sucesso && i < quantidade_parametros; i++)
        sucesso = emitir(&contexto, entrada, (SefInstrucaoIr){SEF_IR_PARAMETRO, i, 0, 0, i, 0, 0});
    uint32_t resultado;
    if (sucesso)
        sucesso = compilar_expressao(&contexto, primeiro(valor->como.funcao.corpo), &resultado) &&
                  emitir(&contexto, contexto.bloco_atual,
                         (SefInstrucaoIr){SEF_IR_RETORNAR_I64, 0, resultado, 0, 0, 0, 0});
    compilada->ir.quantidade_registradores = contexto.proximo_registrador;
    if (sucesso)
        sucesso = sef_funcao_ir_verificar(&compilada->ir, erro);
#if defined(__x86_64__) || defined(_M_X64)
    if (sucesso)
        sucesso = sef_funcao_ir_emitir_x64(&compilada->ir, sef_abi_x64_hospedeiro(),
                                           &compilada->codigo, erro);
#elif defined(__aarch64__) || defined(_M_ARM64)
    if (sucesso)
        sucesso = sef_funcao_ir_emitir_aarch64(&compilada->ir, &compilada->codigo, erro);
#endif
    if (sucesso && preparar_jit)
        sucesso = sef_codigo_nativo_preparar(&compilada->codigo, erro);
    compilada->possui_codigo_nativo = sucesso && compilada->codigo.memoria_executavel != NULL;
    free(parametros);
    if (!sucesso) {
        sef_funcao_compilada_liberar(compilada);
        return NULL;
    }
    return compilada;
}

SefFuncaoCompilada *sef_runtime_compilar_funcao_i64(SefRuntime *runtime, const char *nome,
                                                    SefErro *erro) {
    return compilar_funcao_i64(runtime, nome, true, erro);
}

SefFuncaoCompilada *sef_runtime_compilar_objeto_i64(SefRuntime *runtime, const char *nome,
                                                    SefErro *erro) {
    return compilar_funcao_i64(runtime, nome, false, erro);
}

bool sef_funcao_compilada_vincular_externa_i64(SefFuncaoCompilada *funcao, const char *simbolo,
                                               SefFuncaoExternaI64 endereco, SefErro *erro) {
    if (funcao == NULL) {
        sef_erro_limpar(erro);
        sef_erro_definir(erro, 0, 0, "funcao compilada ausente para vinculacao externa");
        return false;
    }
    if (!sef_codigo_nativo_vincular_externa_i64(&funcao->codigo, simbolo, endereco, erro))
        return false;
    for (size_t i = 0; i < funcao->ir.quantidade_externas; i++) {
        if (strcmp(funcao->ir.externas[i].nome, simbolo) == 0)
            funcao->ir.externas[i].endereco = endereco;
    }
    return true;
}

bool sef_funcao_compilada_vincular_externa_i64_binaria(SefFuncaoCompilada *funcao,
                                                       const char *simbolo,
                                                       SefFuncaoExternaI64Binaria endereco,
                                                       SefErro *erro) {
    if (funcao == NULL) {
        sef_erro_limpar(erro);
        sef_erro_definir(erro, 0, 0, "funcao compilada ausente para vinculacao externa");
        return false;
    }
    if (!sef_codigo_nativo_vincular_externa_i64_binaria(&funcao->codigo, simbolo, endereco, erro))
        return false;
    SefFuncaoExternaI64 endereco_armazenado = NULL;
    _Static_assert(sizeof(endereco) == sizeof(endereco_armazenado),
                   "ponteiros de funcoes i64 devem ter o mesmo tamanho");
    memcpy(&endereco_armazenado, &endereco, sizeof(endereco_armazenado));
    for (size_t i = 0; i < funcao->ir.quantidade_externas; i++) {
        if (strcmp(funcao->ir.externas[i].nome, simbolo) == 0)
            funcao->ir.externas[i].endereco = endereco_armazenado;
    }
    return true;
}

static bool vincular_recurso_biblioteca_i64(SefFuncaoCompilada *funcao,
                                            SefRecursoBiblioteca *recurso, SefErro *erro) {
    sef_erro_limpar(erro);
    if (funcao == NULL || recurso == NULL || funcao->biblioteca_externa != NULL ||
        funcao->codigo.memoria_executavel != NULL) {
        sef_erro_definir(erro, 0, 0, "funcao ou recurso invalido para biblioteca compartilhada");
        sef_biblioteca_recurso_liberar(recurso);
        return false;
    }
    if (funcao->codigo.quantidade_relocacoes == 0) {
        sef_erro_definir(erro, 0, 0, "funcao compilada nao possui simbolos externos");
        sef_biblioteca_recurso_liberar(recurso);
        return false;
    }
    bool sucesso = true;
    for (size_t i = 0; sucesso && i < funcao->codigo.quantidade_relocacoes; i++) {
        const char *simbolo = funcao->codigo.relocacoes[i].simbolo;
        SefFuncaoExternaI64 endereco = sef_biblioteca_recurso_resolver(recurso, simbolo, erro);
        sucesso = endereco != NULL &&
                  sef_funcao_compilada_vincular_externa_i64(funcao, simbolo, endereco, erro);
    }
    if (!sucesso) {
        sef_biblioteca_recurso_liberar(recurso);
        return false;
    }
    funcao->biblioteca_externa = recurso;
    return true;
}

bool sef_funcao_compilada_vincular_biblioteca_i64(SefFuncaoCompilada *funcao, const char *caminho,
                                                  SefErro *erro) {
    SefRecursoBiblioteca *recurso = sef_biblioteca_recurso_abrir(caminho, erro);
    if (recurso == NULL)
        return false;
    return vincular_recurso_biblioteca_i64(funcao, recurso, erro);
}

static bool vincular_objeto_biblioteca_i64(SefFuncaoCompilada *funcao, SefValor biblioteca,
                                           SefErro *erro) {
    sef_erro_limpar(erro);
    if (biblioteca == NULL || biblioteca->tipo != SEF_TIPO_BIBLIOTECA ||
        biblioteca->como.biblioteca.fechada || biblioteca->como.biblioteca.recurso == NULL) {
        sef_erro_definir(erro, 0, 0, "biblioteca compartilhada ausente ou fechada");
        return false;
    }
    sef_biblioteca_recurso_reter(biblioteca->como.biblioteca.recurso);
    return vincular_recurso_biblioteca_i64(funcao, biblioteca->como.biblioteca.recurso, erro);
}

bool sef_funcao_compilada_preparar_jit(SefFuncaoCompilada *funcao, SefErro *erro) {
    if (funcao == NULL || funcao->possui_codigo_nativo) {
        sef_erro_limpar(erro);
        sef_erro_definir(erro, 0, 0, "funcao compilada ausente ou JIT ja preparado");
        return false;
    }
    if (!sef_codigo_nativo_preparar(&funcao->codigo, erro))
        return false;
    funcao->possui_codigo_nativo = true;
    return true;
}

bool sef_funcao_compilada_executar_i64(const SefFuncaoCompilada *funcao, const int64_t *argumentos,
                                       size_t quantidade_argumentos, int64_t *resultado,
                                       SefErro *erro) {
    if (funcao == NULL || resultado == NULL) {
        sef_erro_limpar(erro);
        sef_erro_definir(erro, 0, 0, "funcao compilada ou resultado ausente");
        return false;
    }
    int64_t resultado_i64 = 0;
    bool sucesso = funcao->possui_codigo_nativo
                       ? sef_codigo_nativo_executar_i64(&funcao->codigo, argumentos,
                                                        quantidade_argumentos, &resultado_i64, erro)
                       : sef_funcao_ir_executar_i64(&funcao->ir, argumentos, quantidade_argumentos,
                                                    &resultado_i64, erro);
    if (sucesso)
        *resultado = resultado_i64;
    return sucesso;
}

bool sef_funcao_compilada_gravar_elf(const SefFuncaoCompilada *funcao, const char *nome_simbolo,
                                     const char *caminho, SefErro *erro) {
    if (funcao == NULL) {
        sef_erro_limpar(erro);
        sef_erro_definir(erro, 0, 0, "funcao compilada ausente para gravar ELF");
        return false;
    }
    if (funcao->codigo.arquitetura == SEF_ARQUITETURA_X64 &&
        funcao->codigo.abi_x64 == SEF_ABI_X64_WINDOWS) {
        SefCodigoNativo codigo_sysv;
        sef_codigo_nativo_iniciar(&codigo_sysv);
        bool sucesso =
            sef_funcao_ir_emitir_x64(&funcao->ir, SEF_ABI_X64_SYSV, &codigo_sysv, erro) &&
            sef_codigo_nativo_gravar_elf(&codigo_sysv, nome_simbolo, caminho, erro);
        sef_codigo_nativo_liberar(&codigo_sysv);
        return sucesso;
    }
    return sef_codigo_nativo_gravar_elf(&funcao->codigo, nome_simbolo, caminho, erro);
}

bool sef_funcao_compilada_gravar_coff(const SefFuncaoCompilada *funcao, const char *nome_simbolo,
                                      const char *caminho, SefErro *erro) {
    if (funcao == NULL) {
        sef_erro_limpar(erro);
        sef_erro_definir(erro, 0, 0, "funcao compilada ausente para gravar COFF");
        return false;
    }
    if (funcao->codigo.arquitetura == SEF_ARQUITETURA_X64 &&
        funcao->codigo.abi_x64 != SEF_ABI_X64_WINDOWS) {
        SefCodigoNativo codigo_windows;
        sef_codigo_nativo_iniciar(&codigo_windows);
        bool sucesso =
            sef_funcao_ir_emitir_x64(&funcao->ir, SEF_ABI_X64_WINDOWS, &codigo_windows, erro) &&
            sef_codigo_nativo_gravar_coff(&codigo_windows, nome_simbolo, caminho, erro);
        sef_codigo_nativo_liberar(&codigo_windows);
        return sucesso;
    }
    return sef_codigo_nativo_gravar_coff(&funcao->codigo, nome_simbolo, caminho, erro);
}

bool sef_funcao_compilada_gravar_macho(const SefFuncaoCompilada *funcao, const char *nome_simbolo,
                                       const char *caminho, SefErro *erro) {
    if (funcao == NULL) {
        sef_erro_limpar(erro);
        sef_erro_definir(erro, 0, 0, "funcao compilada ausente para gravar Mach-O");
        return false;
    }
    if (funcao->codigo.arquitetura == SEF_ARQUITETURA_X64 &&
        funcao->codigo.abi_x64 == SEF_ABI_X64_WINDOWS) {
        SefCodigoNativo codigo_sysv;
        sef_codigo_nativo_iniciar(&codigo_sysv);
        bool sucesso =
            sef_funcao_ir_emitir_x64(&funcao->ir, SEF_ABI_X64_SYSV, &codigo_sysv, erro) &&
            sef_codigo_nativo_gravar_macho(&codigo_sysv, nome_simbolo, caminho, erro);
        sef_codigo_nativo_liberar(&codigo_sysv);
        return sucesso;
    }
    return sef_codigo_nativo_gravar_macho(&funcao->codigo, nome_simbolo, caminho, erro);
}

void sef_funcao_compilada_liberar(SefFuncaoCompilada *funcao) {
    if (funcao == NULL)
        return;
    sef_codigo_nativo_liberar(&funcao->codigo);
    sef_biblioteca_recurso_liberar(funcao->biblioteca_externa);
    sef_funcao_ir_liberar(&funcao->ir);
    free(funcao->nome);
    free(funcao);
}

static SefFuncaoCompilada *compilar_simbolo_para_instalacao(SefRuntime *runtime, SefValor simbolo,
                                                            bool objeto, const char *operacao,
                                                            SefValor *funcao, SefErro *erro) {
    if (runtime == NULL || simbolo == NULL || simbolo->tipo != SEF_TIPO_SIMBOLO) {
        sef_erro_limpar(erro);
        sef_erro_definir(erro, 0, 0, "%s exige um simbolo de funcao", operacao);
        return NULL;
    }
    if (!sef_ambiente_obter_funcao(runtime->ambiente_global, simbolo, funcao) ||
        (*funcao)->tipo != SEF_TIPO_FUNCAO || (*funcao)->como.funcao.macro) {
        sef_erro_limpar(erro);
        sef_erro_definir(erro, 0, 0, "simbolo nao nomeia funcao Lisp compilavel");
        return NULL;
    }
    char *nome = sef_valor_para_texto(runtime, simbolo, true, erro);
    if (nome == NULL)
        return NULL;
    SefFuncaoCompilada *compilada = objeto ? sef_runtime_compilar_objeto_i64(runtime, nome, erro)
                                           : sef_runtime_compilar_funcao_i64(runtime, nome, erro);
    sef_texto_liberar(nome);
    return compilada;
}

static void instalar_compilada(SefValor funcao, SefFuncaoCompilada *compilada) {
    sef_funcao_compilada_liberar(funcao->como.funcao.compilada_i64);
    funcao->como.funcao.compilada_i64 = compilada;
}

bool sef_funcao_compilada_instalar_i64(SefRuntime *runtime, SefValor simbolo, SefErro *erro) {
    SefValor funcao = NULL;
    SefFuncaoCompilada *compilada =
        compilar_simbolo_para_instalacao(runtime, simbolo, false, "COMPILE", &funcao, erro);
    if (compilada == NULL)
        return false;
    instalar_compilada(funcao, compilada);
    return true;
}

bool sef_funcao_compilada_instalar_biblioteca_i64(SefRuntime *runtime, SefValor simbolo,
                                                  const char *caminho, SefErro *erro) {
    SefValor funcao = NULL;
    SefFuncaoCompilada *compilada = compilar_simbolo_para_instalacao(
        runtime, simbolo, true, "COMPILE-EXTERNAL-I64", &funcao, erro);
    if (compilada == NULL)
        return false;
    bool sucesso = sef_funcao_compilada_vincular_biblioteca_i64(compilada, caminho, erro) &&
                   sef_funcao_compilada_preparar_jit(compilada, erro);
    if (!sucesso) {
        sef_funcao_compilada_liberar(compilada);
        return false;
    }
    instalar_compilada(funcao, compilada);
    return true;
}

bool sef_funcao_compilada_instalar_objeto_biblioteca_i64(SefRuntime *runtime, SefValor simbolo,
                                                         SefValor biblioteca, SefErro *erro) {
    SefValor funcao = NULL;
    SefFuncaoCompilada *compilada = compilar_simbolo_para_instalacao(
        runtime, simbolo, true, "COMPILE-EXTERNAL-I64", &funcao, erro);
    if (compilada == NULL)
        return false;
    bool sucesso = vincular_objeto_biblioteca_i64(compilada, biblioteca, erro) &&
                   sef_funcao_compilada_preparar_jit(compilada, erro);
    if (!sucesso) {
        sef_funcao_compilada_liberar(compilada);
        return false;
    }
    instalar_compilada(funcao, compilada);
    return true;
}
