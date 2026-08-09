# Compilador

O compilador e um modulo separado do runtime e da GUI. Sua primeira camada,
declarada em `sefirah/compilador.h`, e uma IR SSA tipada para inteiros de 64
bits. Uma funcao contem blocos basicos; cada registrador tem exatamente uma
definicao estatica e o fluxo termina em salto, ramificacao ou retorno.

As operacoes iniciais cobrem parametros, constantes, `PHI`, soma, subtracao,
multiplicacao, comparacoes, chamada externa unaria, saltos e retorno. A
aritmetica inteira possui semantica modular de 64 bits e nao depende de
overflow indefinido de C.

Antes de executar uma funcao, o verificador confirma:

- blocos terminados e alcançaveis a partir da entrada;
- alvos de salto, parametros e registradores existentes;
- uma unica definicao para cada registrador SSA;
- `PHI` no inicio do bloco e ligado a predecessores reais;
- dominancia de toda definicao sobre seus usos.

O interpretador de referencia executa a mesma IR consumida pelos backends
nativos. Ele tem limite de passos para que uma IR defeituosa nao prenda
ferramentas de compilacao. O teste do compilador monta um fatorial com quatro
blocos, dois `PHI` e uma aresta de retorno, além de exercitar rejeicoes do
verificador.

## Backend x86-64

O primeiro backend nativo cobre toda a IR i64 atual nas ABIs System V e
Microsoft x64. Registradores SSA usam slots de pilha nesta versao inicial,
permitindo separar corretude de alocacao de registradores. O emissor resolve
saltos `rel32`, cria copias paralelas nas arestas de `PHI` e recebe parametros
por um vetor com contrato identico nas duas ABIs.

`sef_codigo_nativo_preparar` copia os bytes para paginas inicialmente
gravaveis e depois somente legiveis/executaveis. Linux/macOS usam
`mmap`/`mprotect`; Windows usa `VirtualAlloc`/`VirtualProtect`. Assim, nenhuma
pagina permanece simultaneamente gravavel e executavel. O teste compara o
interpretador e o codigo nativo em laços, nos dois lados de ramificacoes e nos
casos base. A ABI Windows também e compilada e executada na verificacao
cruzada do projeto.

## Backend AArch64

O segundo backend cobre a mesma IR i64 e usa o contrato AAPCS64 compartilhado
por Linux ARM64, Apple Silicon e Windows ARM64: o vetor de argumentos chega em
`x0` e o resultado retorna em `x0`. Valores SSA e temporarios de `PHI` usam
slots alinhados na pilha; `x9` preserva o ponteiro dos argumentos e `x0`/`x1`
sao registradores de trabalho.

O emissor materializa constantes de 64 bits com `MOVZ`/`MOVK`, resolve saltos
`B` de 26 bits e ramificacoes `CBZ` de 19 bits, e gera prologo/epilogo AAPCS64.
As palavras de maquina centrais sao comparadas nos testes e foram conferidas
contra o assembler AArch64 independente do Clang. Em um hospedeiro ARM64, os
mesmos testes preparam paginas W^X e executam fatorial e ambos os caminhos de
uma ramificacao nativamente.

## Frontend Lisp i64

O modulo de runtime baixa `DEFUN` reais para a IR quando o programa chama
`COMPILE`. Parametros posicionais viram instrucoes `PARAMETRO`; inteiros e as
operacoes `+`, `-`, `*`, `<` e `<=` geram valores SSA; cada `IF` cria dois
caminhos e um bloco de uniao com `PHI`. A funcao continua guardando seu corpo
Lisp, mas chamadas normais passam a executar o codigo nativo instalado.

O cache nativo possui ciclo de vida ligado ao objeto de funcao e e liberado
pelo GC. Imagens salvam somente a definicao portavel e recompilam sob demanda,
evitando gravar bytes associados a uma arquitetura ou processo antigo.

## Objetos ELF64

`sef_codigo_nativo_gravar_elf` encapsula codigo autocontido em um objeto
`ET_REL` little-endian. O arquivo contém `.text` executavel, tabela de simbolos
com uma funcao global, tabela de strings e nomes de secoes. O `e_machine` e
`EM_X86_64` ou `EM_AARCH64`. Chamadas externas acrescentam `.rela.text`, um
simbolo global indefinido e relocacoes `R_X86_64_PLT32` ou
`R_AARCH64_CALL26`.

O comando `compilar-elf` liga leitor, frontend, IR, backend e gravador. Sua
saida foi validada por `readelf`, pelo linker C do sistema e por execucao. O
mesmo fluxo foi executado no binario Windows via Wine: como ELF x86-64 exige
System V, o frontend preserva a IR e reemite o alvo correto em vez de copiar
bytes da ABI Microsoft.

## Objetos COFF

`sef_codigo_nativo_gravar_coff` cria um objeto relocavel para AMD64 ou ARM64
com uma secao `.text`, simbolo de secao com registro auxiliar e uma funcao
global. Chamadas externas usam simbolos indefinidos e relocações
`IMAGE_REL_AMD64_REL32` ou `IMAGE_REL_ARM64_BRANCH26`. O backend x86-64 e
reemitido com a ABI Microsoft antes da gravacao. O comando `compilar-coff` foi
validado por `objdump`, ligado com MinGW e executado no Wine; o programa C de
exemplo recebeu o resultado 42 do codigo Sefirah.

O arquivo e COFF, entrada nativa do linker do Windows, e nao um executavel PE
completo. A criacao do `.exe`, import table e metadados de distribuicao continua
sendo responsabilidade do linker e do empacotador.

## Objetos Mach-O 64-bit

`sef_codigo_nativo_gravar_macho` cria `MH_OBJECT` para x86-64 e ARM64. O objeto
possui `LC_SEGMENT_64`, a secao `__TEXT,__text`, `LC_SYMTAB` e uma funcao externa
com o prefixo `_` usado por simbolos C no Darwin. Codigo x86-64 usa System V;
AArch64 usa o mesmo contrato AAPCS64 dos outros sistemas. Relocacoes externas
de branch referenciam entradas indefinidas da tabela de simbolos.

O comando `compilar-macho` fecha o caminho desde um arquivo `.lisp`. A saida
x86-64 foi reconhecida pelas ferramentas de arquivo e consumida pelo linker
Darwin do LLVM para formar um executavel Mach-O. Os testes também verificam o
cabeçalho ARM64 independentemente do hospedeiro.

## Chamadas externas i64

`sef_funcao_ir_adicionar_externa_i64` registra nome e, opcionalmente, um
endereco hospedeiro. `SEF_IR_CHAMAR_EXTERNA_I64` consome um registrador, chama o
contrato C `int64_t externa(int64_t)` e produz outro registrador SSA. O
interpretador de referencia usa o endereco fornecido; os backends geram `call
rel32` ou `BL` e guardam simbolo, deslocamento e tipo em `SefCodigoNativo`.

O exemplo `exemplo_gerar_objeto_externo` gera ELF, COFF ou Mach-O, tanto x64
quanto ARM64. As saidas x64 foram ligadas e executadas em Linux e Windows; os
dois Mach-O foram consumidos pelo linker Darwin do LLVM, e ELF/COFF ARM64 foram
reconhecidos pelas ferramentas de formato. O JIT deliberadamente recusa essas
relocacoes enquanto nao houver um resolvedor dinamico com trampolins seguros.

Os proximos niveis acrescentarao valores Lisp etiquetados, chamadas, alocacao,
pontos seguros e metadados de excecao. Depois entram alocacao de registradores,
imports no frontend Lisp e resolucao dinamica para o JIT.
