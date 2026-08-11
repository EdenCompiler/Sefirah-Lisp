#include "sefirah/runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int falhas = 0;

static int64_t dobrar_i64(int64_t valor) { return (int64_t)((uint64_t)valor * 2u); }
static int64_t combinar_i64(int64_t a, int64_t b) {
    return (int64_t)((uint64_t)a * 10u + (uint64_t)b);
}

static void verificar(bool condicao, const char *mensagem) {
    if (!condicao) {
        fprintf(stderr, "FALHOU: %s\n", mensagem);
        falhas++;
    }
}

static SefValor avaliar(SefRuntime *runtime, const char *codigo) {
    SefErro erro;
    SefValor valor = sef_runtime_avaliar_texto(runtime, codigo, &erro);
    if (valor == NULL) {
        fprintf(stderr, "avaliacao falhou para %s: %s\n", codigo, erro.mensagem);
        falhas++;
    }
    return valor;
}

static void verificar_texto(SefRuntime *runtime, const char *codigo, const char *esperado) {
    SefValor valor = avaliar(runtime, codigo);
    if (valor == NULL)
        return;
    SefErro erro;
    char *texto = sef_valor_para_texto(runtime, valor, true, &erro);
    verificar(texto != NULL, "impressor devolveu texto");
    if (texto != NULL) {
        if (strcmp(texto, esperado) != 0) {
            fprintf(stderr, "FALHOU: %s => %s; esperado %s\n", codigo, texto, esperado);
            falhas++;
        }
        sef_texto_liberar(texto);
    }
}

static void converter_assinatura(const char *caminho, int versao) {
    FILE *arquivo = fopen(caminho, "r+b");
    verificar(arquivo != NULL, "imagem de compatibilidade foi aberta para ajuste");
    if (arquivo == NULL)
        return;
    verificar(fseek(arquivo, 6, SEEK_SET) == 0 && fputc(versao, arquivo) == versao,
              "assinatura de compatibilidade foi convertida");
    verificar(fclose(arquivo) == 0, "imagem de compatibilidade foi fechada");
}

static void testar_repl(SefRuntime *runtime) {
    FILE *entrada = tmpfile();
    FILE *saida = tmpfile();
    verificar(entrada != NULL && saida != NULL, "arquivos temporarios do REPL foram criados");
    if (entrada == NULL || saida == NULL) {
        if (entrada != NULL)
            fclose(entrada);
        if (saida != NULL)
            fclose(saida);
        return;
    }

    fputs("(defun soma-repl\n  (a b)\n  (+ a b))\n", entrada);
    fputs("(soma-repl 40 2)\n", entrada);
    fputs("(values 40 41 42)\n", entrada);
    fputs(":ajuda\n:sair\n", entrada);
    rewind(entrada);
    verificar(sef_runtime_repl(runtime, entrada, saida) == 0, "REPL executou sessao completa");
    rewind(saida);

    char transcricao[8192];
    size_t lidos = fread(transcricao, 1, sizeof(transcricao) - 1, saida);
    transcricao[lidos] = '\0';
    verificar(strstr(transcricao, "......> ") != NULL, "REPL mostrou prompt de continuacao");
    verificar(strstr(transcricao, "SOMA-REPL\n") != NULL, "REPL definiu funcao multilinha");
    verificar(strstr(transcricao, "42\n") != NULL, "REPL executou a funcao definida");
    verificar(strstr(transcricao, "40\n41\n42\n") != NULL,
              "REPL imprimiu todos os valores retornados");
    verificar(strstr(transcricao, ":ajuda  mostra esta ajuda") != NULL,
              "REPL reconheceu comando interativo");
    fclose(entrada);
    fclose(saida);
}

int main(int argc, char **argv) {
    SefErro erro;
    SefRuntime *runtime = sef_runtime_criar(&erro);
    if (runtime == NULL) {
        fprintf(stderr, "runtime nao iniciou: %s\n", erro.mensagem);
        return 1;
    }

    verificar(sef_runtime_estado_codigo("(defun x (a)\n", &erro) == SEF_CODIGO_INCOMPLETO,
              "analisador reconheceu forma multilinha incompleta");
    verificar(sef_runtime_estado_codigo("(list #\\( \"texto)\")", &erro) == SEF_CODIGO_COMPLETO,
              "analisador distinguiu caractere, texto e parenteses");
    verificar(sef_runtime_estado_codigo("' ; comentario\n", &erro) == SEF_CODIGO_INCOMPLETO,
              "analisador preservou prefixo de leitura pendente");
    verificar(sef_runtime_estado_codigo("|nome com espaco", &erro) == SEF_CODIGO_INCOMPLETO &&
                  sef_runtime_estado_codigo("|nome com espaco|", &erro) == SEF_CODIGO_COMPLETO,
              "analisador acompanhou barras verticais de simbolo");
    verificar(sef_runtime_estado_codigo("simbolo\\", &erro) == SEF_CODIGO_INCOMPLETO,
              "analisador reconheceu escape de simbolo pendente");
    verificar(sef_runtime_estado_codigo("(list ')", &erro) == SEF_CODIGO_INVALIDO,
              "analisador rejeitou prefixo sem forma dentro de lista");
    verificar(sef_runtime_estado_codigo(")", &erro) == SEF_CODIGO_INVALIDO && erro.ocorreu,
              "analisador rejeitou fechamento sem abertura");
    testar_repl(runtime);

    verificar_texto(runtime, "(+ 1 2 39)", "42");
    verificar_texto(runtime,
                    "(list (symbolp nil) (symbolp '()) (symbolp :chave) (symbolp 42) "
                    "(keywordp :chave) (keywordp nil))",
                    "(T T T NIL T NIL)");
    verificar_texto(runtime,
                    "(list (symbol-name nil) (package-name (symbol-package nil)) "
                    "(boundp nil) (symbol-value nil) (fboundp nil))",
                    "(\"NIL\" \"COMMON-LISP\" T NIL NIL)");
    verificar_texto(runtime,
                    "(list (boundp :chave) (eq (symbol-value :chave) :chave) "
                    "(constantp nil) (constantp t) (constantp :chave) "
                    "(constantp 42) (constantp \"texto\") (constantp 'variavel) "
                    "(constantp ''variavel) (constantp '(+ 1 2)))",
                    "(T T T T T T T NIL T NIL)");
    verificar_texto(runtime,
                    "(list (eq (intern \"NIL\" \"COMMON-LISP\") nil) "
                    "(eq 'common-lisp:nil nil) "
                    "(multiple-value-list (find-symbol \"NIL\" \"COMMON-LISP\")) "
                    "(multiple-value-list (find-symbol nil \"COMMON-LISP\")) "
                    "(multiple-value-list (find-symbol \"NIL\" \"COMMON-LISP-USER\")) "
                    "(multiple-value-list (intern \"NIL\" \"COMMON-LISP-USER\")))",
                    "(T T (NIL :EXTERNAL) (NIL :EXTERNAL) "
                    "(NIL :INHERITED) (NIL :INHERITED))");
    verificar_texto(runtime,
                    "(list (handler-case (set nil 42) (error (c) :nil-constante)) "
                    "(handler-case (set t 42) (error (c) :t-constante)) "
                    "(handler-case (set :chave 42) (error (c) :keyword-constante)) "
                    "(handler-case (setq t 42) (error (c) :setq-constante)) "
                    "(handler-case (let ((:chave 42)) :falhou) "
                    "  (error (c) :let-constante)))",
                    "(:NIL-CONSTANTE :T-CONSTANTE :KEYWORD-CONSTANTE "
                    ":SETQ-CONSTANTE :LET-CONSTANTE)");
    verificar(sef_runtime_imagem_salvar(runtime, "teste-sefirah-v6.imagem", &erro),
              "imagem sem vetores foi salva no formato atual");
    converter_assinatura("teste-sefirah-v6.imagem", 6);
    SefRuntime *runtime_v6 = sef_runtime_imagem_abrir("teste-sefirah-v6.imagem", &erro);
    verificar(runtime_v6 != NULL, "leitor atual aceitou imagem v6");
    if (runtime_v6 != NULL) {
        verificar_texto(runtime_v6, "(+ 40 2)", "42");
        verificar_texto(runtime_v6,
                        "(list (symbolp nil) (symbol-name nil) "
                        "(package-name (symbol-package nil)) "
                        "(multiple-value-list (find-symbol \"NIL\" \"COMMON-LISP-USER\")))",
                        "(T \"NIL\" \"COMMON-LISP\" (NIL :INHERITED))");
        sef_runtime_destruir(runtime_v6);
    }
    remove("teste-sefirah-v6.imagem");
    verificar_texto(runtime, "(list (<= 1 1 2) (>= 3 2 2) (/= 1 2 3) (/= 1 2 1))", "(T T T NIL)");
    verificar_texto(runtime, "'(a b . c)", "(A B . C)");
    verificar_texto(runtime,
                    "(list (consp (cons 1 2)) (listp nil) (listp (cons 1 2)) "
                    "(endp nil) (endp '(1)) (first '(1 2)) (rest '(1 2)))",
                    "(T T T T NIL 1 (2))");
    verificar_texto(runtime,
                    "(list (nth 2 '(0 1 2 3)) (nth 9 '(0 1)) "
                    "(nthcdr 2 '(0 1 2 3)) (last '(0 1 2 3)) "
                    "(last '(0 1 2 3) 2) (last '(0 1) 0))",
                    "(2 NIL (2 3) (3) (2 3) NIL)");
    verificar_texto(runtime,
                    "(let ((a (list 1 2)) (b (list 3 4))) "
                    "(list (append a b 'fim) a b))",
                    "((1 2 3 4 . FIM) (1 2) (3 4))");
    verificar_texto(runtime,
                    "(let ((a (list 1 2)) (b (list 3 4))) "
                    "(list (eq (nconc a b) a) a))",
                    "(T (1 2 3 4))");
    verificar_texto(runtime, "(let ((p (cons 1 2))) (rplaca p 40) (rplacd p 2) p)", "(40 . 2)");
    verificar_texto(runtime,
                    "(list (member 2 '(1 2 3)) "
                    "(assoc 'b (list (cons 'a 1) (cons 'b 2))))",
                    "((2 3) (B . 2))");
    verificar_texto(runtime,
                    "(list (mapcar #'+ '(1 2 3) '(10 20)) "
                    "(let ((s 0) (xs '(10 20 12))) "
                    "(list (eq (mapc (lambda (x) (setq s (+ s x))) xs) xs) s)))",
                    "((11 22) (T 42))");
    verificar_texto(runtime, "(handler-case (endp 42) (error (c) :lista-exigida))",
                    ":LISTA-EXIGIDA");
    verificar_texto(runtime, "(let ((x 20) (y 22)) (+ x y))", "42");
    verificar_texto(runtime,
                    "(let ((h (make-hash-table))) "
                    "(setf (gethash 1 h) 40 (gethash 17 h) 42) "
                    "(list (hash-table-p h) (gethash 1 h) (gethash 2 h :ausente) "
                    "(hash-table-count h) (remhash 1 h) (hash-table-count h) "
                    "(type-of h) (clrhash h) (hash-table-count h)))",
                    "(T 40 :AUSENTE 2 T 1 HASH-TABLE #<HASH-TABLE 0> 0)");
    verificar_texto(runtime,
                    "(let ((h (make-hash-table))) "
                    "(setf (gethash 0.0 h) 42) "
                    "(list (gethash -0.0 h) (hash-table-count h)))",
                    "(42 1)");
    verificar_texto(runtime, "(multiple-value-list (values 40 41 42))", "(40 41 42)");
    verificar_texto(runtime, "(multiple-value-list (values))", "NIL");
    verificar_texto(runtime, "(multiple-value-bind (a b c) (values 40 2) (list a b c))",
                    "(40 2 NIL)");
    verificar_texto(runtime, "(multiple-value-call #'list (values 40 41) (values 42))",
                    "(40 41 42)");
    verificar_texto(runtime,
                    "(multiple-value-list "
                    "(multiple-value-prog1 (values 40 41 42) (+ 1 2)))",
                    "(40 41 42)");
    verificar_texto(runtime, "(nth-value 1 (values 40 41 42))", "41");
    verificar_texto(runtime, "(nth-value 4 (values 40 41 42))", "NIL");
    verificar_texto(runtime,
                    "(let ((h (make-hash-table))) "
                    "(setf (gethash 'presente h) nil) "
                    "(list (multiple-value-list (gethash 'presente h)) "
                    "(multiple-value-list (gethash 'ausente h))))",
                    "((NIL T) (NIL NIL))");
    verificar_texto(runtime, "(list (values 40 41) 42)", "(40 42)");
    verificar_texto(runtime, "(mapcar (lambda (x) (values x (+ x 40))) '(1 2))", "(1 2)");
    verificar_texto(runtime,
                    "(multiple-value-list "
                    "(block fim (return-from fim (values 40 41 42))))",
                    "(40 41 42)");
    verificar_texto(runtime, "(multiple-value-list (catch 'fim (throw 'fim (values 40 41 42))))",
                    "(40 41 42)");
    verificar_texto(runtime,
                    "(let ((limpo nil)) "
                    "(list (multiple-value-list "
                    "(unwind-protect (values 40 41 42) (setq limpo t))) limpo))",
                    "((40 41 42) T)");
    verificar_texto(runtime,
                    "(multiple-value-bind (valor condicao) "
                    "(ignore-errors (+ 1 desconhecido)) "
                    "(list valor (type-of condicao)))",
                    "(NIL ERROR)");
    SefValor valores_sdk = avaliar(runtime, "(values 40 41 42)");
    verificar(valores_sdk != NULL && sef_runtime_quantidade_valores(runtime) == 3,
              "SDK informou tres valores retornados");
    verificar(strcmp(sef_valor_nome_tipo(valores_sdk), "INTEGER") == 0,
              "SDK informou o tipo do valor para ferramentas residentes");
    verificar(sef_runtime_valor(runtime, 0) == valores_sdk &&
                  sef_valor_como_inteiro(sef_runtime_valor(runtime, 2)) == 42,
              "SDK preservou o valor primario e consultou os secundarios");
    verificar(sef_runtime_valor(runtime, 3) == NULL, "SDK rejeitou valor multiplo inexistente");
    verificar_texto(runtime,
                    "(let ((h (make-hash-table))) "
                    "(labels ((preencher (n) "
                    "(if (< n 1) h (progn (setf (gethash n h) (+ n 1)) "
                    "(preencher (- n 1)))))) "
                    "(preencher 100) "
                    "(list (gethash 41 h) (hash-table-count h) "
                    "(remhash 41 h) (gethash 41 h :removida))))",
                    "(42 100 T :REMOVIDA)");
    verificar_texto(runtime, "(handler-case (gethash 'chave 42) (error (c) :tabela-exigida))",
                    ":TABELA-EXIGIDA");
    verificar_texto(runtime, "(defun fat (n) (if (< n 2) 1 (* n (fat (- n 1))))) (fat 6)", "720");
    verificar_texto(runtime,
                    "(defmacro primeiro-quando (teste valor) (list 'if teste valor nil)) "
                    "(primeiro-quando t (+ 40 2))",
                    "42");
    verificar_texto(runtime,
                    "(defmacro incrementar (lugar) `(setq ,lugar (+ ,lugar 1))) "
                    "(let ((x 41)) (incrementar x) x)",
                    "42");
    verificar_texto(runtime,
                    "(defmacro fazer-lista (&rest itens) `(list ,@itens)) "
                    "(fazer-lista 40 41 42)",
                    "(40 41 42)");
    verificar_texto(runtime, "((lambda (x &rest xs) (length xs)) 1 2 3 4)", "3");
    verificar_texto(runtime, "#()", "#()");
    verificar_texto(runtime, "#(1 2 (+ 1 2))", "#(1 2 (+ 1 2))");
    verificar_texto(runtime, "#(#() #(1 2))", "#(#() #(1 2))");
    verificar_texto(runtime, "(vector 1 (+ 1 1) 'tres)", "#(1 2 TRES)");
    verificar_texto(runtime,
                    "(let ((v (make-array 3 :initial-element 7))) "
                    "(setf (aref v 1) 42) "
                    "(list v (length v) (aref v 0) (svref v 1) "
                    "(vectorp v) (arrayp v) (type-of v)))",
                    "(#(7 42 7) 3 7 42 T T VECTOR)");
    verificar_texto(runtime,
                    "(let ((x 1) (p (list 2 3))) "
                    "(setf x 40 (car p) 41 (cdr p) (list 42)) (list x p))",
                    "(40 (41 42))");
    verificar_texto(runtime, "(handler-case (aref #(1 2) 2) (error (c) :limite-detectado))",
                    ":LIMITE-DETECTADO");
    SefValor vetor_sdk = avaliar(runtime, "(vector 40 41 42)");
    verificar(sef_valor_e_vetor(vetor_sdk), "SDK reconheceu vetor");
    verificar(sef_vetor_tamanho(vetor_sdk) == 3, "SDK informou tamanho do vetor");
    verificar(sef_valor_como_inteiro(sef_vetor_obter(vetor_sdk, 2)) == 42,
              "SDK obteve item do vetor");
    verificar(sef_vetor_definir(vetor_sdk, 1, sef_vetor_obter(vetor_sdk, 2), &erro) &&
                  sef_valor_como_inteiro(sef_vetor_obter(vetor_sdk, 1)) == 42,
              "SDK alterou item do vetor");
    verificar(sef_vetor_obter(vetor_sdk, 3) == NULL, "SDK rejeitou indice fora do vetor");
    char rotulo_componente[64];
    SefValor componente_sdk = NULL;
    verificar(sef_valor_quantidade_componentes(runtime, vetor_sdk) == 3 &&
                  sef_valor_componente(runtime, vetor_sdk, 2, &componente_sdk, rotulo_componente,
                                       sizeof(rotulo_componente)) &&
                  strcmp(rotulo_componente, "[2]") == 0 &&
                  sef_valor_como_inteiro(componente_sdk) == 42,
              "SDK inspecionou componente estrutural de vetor");
    verificar(!sef_valor_componente(runtime, vetor_sdk, 3, &componente_sdk, rotulo_componente,
                                    sizeof(rotulo_componente)),
              "SDK rejeitou componente estrutural inexistente");
    SefValor vetor_criado = sef_vetor_criar(runtime, 2, sef_vetor_obter(vetor_sdk, 2), &erro);
    verificar(vetor_criado != NULL && sef_vetor_tamanho(vetor_criado) == 2 &&
                  sef_valor_como_inteiro(sef_vetor_obter(vetor_criado, 0)) == 42,
              "SDK criou vetor inicializado");
    SefValor par_sdk = avaliar(runtime, "(cons 40 42)");
    verificar(sef_valor_quantidade_componentes(runtime, par_sdk) == 2 &&
                  sef_valor_componente(runtime, par_sdk, 0, &componente_sdk, rotulo_componente,
                                       sizeof(rotulo_componente)) &&
                  strcmp(rotulo_componente, "PRIMEIRO") == 0 &&
                  sef_valor_como_inteiro(componente_sdk) == 40 &&
                  sef_valor_componente(runtime, par_sdk, 1, &componente_sdk, rotulo_componente,
                                       sizeof(rotulo_componente)) &&
                  strcmp(rotulo_componente, "RESTO") == 0 &&
                  sef_valor_como_inteiro(componente_sdk) == 42,
              "SDK expôs os dois componentes de um par");
    SefValor nulo_sdk = avaliar(runtime, "nil");
    verificar(sef_valor_quantidade_componentes(runtime, nulo_sdk) == 1 &&
                  sef_valor_componente(runtime, nulo_sdk, 0, &componente_sdk, rotulo_componente,
                                       sizeof(rotulo_componente)) &&
                  strcmp(rotulo_componente, "PACOTE") == 0 &&
                  strcmp(sef_valor_nome_tipo(componente_sdk), "PACKAGE") == 0,
              "SDK preservou a identidade simbolica de NIL na introspeccao");
    SefValor funcao_sdk = avaliar(runtime, "#'(lambda (x) (+ x 1))");
    verificar(sef_valor_quantidade_componentes(runtime, funcao_sdk) == 3 &&
                  sef_valor_componente(runtime, funcao_sdk, 2, &componente_sdk, rotulo_componente,
                                       sizeof(rotulo_componente)) &&
                  strcmp(rotulo_componente, "AMBIENTE") == 0 &&
                  strcmp(sef_valor_nome_tipo(componente_sdk), "SEFIRAH::ENVIRONMENT") == 0 &&
                  sef_valor_quantidade_componentes(runtime, componente_sdk) >= 1,
              "SDK inspecionou funcao e seu ambiente lexico");
    SefValor condicao_sdk = avaliar(runtime, "(handler-case (error \"falha sdk\") (error (c) c))");
    verificar(sef_valor_quantidade_componentes(runtime, condicao_sdk) == 2 &&
                  sef_valor_componente(runtime, condicao_sdk, 1, &componente_sdk, rotulo_componente,
                                       sizeof(rotulo_componente)) &&
                  strcmp(rotulo_componente, "MENSAGEM") == 0 &&
                  strcmp(sef_valor_nome_tipo(componente_sdk), "STRING") == 0,
              "SDK expôs classe e mensagem da condicao");
    SefValor falha_condicao = sef_runtime_avaliar_texto(runtime, "(error \"nao tratada\")", &erro);
    sef_runtime_coletar(runtime, NULL);
    SefValor ultima_condicao = sef_runtime_ultima_condicao(runtime);
    verificar(falha_condicao == NULL && erro.ocorreu && ultima_condicao != NULL &&
                  strcmp(sef_valor_nome_tipo(ultima_condicao), "CONDITION") == 0,
              "runtime reteve a identidade da ultima condicao nao tratada");
    verificar_texto(runtime, "(+ 40 2)", "42");
    verificar(sef_runtime_ultima_condicao(runtime) == NULL,
              "avaliacao concluida limpou a condicao nao tratada anterior");
    SefValor hash_sdk = avaliar(runtime, "(let ((h (make-hash-table))) "
                                         "(setf (gethash 'chave h) 42) h)");
    verificar(sef_valor_quantidade_componentes(runtime, hash_sdk) == 2 &&
                  sef_valor_componente(runtime, hash_sdk, 0, &componente_sdk, rotulo_componente,
                                       sizeof(rotulo_componente)) &&
                  strcmp(rotulo_componente, "CHAVE 1") == 0 &&
                  sef_valor_componente(runtime, hash_sdk, 1, &componente_sdk, rotulo_componente,
                                       sizeof(rotulo_componente)) &&
                  strcmp(rotulo_componente, "VALOR 1") == 0 &&
                  sef_valor_como_inteiro(componente_sdk) == 42,
              "SDK inspecionou pares de chave e valor da hash table");
    verificar_texto(runtime, "(define vetor-v7 #(40 41 42))", "VETOR-V7");
    verificar(sef_runtime_imagem_salvar(runtime, "teste-sefirah-v7.imagem", &erro),
              "imagem com vetor foi salva no formato atual");
    converter_assinatura("teste-sefirah-v7.imagem", 7);
    SefRuntime *runtime_v7 = sef_runtime_imagem_abrir("teste-sefirah-v7.imagem", &erro);
    verificar(runtime_v7 != NULL, "leitor atual aceitou imagem v7 com vetor");
    if (runtime_v7 != NULL) {
        verificar_texto(runtime_v7, "vetor-v7", "#(40 41 42)");
        sef_runtime_destruir(runtime_v7);
    }
    remove("teste-sefirah-v7.imagem");
    verificar_texto(runtime, "(list #\\A #\\Space #\\Newline #\\é #\\U+03BB)",
                    "(#\\A #\\Space #\\Newline #\\é #\\λ)");
    verificar_texto(runtime,
                    "(list (characterp #\\é) (stringp \"ação\") "
                    "(char-code #\\é) (code-char 233) (type-of #\\λ))",
                    "(T T 233 #\\é CHARACTER)");
    verificar_texto(runtime,
                    "(list (char= #\\a #\\a) (char/= #\\a #\\b #\\c) "
                    "(char< #\\a #\\b #\\c) (char>= #\\c #\\b #\\b) "
                    "(eql #\\λ #\\λ) (equal \"ação\" \"ação\") "
                    "(equal '(1 #\\a) '(1 #\\a)))",
                    "(T T T T T T T)");
    verificar_texto(runtime,
                    "(list (length \"ação\") (char \"ação\" 1) "
                    "(schar \"ação\" 3) (elt '(40 41 42) 2) (elt #(40 41 42) 2))",
                    "(4 #\\ç #\\o 42 42)");
    verificar_texto(runtime,
                    "(let ((s \"abc\") (t2 \"ação\") (nulo \"a\") "
                    "(v #(1 2 3)) (l (list 1 2 3))) "
                    "(setf (char s 1) #\\é (char t2 1) #\\X "
                    "(char nulo 0) #\\Null (elt v 1) 42 (elt l 2) 42) "
                    "(list s t2 nulo (length nulo) v l))",
                    "(\"aéc\" \"aXão\" \"\\0\" 1 #(1 42 3) (1 2 42))");
    verificar_texto(runtime,
                    "(list (reverse #(1 2 3)) (reverse \"ação\") "
                    "(reverse '(1 2 3)) (subseq #(0 1 2 3) 1 3) "
                    "(subseq \"ação\" 1 3) (subseq '(0 1 2 3) 2))",
                    "(#(3 2 1) \"oãça\" (3 2 1) #(1 2) \"çã\" (2 3))");
    verificar_texto(runtime,
                    "(let* ((v #(1 2 3)) (vc (copy-seq v)) "
                    "(s \"ação\") (sc (copy-seq s)) "
                    "(l (list 1 2 3)) (lc (copy-seq l))) "
                    "(setf (aref vc 1) 42 (char sc 1) #\\X (car lc) 42) "
                    "(list v vc s sc l lc))",
                    "(#(1 2 3) #(1 42 3) \"ação\" \"aXão\" (1 2 3) (42 2 3))");
    verificar_texto(runtime,
                    "(let ((v #(1 2 3 4)) (s \"ação\") (l (list 1 2 3 4))) "
                    "(fill v 9 :start 1 :end 3) "
                    "(fill s #\\λ :start 1 :end 3) (fill l 9 :start 2) "
                    "(list v s l))",
                    "(#(1 9 9 4) \"aλλo\" (1 2 9 9))");
    verificar_texto(runtime,
                    "(handler-case (subseq #(1 2) 2 1) "
                    "(error (c) :intervalo-detectado))",
                    ":INTERVALO-DETECTADO");
    SefValor caractere_sdk = sef_caractere_criar(runtime, 0x03bbu, &erro);
    verificar(sef_valor_e_caractere(caractere_sdk) &&
                  sef_caractere_codigo(caractere_sdk) == 0x03bbu,
              "SDK criou e consultou caractere Unicode");
    verificar(sef_caractere_criar(runtime, 0xd800u, &erro) == NULL && erro.ocorreu,
              "SDK rejeitou surrogate Unicode");
    verificar_texto(runtime, "(define caractere-v8 #\\λ)", "CARACTERE-V8");
    verificar(sef_runtime_imagem_salvar(runtime, "teste-sefirah-v8.imagem", &erro),
              "imagem com caractere foi salva no formato atual");
    converter_assinatura("teste-sefirah-v8.imagem", 8);
    SefRuntime *runtime_v8 = sef_runtime_imagem_abrir("teste-sefirah-v8.imagem", &erro);
    verificar(runtime_v8 != NULL, "leitor atual aceitou imagem v8 com caractere");
    if (runtime_v8 != NULL) {
        verificar_texto(runtime_v8, "caractere-v8", "#\\λ");
        sef_runtime_destruir(runtime_v8);
    }
    remove("teste-sefirah-v8.imagem");
    verificar_texto(runtime, "(type-of \"sefirah\")", "STRING");
    verificar_texto(runtime, "(> (sefirah::object-count) 0)", "T");
    verificar_texto(runtime,
                    "(define tabela-v9 (make-hash-table)) "
                    "(setf (gethash 'legado tabela-v9) 42) "
                    "(gethash 'legado tabela-v9)",
                    "42");
    verificar(sef_runtime_imagem_salvar(runtime, "teste-sefirah-v9.imagem", &erro),
              "imagem sem objetos v10 foi salva no formato atual");
    converter_assinatura("teste-sefirah-v9.imagem", 9);
    SefRuntime *runtime_v9 = sef_runtime_imagem_abrir("teste-sefirah-v9.imagem", &erro);
    verificar(runtime_v9 != NULL, "leitor atual aceitou imagem v9 com tabela hash");
    if (runtime_v9 != NULL) {
        verificar_texto(runtime_v9, "(gethash 'legado tabela-v9)", "42");
        sef_runtime_destruir(runtime_v9);
    }
    remove("teste-sefirah-v9.imagem");
    verificar_texto(runtime,
                    "(define celula-separada 41) "
                    "(defun celula-separada () 42) "
                    "(list celula-separada (celula-separada))",
                    "(41 42)");
    verificar_texto(runtime, "(funcall #'+ 20 22)", "42");
    verificar_texto(runtime,
                    "(list (boundp 'celula-separada) "
                    "(fboundp 'celula-separada) (functionp #'+))",
                    "(T T T)");
    verificar_texto(runtime, "(let ((x 2)) (let* ((x 20) (y (+ x 22))) y))", "42");
    verificar_texto(runtime, "(cond ((> 1 2) 0) ((< 1 2) 42) (t 99))", "42");
    verificar_texto(runtime, "(flet ((dobro (x) (* x 2))) (dobro 21))", "42");
    verificar_texto(runtime,
                    "(labels ((soma-ate (n) (if (< n 1) 0 (+ n (soma-ate (- n 1)))))) "
                    "(soma-ate 9))",
                    "45");
    verificar_texto(runtime, "(list (when t 42) (unless nil 43) :palavra-chave)",
                    "(42 43 :PALAVRA-CHAVE)");
    verificar_texto(runtime, "(block pronto (+ 1 (return-from pronto 42) 100))", "42");
    verificar_texto(runtime, "(block nil (return 42) 99)", "42");
    verificar_texto(runtime, "(catch 'fim (+ 1 (throw 'fim 42) 100))", "42");
    verificar_texto(runtime,
                    "(let ((limpo nil)) "
                    "(list (block fim (unwind-protect (return-from fim 42) "
                    "(setq limpo t))) limpo))",
                    "(42 T)");
    verificar_texto(runtime, "(ignore-errors (+ 1 simbolo-inexistente))", "NIL");
    verificar_texto(runtime,
                    "(handler-case (error \"falha controlada\") "
                    "(error (condicao) (list (type-of condicao) condicao)))",
                    "(ERROR #<ERROR falha controlada>)");
    verificar_texto(runtime,
                    "(restart-case (invoke-restart 'use-value 42) "
                    "(use-value (valor) (+ valor 1)))",
                    "43");
    verificar_texto(runtime,
                    "(restart-case "
                    "(list (find-restart 'a) (compute-restarts) "
                    "(restart-name (find-restart 'a))) "
                    "(a () :a) (b () :b))",
                    "(#<RESTART A> (#<RESTART A> #<RESTART B>) A)");
    verificar_texto(runtime,
                    "(restart-case "
                    "(let ((r (find-restart 'identidade))) "
                    "(list (type-of r) (eq r (first (compute-restarts))) "
                    "(eq r (find-restart r)) (restart-name r))) "
                    "(identidade () :falhou))",
                    "(RESTART T T IDENTIDADE)");
    verificar_texto(runtime,
                    "(restart-case (invoke-restart (first (compute-restarts))) "
                    "(nil () 42))",
                    "42");
    verificar_texto(runtime,
                    "(multiple-value-list "
                    "(restart-case (values 40 41) (substituir () 0)))",
                    "(40 41)");
    verificar_texto(runtime,
                    "(multiple-value-list "
                    "(restart-case (invoke-restart 'trocar 40 41) "
                    "(trocar (a b) (values b a))))",
                    "(41 40)");
    verificar_texto(runtime,
                    "(list (restart-case (use-value 44) (use-value (x) x)) "
                    "(restart-case (store-value 45) (store-value (x) x)) "
                    "(restart-case (continue) (continue () 46)) "
                    "(restart-case (abort) (abort () 47)) "
                    "(restart-case (muffle-warning) (muffle-warning () 48)))",
                    "(44 45 46 47 48)");
    verificar_texto(runtime,
                    "(restart-case (restart-case (invoke-restart 'mesmo) "
                    "(mesmo () 1)) (mesmo () 2))",
                    "1");
    verificar_texto(runtime,
                    "(let ((limpo nil)) "
                    "(list (restart-case "
                    "(unwind-protect (invoke-restart 'usar 42) (setq limpo t)) "
                    "(usar (valor) valor)) limpo))",
                    "(42 T)");
    verificar_texto(runtime,
                    "(list (block fim "
                    "(restart-case (return-from fim 42) (temporario () 0))) "
                    "(find-restart 'temporario))",
                    "(42 NIL)");
    verificar_texto(runtime,
                    "(define reinicio-salvo "
                    "(restart-case (find-restart 'temporario) (temporario () nil))) "
                    "(define reinicio-anonimo-salvo "
                    "(restart-case (first (compute-restarts)) (nil () nil))) "
                    "(list (type-of reinicio-salvo) (restart-name reinicio-salvo) "
                    "(find-restart reinicio-salvo) (restart-name reinicio-anonimo-salvo))",
                    "(RESTART TEMPORARIO NIL NIL)");
    SefValor reinicio_sdk = avaliar(runtime, "reinicio-salvo");
    verificar(reinicio_sdk != NULL && strcmp(sef_valor_nome_tipo(reinicio_sdk), "RESTART") == 0 &&
                  sef_valor_quantidade_componentes(runtime, reinicio_sdk) == 2 &&
                  sef_valor_componente(runtime, reinicio_sdk, 0, &componente_sdk,
                                       rotulo_componente, sizeof(rotulo_componente)) &&
                  strcmp(rotulo_componente, "NOME") == 0 &&
                  strcmp(sef_valor_nome_tipo(componente_sdk), "SYMBOL") == 0 &&
                  sef_valor_componente(runtime, reinicio_sdk, 1, &componente_sdk,
                                       rotulo_componente, sizeof(rotulo_componente)) &&
                  strcmp(rotulo_componente, "ATIVO") == 0 &&
                  sef_valor_e_nulo(runtime, componente_sdk),
              "SDK inspecionou nome e atividade do objeto RESTART");
    verificar_texto(runtime,
                    "(handler-case (invoke-restart reinicio-salvo) "
                    "(error (condicao) :reinicio-inativo))",
                    ":REINICIO-INATIVO");
    verificar(sef_runtime_imagem_salvar(runtime, "teste-sefirah-v10.imagem", &erro),
              "imagem v10 persistiu objeto RESTART inativo");
    SefRuntime *runtime_v10 =
        sef_runtime_imagem_abrir("teste-sefirah-v10.imagem", &erro);
    verificar(runtime_v10 != NULL, "leitor abriu imagem v10 com objeto RESTART");
    if (runtime_v10 != NULL) {
        verificar_texto(runtime_v10,
                        "(list (type-of reinicio-salvo) (restart-name reinicio-salvo) "
                        "(find-restart reinicio-salvo) "
                        "(type-of reinicio-anonimo-salvo) "
                        "(restart-name reinicio-anonimo-salvo))",
                        "(RESTART TEMPORARIO NIL RESTART NIL)");
        verificar_texto(runtime_v10,
                        "(restart-case (invoke-restart (find-restart 'restaurado) 42) "
                        "(restaurado (valor) valor))",
                        "42");
        sef_runtime_destruir(runtime_v10);
    }
    remove("teste-sefirah-v10.imagem");
    verificar_texto(runtime,
                    "(handler-case (invoke-restart 'ausente) "
                    "(error (condicao) :reinicio-ausente))",
                    ":REINICIO-AUSENTE");
    verificar_texto(runtime,
                    "(restart-case "
                    "(handler-bind ((error (lambda (condicao) "
                    "(invoke-restart 'use-value 42)))) "
                    "(error \"recuperavel\")) "
                    "(use-value (valor) valor))",
                    "42");
    verificar_texto(runtime,
                    "(defun escolher-valor-do-handler (condicao) "
                    "(invoke-restart 'usar-designador 43)) "
                    "(restart-case "
                    "(handler-bind ((error 'escolher-valor-do-handler)) "
                    "(error \"designador\")) "
                    "(usar-designador (valor) valor))",
                    "43");
    verificar_texto(runtime,
                    "(let ((visto nil)) "
                    "(list (handler-bind "
                    "((condition (lambda (c) (setq visto :primeiro))) "
                    "(condition (lambda (c) (setq visto :segundo)))) "
                    "(signal \"aviso\")) visto))",
                    "(NIL :PRIMEIRO)");
    verificar_texto(runtime,
                    "(let ((visto nil)) "
                    "(handler-bind ((condition (lambda (c) (setq visto :externo)))) "
                    "(handler-bind ((condition (lambda (c) (setq visto :interno)))) "
                    "(signal \"aviso\"))) visto)",
                    ":EXTERNO");
    verificar_texto(runtime,
                    "(let ((vazou nil)) "
                    "(block fim (handler-bind "
                    "((condition (lambda (c) (setq vazou t)))) "
                    "(return-from fim 42))) "
                    "(signal \"fora\") vazou)",
                    "NIL");
    SefValor transiente = avaliar(runtime, "(list 'objeto 'retido 42)");
    SefRaiz *raiz = transiente == NULL ? NULL : sef_raiz_criar(runtime, transiente, &erro);
    verificar(raiz != NULL, "handle de raiz foi criado");
    verificar_texto(runtime, "(list 1 2 3 4 5)", "(1 2 3 4 5)");
    if (raiz != NULL) {
        char *texto_retido = sef_valor_para_texto(runtime, sef_raiz_valor(raiz), true, &erro);
        verificar(texto_retido != NULL && strcmp(texto_retido, "(OBJETO RETIDO 42)") == 0,
                  "raiz preservou valor entre avaliacoes");
        sef_texto_liberar(texto_retido);
        sef_raiz_liberar(raiz);
    }
    verificar_texto(runtime,
                    "(define condicao-salva "
                    "(handler-case (error \"persistente\") (error (c) c)))",
                    "CONDICAO-SALVA");
    verificar_texto(runtime,
                    "(defpackage :alpha (:use :common-lisp) "
                    "(:export :resposta-do-pacote)) "
                    "(in-package :alpha) "
                    "(defun resposta-do-pacote () (+ 40 2)) "
                    "(in-package :common-lisp-user) "
                    "(list (alpha:resposta-do-pacote) "
                    "(package-name (find-package \"alpha\")) "
                    "(find-symbol \"RESPOSTA-DO-PACOTE\" \"ALPHA\"))",
                    "(42 \"ALPHA\" ALPHA:RESPOSTA-DO-PACOTE)");
    verificar_texto(runtime,
                    "(list (eq (intern \"X\" \"ALPHA\") "
                    "(intern \"X\" \"COMMON-LISP-USER\")) "
                    "(packagep *package*) :chave)",
                    "(NIL T :CHAVE)");
    verificar_texto(runtime, "(intern \"PRIVADO\" \"ALPHA\")", "ALPHA::PRIVADO");
    verificar_texto(runtime,
                    "(list (multiple-value-list (find-symbol \"AUSENTE\" \"ALPHA\")) "
                    "(multiple-value-list (find-symbol \"PRIVADO\" \"ALPHA\")) "
                    "(multiple-value-list (find-symbol \"RESPOSTA-DO-PACOTE\" \"ALPHA\")) "
                    "(multiple-value-list (find-symbol \"CAR\" \"COMMON-LISP-USER\")))",
                    "((NIL NIL) (ALPHA::PRIVADO :INTERNAL) "
                    "(ALPHA:RESPOSTA-DO-PACOTE :EXTERNAL) (CAR :INHERITED))");
    verificar_texto(runtime,
                    "(list (multiple-value-list (intern \"NOVO-STATUS\" \"ALPHA\")) "
                    "(multiple-value-list (intern \"NOVO-STATUS\" \"ALPHA\")) "
                    "(multiple-value-list (intern \"CAR\" \"COMMON-LISP-USER\")))",
                    "((ALPHA::NOVO-STATUS NIL) (ALPHA::NOVO-STATUS :INTERNAL) "
                    "(CAR :INHERITED))");
    verificar_texto(runtime,
                    "(list (multiple-value-list (intern \"STATUS-KEY\" \"KEYWORD\")) "
                    "(multiple-value-list (intern \"STATUS-KEY\" \"KEYWORD\")) "
                    "(multiple-value-list (find-symbol \"STATUS-KEY\" \"KEYWORD\")))",
                    "((:STATUS-KEY NIL) (:STATUS-KEY :EXTERNAL) (:STATUS-KEY :EXTERNAL))");
    verificar_texto(runtime,
                    "(let ((simbolo (intern \"Nome-Misto\" \"ALPHA\"))) "
                    "(list (symbol-name simbolo) "
                    "(multiple-value-list (find-symbol \"Nome-Misto\" \"ALPHA\")) "
                    "(multiple-value-list (find-symbol \"NOME-MISTO\" \"ALPHA\"))))",
                    "(\"Nome-Misto\" (ALPHA::|Nome-Misto| :INTERNAL) (NIL NIL))");
    verificar_texto(runtime,
                    "(list '|Nome Misto| 'simbolo\\ com\\ espaco "
                    "'|barra\\|e\\\\escape| :|Chave Mista|)",
                    "(|Nome Misto| |SIMBOLO COM ESPACO| |barra\\|e\\\\escape| "
                    ":|Chave Mista|)");
    SefValor simbolo_escapado = avaliar(runtime, "'|Nome \\| com \\\\ escape|");
    char *simbolo_impresso = sef_valor_para_texto(runtime, simbolo_escapado, true, &erro);
    verificar(simbolo_impresso != NULL, "impressor produziu simbolo escapado legivel");
    if (simbolo_impresso != NULL) {
        char codigo_relido[256];
        int tamanho_relido =
            snprintf(codigo_relido, sizeof(codigo_relido), "(quote %s)", simbolo_impresso);
        SefValor simbolo_relido =
            tamanho_relido > 0 && (size_t)tamanho_relido < sizeof(codigo_relido)
                ? sef_runtime_avaliar_texto(runtime, codigo_relido, &erro)
                : NULL;
        verificar(simbolo_relido != NULL && simbolo_relido == simbolo_escapado,
                  "simbolo escapado sobreviveu ao ciclo imprimir e ler");
        sef_texto_liberar(simbolo_impresso);
    }
    SefValor privado = sef_runtime_avaliar_texto(runtime, "alpha:privado", &erro);
    verificar(privado == NULL && erro.ocorreu, "dois-pontos simples rejeita simbolo interno");
    verificar_texto(runtime,
                    "(make-package :origem-a) (make-package :origem-b) "
                    "(make-package :destino) "
                    "(export (intern \"CHOQUE\" \"ORIGEM-A\") \"ORIGEM-A\") "
                    "(export (intern \"CHOQUE\" \"ORIGEM-B\") \"ORIGEM-B\") "
                    "(use-package \"ORIGEM-A\" \"DESTINO\") "
                    "(handler-case (use-package \"ORIGEM-B\" \"DESTINO\") "
                    "(error (c) :conflito-detectado))",
                    ":CONFLITO-DETECTADO");
    verificar_texto(runtime,
                    "(defun calcular-compilado (x y) "
                    "(if (< x y) (+ (* x 2) y) (- x y)))",
                    "CALCULAR-COMPILADO");
    SefFuncaoCompilada *compilada =
        sef_runtime_compilar_funcao_i64(runtime, "CALCULAR-COMPILADO", &erro);
    verificar(compilada != NULL, "funcao Lisp foi baixada para IR e codigo nativo");
    if (compilada != NULL) {
        int64_t argumentos_compilados[2] = {10, 22};
        int64_t resultado_compilado = 0;
        verificar(sef_funcao_compilada_executar_i64(compilada, argumentos_compilados, 2,
                                                    &resultado_compilado, &erro) &&
                      resultado_compilado == 42,
                  "funcao Lisp compilada executou consequente");
        argumentos_compilados[0] = 50;
        argumentos_compilados[1] = 8;
        verificar(sef_funcao_compilada_executar_i64(compilada, argumentos_compilados, 2,
                                                    &resultado_compilado, &erro) &&
                      resultado_compilado == 42,
                  "funcao Lisp compilada executou alternativa");
        sef_funcao_compilada_liberar(compilada);
    }
    verificar_texto(runtime,
                    "(list (compile 'calcular-compilado) "
                    "(calcular-compilado 10 22) "
                    "(calcular-compilado 50 8) "
                    "(funcall #'calcular-compilado 10 22) "
                    "(apply #'calcular-compilado '(50 8)) "
                    "(type-of #'calcular-compilado) "
                    "(compiled-function-p #'calcular-compilado))",
                    "(CALCULAR-COMPILADO 42 42 42 42 COMPILED-FUNCTION T)");
    verificar_texto(runtime,
                    "(handler-case (calcular-compilado 1.5 2) "
                    "(error (c) :tipo-compilado-rejeitado))",
                    ":TIPO-COMPILADO-REJEITADO");
    verificar_texto(runtime, "(defun operacao-nao-compilavel (x) (print x))",
                    "OPERACAO-NAO-COMPILAVEL");
    compilada = sef_runtime_compilar_funcao_i64(runtime, "OPERACAO-NAO-COMPILAVEL", &erro);
    verificar(compilada == NULL && erro.ocorreu,
              "frontend recusou operacao fora do subconjunto i64");
    verificar_texto(runtime, "(defun chamar-externa (x) (external-i64 \"dobrar_i64\" x))",
                    "CHAMAR-EXTERNA");
    verificar_texto(runtime,
                    "(defun combinar-externa (a b) "
                    "(external-i64 \"combinar_i64\" a b))",
                    "COMBINAR-EXTERNA");
    compilada = sef_runtime_compilar_objeto_i64(runtime, "CHAMAR-EXTERNA", &erro);
    verificar(compilada != NULL, "frontend Lisp baixou chamada C externa para objeto");
    if (compilada != NULL) {
        verificar(sef_funcao_compilada_gravar_elf(compilada, "chamar_externa",
                                                  "teste-frontend-externo.o", &erro) &&
                      sef_funcao_compilada_gravar_coff(compilada, "chamar_externa",
                                                       "teste-frontend-externo.obj", &erro) &&
                      sef_funcao_compilada_gravar_macho(compilada, "chamar_externa",
                                                        "teste-frontend-externo-macho.o", &erro),
                  "frontend externo gerou ELF, COFF e Mach-O");
        int64_t argumento_externo = 21;
        int64_t resultado_externo = 0;
        verificar(
            sef_funcao_compilada_vincular_externa_i64(compilada, "dobrar_i64", dobrar_i64, &erro) &&
                sef_funcao_compilada_preparar_jit(compilada, &erro) &&
                sef_funcao_compilada_executar_i64(compilada, &argumento_externo, 1,
                                                  &resultado_externo, &erro) &&
                resultado_externo == 42,
            "frontend externo vinculou trampolim e executou no JIT");
        sef_funcao_compilada_liberar(compilada);
    }
    remove("teste-frontend-externo.o");
    remove("teste-frontend-externo.obj");
    remove("teste-frontend-externo-macho.o");
    compilada = sef_runtime_compilar_funcao_i64(runtime, "CHAMAR-EXTERNA", &erro);
    verificar(compilada == NULL && erro.ocorreu,
              "JIT rejeitou chamada externa sem resolvedor dinamico");
    compilada = sef_runtime_compilar_objeto_i64(runtime, "COMBINAR-EXTERNA", &erro);
    verificar(compilada != NULL, "frontend Lisp baixou chamada externa binaria");
    if (compilada != NULL) {
        int64_t argumentos_externos[2] = {4, 2};
        int64_t resultado_externo = 0;
        verificar(sef_funcao_compilada_vincular_externa_i64_binaria(compilada, "combinar_i64",
                                                                    combinar_i64, &erro) &&
                      sef_funcao_compilada_preparar_jit(compilada, &erro) &&
                      sef_funcao_compilada_executar_i64(compilada, argumentos_externos, 2,
                                                        &resultado_externo, &erro) &&
                      resultado_externo == 42,
                  "SDK vinculou e executou chamada externa binaria");
        sef_funcao_compilada_liberar(compilada);
    }
    verificar(argc == 2, "teste recebeu caminho da biblioteca FFI");
    if (argc == 2) {
        char codigo_ffi[1024];
        int escritos = snprintf(codigo_ffi, sizeof(codigo_ffi),
                                "(list (compile-external-i64 'chamar-externa \"%s\") "
                                "(chamar-externa 21) "
                                "(compiled-function-p #'chamar-externa))",
                                argv[1]);
        verificar(escritos > 0 && (size_t)escritos < sizeof(codigo_ffi),
                  "codigo de teste FFI coube no buffer");
        if (escritos > 0 && (size_t)escritos < sizeof(codigo_ffi))
            verificar_texto(runtime, codigo_ffi, "(CHAMAR-EXTERNA 42 T)");

        escritos = snprintf(codigo_ffi, sizeof(codigo_ffi),
                            "(define biblioteca-teste (open-shared-library \"%s\")) "
                            "(list (shared-library-p biblioteca-teste) "
                            "(shared-library-open-p biblioteca-teste) (type-of biblioteca-teste) "
                            "(compile-external-i64 'chamar-externa biblioteca-teste) "
                            "(compile-external-i64 'combinar-externa biblioteca-teste) "
                            "(close-shared-library biblioteca-teste) "
                            "(shared-library-open-p biblioteca-teste) "
                            "(chamar-externa 21) (combinar-externa 4 2) biblioteca-teste)",
                            argv[1]);
        verificar(escritos > 0 && (size_t)escritos < sizeof(codigo_ffi),
                  "codigo de objeto de biblioteca coube no buffer");
        if (escritos > 0 && (size_t)escritos < sizeof(codigo_ffi))
            verificar_texto(runtime, codigo_ffi,
                            "(T T SEFIRAH::SHARED-LIBRARY CHAMAR-EXTERNA COMBINAR-EXTERNA "
                            "T NIL 42 42 "
                            "#<BIBLIOTECA-COMPARTILHADA FECHADA>)");
        verificar_texto(runtime,
                        "(list (handler-case "
                        "(compile-external-i64 'chamar-externa biblioteca-teste) "
                        "(error (c) :biblioteca-fechada)) "
                        "(chamar-externa 21) (combinar-externa 4 2))",
                        "(:BIBLIOTECA-FECHADA 42 42)");

        escritos = snprintf(codigo_ffi, sizeof(codigo_ffi),
                            "(define biblioteca-imagem (open-shared-library \"%s\"))", argv[1]);
        verificar(escritos > 0 && (size_t)escritos < sizeof(codigo_ffi),
                  "codigo de biblioteca para imagem coube no buffer");
        if (escritos > 0 && (size_t)escritos < sizeof(codigo_ffi))
            verificar_texto(runtime, codigo_ffi, "BIBLIOTECA-IMAGEM");
        verificar(!sef_runtime_imagem_salvar(runtime, "imagem-biblioteca-aberta.imagem", &erro) &&
                      erro.ocorreu,
                  "imagem rejeita biblioteca compartilhada aberta");
        verificar_texto(runtime, "(close-shared-library biblioteca-imagem)", "T");
        remove("imagem-biblioteca-aberta.imagem");
    }
    verificar_texto(runtime, "(defun externa-invalida (x) (external-i64 42 x))",
                    "EXTERNA-INVALIDA");
    compilada = sef_runtime_compilar_objeto_i64(runtime, "EXTERNA-INVALIDA", &erro);
    verificar(compilada == NULL && erro.ocorreu,
              "frontend exigiu string como nome de simbolo externo");
    verificar_texto(runtime,
                    "(defun externa-com-argumentos-demais (a b c) "
                    "(external-i64 \"combinar_i64\" a b c))",
                    "EXTERNA-COM-ARGUMENTOS-DEMAIS");
    compilada = sef_runtime_compilar_objeto_i64(runtime, "EXTERNA-COM-ARGUMENTOS-DEMAIS", &erro);
    verificar(compilada == NULL && erro.ocorreu,
              "frontend limitou chamada externa i64 a duas entradas");
    verificar_texto(runtime,
                    "(list (streamp *standard-input*) (streamp *standard-output*) "
                    "(type-of *error-output*))",
                    "(T T STREAM)");
    verificar_texto(runtime,
                    "(define fluxo-saida "
                    "(open \"teste-stream-sefirah.txt\" :direction :output "
                    ":if-exists :supersede)) "
                    "(write-string \"linha 42\" fluxo-saida) "
                    "(terpri fluxo-saida) (finish-output fluxo-saida) "
                    "(write-string \"ultima linha\" fluxo-saida) "
                    "(close fluxo-saida)",
                    "T");
    verificar_texto(runtime,
                    "(define fluxo-entrada (open \"teste-stream-sefirah.txt\")) "
                    "(list (multiple-value-list (read-line fluxo-entrada)) "
                    "(multiple-value-list (read-line fluxo-entrada)) "
                    "(multiple-value-list (read-line fluxo-entrada nil :fim t)) "
                    "(handler-case (read-line fluxo-entrada) "
                    "(error (condicao) :fim-sinalizado)) "
                    "(close fluxo-entrada))",
                    "((\"linha 42\" NIL) (\"ultima linha\" T) (:FIM T) "
                    ":FIM-SINALIZADO T)");
    verificar_texto(runtime,
                    "(define fluxo-imagem "
                    "(open \"teste-stream-imagem.txt\" :direction :output))",
                    "FLUXO-IMAGEM");
    verificar(!sef_runtime_imagem_salvar(runtime, "imagem-nao-deve-existir.imagem", &erro) &&
                  erro.ocorreu,
              "imagem rejeita stream de arquivo aberto");
    verificar_texto(runtime, "(close fluxo-imagem)", "T");
    remove("imagem-nao-deve-existir.imagem");
    remove("teste-stream-imagem.txt");
    remove("teste-stream-sefirah.txt");

    SefValor invalido = sef_runtime_avaliar_texto(runtime, "(+ 1 desconhecido)", &erro);
    verificar(invalido == NULL && erro.ocorreu, "erro de simbolo nao vinculado");
    verificar(sef_runtime_objetos_vivos(runtime) > 0, "runtime possui objetos vivos");

    verificar_texto(runtime,
                    "(define base-da-imagem 40) "
                    "(defun usar-imagem (x) (+ base-da-imagem x)) "
                    "(define vetor-da-imagem (vector 'persistente 41 42)) "
                    "(define caractere-da-imagem #\\λ) "
                    "(define tabela-da-imagem (make-hash-table)) "
                    "(setf (gethash 'resposta tabela-da-imagem) 42 "
                    "(gethash 'dados tabela-da-imagem) (list 'persistente 42) "
                    "(gethash :self tabela-da-imagem) tabela-da-imagem) "
                    "(hash-table-count tabela-da-imagem)",
                    "3");
    verificar(sef_runtime_imagem_salvar(runtime, "teste-sefirah.imagem", &erro),
              "imagem foi salva");
    sef_runtime_destruir(runtime);
    runtime = sef_runtime_imagem_abrir("teste-sefirah.imagem", &erro);
    verificar(runtime != NULL, "imagem foi reaberta");
    if (runtime != NULL)
        verificar_texto(runtime, "(usar-imagem 2)", "42");
    if (runtime != NULL)
        verificar_texto(runtime, "(setf (aref vetor-da-imagem 1) 42) vetor-da-imagem",
                        "#(PERSISTENTE 42 42)");
    if (runtime != NULL)
        verificar_texto(runtime, "(list caractere-da-imagem (char-code caractere-da-imagem))",
                        "(#\\λ 955)");
    if (runtime != NULL)
        verificar_texto(runtime,
                        "(list (gethash 'resposta tabela-da-imagem) "
                        "(gethash 'dados tabela-da-imagem) "
                        "(eq (gethash :self tabela-da-imagem) tabela-da-imagem) "
                        "(hash-table-count tabela-da-imagem))",
                        "(42 (PERSISTENTE 42) T 3)");
    if (runtime != NULL)
        verificar_texto(runtime, "condicao-salva", "#<ERROR persistente>");
    if (runtime != NULL)
        verificar_texto(runtime, "(alpha:resposta-do-pacote)", "42");
    if (runtime != NULL)
        verificar_texto(runtime,
                        "(list (calcular-compilado 10 22) "
                        "(type-of #'calcular-compilado))",
                        "(42 FUNCTION)");
    if (runtime != NULL)
        verificar_texto(runtime,
                        "(compile 'calcular-compilado) "
                        "(list (calcular-compilado 50 8) "
                        "(compiled-function-p #'calcular-compilado))",
                        "(42 T)");
    if (runtime != NULL)
        verificar_texto(runtime,
                        "(list (streamp *standard-input*) (streamp *standard-output*) "
                        "(streamp *error-output*))",
                        "(T T T)");
    if (runtime != NULL)
        verificar_texto(runtime, "(> (sefirah::object-count) 0)", "T");
    if (runtime != NULL && argc == 2)
        verificar_texto(runtime,
                        "(list (shared-library-p biblioteca-teste) "
                        "(shared-library-open-p biblioteca-teste) "
                        "(type-of biblioteca-imagem) biblioteca-imagem)",
                        "(T NIL SEFIRAH::SHARED-LIBRARY "
                        "#<BIBLIOTECA-COMPARTILHADA FECHADA>)");
    remove("teste-sefirah.imagem");

    sef_runtime_destruir(runtime);
    if (falhas == 0)
        puts("nucleo: todos os testes passaram");
    return falhas == 0 ? 0 : 1;
}
