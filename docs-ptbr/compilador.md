# Compilador do Sefirah Lisp

[English](../docs-en/compiler.md) · **Português do Brasil**

## Visão geral

O compilador é um módulo separado do runtime e da GUI. A camada pública em
`sefirah/compilador.h` oferece uma IR SSA tipada para inteiros de 64 bits, um
interpretador de referência, emissores x86-64/AArch64 e gravadores de objetos
desktop.

| Camada | Estado atual |
| --- | --- |
| Frontend Lisp | `DEFUN` com parâmetros i64, constantes, aritmética, comparação, `IF` e `EXTERNAL-I64` |
| IR | blocos básicos, SSA, `PHI`, fluxo de controle e chamadas externas |
| Referência | interpretador com limite de passos |
| x86-64 | System V e Microsoft x64, JIT e objetos |
| AArch64 | AAPCS64, JIT nativo quando hospedado e objetos |
| Objetos | ELF64, COFF e Mach-O para x86-64/ARM64 |

O compilador geral de Common Lisp, com valores etiquetados, alocação e pontos
seguros, ainda não está implementado. O backend atual é a fundação verificável
desse trabalho.

## IR SSA

Uma função contém blocos básicos; cada registrador tem exatamente uma definição
estática e todo fluxo termina em salto, ramificação ou retorno. As operações
atuais cobrem:

- parâmetros e constantes i64;
- `PHI`;
- soma, subtração e multiplicação;
- menor e menor-ou-igual;
- chamadas C com uma ou duas entradas i64;
- salto, ramificação e retorno.

A aritmética inteira possui semântica modular de 64 bits e não depende de
overflow indefinido de C.

Antes da execução, o verificador confirma:

- blocos terminados e alcançáveis a partir da entrada;
- alvos de salto, parâmetros e registradores existentes;
- uma única definição para cada registrador SSA;
- instruções `PHI` no início do bloco e ligadas a predecessores reais;
- dominância de toda definição sobre seus usos;
- aridade e símbolos válidos em chamadas externas.

O interpretador de referência consome a mesma IR dos backends. Os testes montam
laços, `PHI`, ramificações e chamadas C, além de exercitar rejeições do
verificador.

## Frontend Lisp i64

`COMPILE` baixa uma função Lisp compatível e instala o código nativo no próprio
objeto de função:

```lisp
(defun escolher (x y)
  (if (< x y)
      (+ (* x 2) y)
      (- x y)))

(compile 'escolher)
(escolher 10 22) ; => 42
```

Parâmetros posicionais viram `PARAMETRO`; inteiros e operações geram valores
SSA; cada `IF` cria dois caminhos e um bloco de união com `PHI`. Chamadas
normais, `FUNCALL` e `APPLY` observam o cache instalado.

A definição Lisp permanece disponível. Imagens salvam essa definição portátil
e descartam bytes associados à arquitetura ou ao processo antigo.

## Backend x86-64

O emissor cobre a IR atual nas ABIs System V e Microsoft x64. Registradores SSA
usam slots de pilha durante o bootstrap, separando correção de alocação de
registradores. O backend:

- recebe os parâmetros por um vetor com contrato estável;
- resolve saltos `rel32`;
- cria cópias paralelas nas arestas de `PHI`;
- prepara shadow space na ABI Microsoft;
- emite chamadas externas nos registradores corretos de cada ABI.

`sef_codigo_nativo_preparar` copia os bytes para páginas graváveis e depois as
torna somente legíveis/executáveis. Linux/macOS usam `mmap`/`mprotect`; Windows
usa `VirtualAlloc`/`VirtualProtect`. Nenhuma página permanece simultaneamente
gravável e executável.

## Backend AArch64

O emissor AArch64 usa AAPCS64 em Linux ARM64, Apple Silicon e Windows ARM64. O
vetor de argumentos chega em `x0`; o resultado retorna em `x0`; `x9` preserva o
vetor e `x0`/`x1` atendem às operações e chamadas externas.

Constantes são materializadas com `MOVZ`/`MOVK`; saltos usam `B` de 26 bits e
ramificações usam `CBZ` de 19 bits. Valores SSA e temporários de `PHI` ocupam
slots alinhados na pilha.

Os testes comparam palavras de máquina centrais e verificam os formatos de
objeto independentemente do hospedeiro. Em ARM64, a mesma suíte prepara páginas
W^X e executa o código diretamente.

## Objetos relocáveis

Uma função compatível pode ser gravada nos três formatos desktop:

```bash
sefirah compilar-elf exemplos/nativo.lisp calcular_nativo calcular.o
sefirah compilar-coff exemplos/nativo.lisp calcular_nativo calcular.obj
sefirah compilar-macho exemplos/nativo.lisp calcular_nativo calcular-macos.o
```

| Formato | Arquiteturas | Relocações externas |
| --- | --- | --- |
| ELF64 `ET_REL` | x86-64, AArch64 | `R_X86_64_PLT32`, `R_AARCH64_CALL26` |
| COFF | AMD64, ARM64 | `IMAGE_REL_AMD64_REL32`, `IMAGE_REL_ARM64_BRANCH26` |
| Mach-O `MH_OBJECT` | x86-64, ARM64 | relocação de branch externa do Darwin |

### ELF64

O objeto contém `.text`, `.symtab`, `.strtab` e `.shstrtab`. Chamadas externas
acrescentam `.rela.text` e símbolos globais indefinidos. O `e_machine` é
`EM_X86_64` ou `EM_AARCH64`.

Como ELF x86-64 exige System V, um frontend executado no Windows preserva a IR
e reemite o alvo correto em vez de copiar bytes da ABI Microsoft.

### COFF

O objeto contém `.text`, tabela de símbolos, registro auxiliar de seção e
string table. x86-64 é reemitido com a ABI Microsoft. O arquivo é entrada para
o linker do Windows, não um executável PE completo.

### Mach-O

O objeto contém `LC_SEGMENT_64`, `__TEXT,__text`, `LC_SYMTAB` e prefixa símbolos
C com `_`, conforme o Darwin. x86-64 usa System V; AArch64 usa AAPCS64.

## Chamadas externas i64

No Lisp, `EXTERNAL-I64` aceita o nome C e uma ou duas entradas:

```lisp
(defun chamar-dobro (valor)
  (external-i64 "dobrar_i64" valor))

(defun combinar (a b)
  (external-i64 "combinar_i64" a b))
```

No SDK, `sef_funcao_ir_adicionar_externa_i64` e a variante `_binaria`
registram nome e endereço. `SEF_IR_CHAMAR_EXTERNA_I64` produz outro registrador
SSA. Os comandos de objeto preservam a relocação para o linker.

Para execução no processo, a vinculação associa o endereço a todas as
relocações daquele símbolo. A preparação W^X acrescenta um trampolim por
chamada:

- x86-64: `call rel32` para `mov rax, endereço; jmp rax`;
- AArch64: `BL` para `ldr x16, literal; br x16`.

Assim, o endereço externo não precisa estar no alcance relativo da página JIT
e os bytes originais continuam disponíveis aos gravadores.

`COMPILE-EXTERNAL-I64` resolve nomes por `LoadLibrary`/`GetProcAddress` no
Windows ou `dlopen`/`dlsym` em Linux/macOS. O objeto de biblioteca e cada função
compilada mantêm referências independentes ao recurso, impedindo o
descarregamento enquanto um trampolim ainda puder chamá-lo.

## Validação

A suíte automatizada verifica:

- interpretador e JIT nos dois caminhos de ramificações e em laços;
- chamadas externas unárias e binárias;
- emissão x86-64 System V/Microsoft e AArch64;
- cabeçalhos, símbolos e relocações ELF/COFF/Mach-O;
- execução do backend Windows x64 via Wine;
- política W^X e erros de símbolos não vinculados.

Os próximos níveis do compilador estão descritos no
[roteiro para 1.0](roteiro.md).
