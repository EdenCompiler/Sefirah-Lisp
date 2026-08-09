# Sefirah Lisp

Sefirah e uma implementacao de Common Lisp em construcao, inspirada no
Interlisp e nas Lisp Machines. O objetivo e unir um ambiente de programacao
vivo a uma plataforma legivel para aplicativos desktop em Windows, Linux e
macOS.

O marco atual e o **bootstrap executavel**: runtime C17, REPL Lisp, avaliacao
lexical, coleta de lixo explicita, compilador i64 x86-64/AArch64 com objetos
ELF/COFF/Mach-O, chamadas externas relocaveis e o primeiro rasterizador/janela
da futura IDE. Ele ainda nao e uma implementacao ANSI Common Lisp completa.

## Compilar

```sh
cmake -S . -B construir -DCMAKE_BUILD_TYPE=Debug
cmake --build construir
ctest --test-dir construir --output-on-failure
cmake --install construir --prefix instalar
```

A instalacao inclui o executavel, as bibliotecas modulares e os cabeçalhos do
SDK C17 (`runtime.h`, `compilador.h`, `graficos.h`, `gui.h` e `janela.h`).

## Experimentar

```sh
./construir/sefirah repl
./construir/sefirah avaliar "(+ 20 22)"
./construir/sefirah executar exemplos/inicio.lisp
./construir/sefirah compilar-elf exemplos/nativo.lisp calcular_nativo calcular.o
./construir/sefirah compilar-coff exemplos/nativo.lisp calcular_nativo calcular.obj
./construir/sefirah compilar-macho exemplos/nativo.lisp calcular_nativo calcular-macos.o
./construir/sefirah ide
```

O codigo interno, os comentarios e a documentacao sao escritos em PT-BR. Os
simbolos publicos da linguagem permanecem em ingles para convergir com ANSI
Common Lisp.

Leia [docs/manual.md](docs/manual.md) para o subconjunto implementado e
[docs/arquitetura.md](docs/arquitetura.md) para os limites entre os modulos.
O desenho do compilador fica em [docs/compilador.md](docs/compilador.md).
O estado exato da caminhada ate 1.0 fica em [docs/roteiro.md](docs/roteiro.md).
