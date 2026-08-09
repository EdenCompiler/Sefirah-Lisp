# Arquitetura inicial

O codigo esta dividido por responsabilidade, e nao por sistema operacional:

- `nucleo` possui objetos, ambientes, leitor, avaliador, primitivas, impressor
  e ciclo de vida. Ele nao conhece janelas.
- `graficos` possui uma superficie RGB, primitivas raster e fonte bitmap. Ele
  nao conhece X11, Win32 ou Cocoa. A camada `gui` acrescenta uma arvore de
  componentes própria com layout flexível, temas, hit-testing, foco e ações.
- `plataforma` apresenta a mesma superficie em uma janela nativa. Cada build
  compila exatamente um backend.
- `cli` conecta comandos publicos ao runtime e a primeira composicao da IDE.
- `compilador` possui a IR SSA, seu verificador de fluxo/dominancia e um
  interpretador de referencia independente do runtime, além do backend
  x86-64 para as ABIs System V/Microsoft, do backend AArch64 AAPCS64 e dos
  gravadores relocaveis ELF64, COFF e Mach-O 64-bit.

O backend macOS permanece C puro. A integracao usa CoreGraphics e chamadas
tipadas ao runtime Objective-C; essa complexidade fica confinada ao adaptador.

O compilador self-hosted entrara depois do bootstrap interpretado. O contrato
ja separa a primeira IR independente de maquina; os proximos niveis sao IR de
baixo nivel, ABI e emissor de objeto, evitando espalhar detalhes de
PE/ELF/Mach-O pelo runtime.

Valores entregues a codigo C podem ser preservados entre coletas por `SefRaiz`.
O handle e explicito e removivel, evitando depender de varredura conservadora
da pilha do hospedeiro.

Streams sao objetos do heap. Streams padrão encapsulam `stdin`, `stdout` e
`stderr` sem possuir esses recursos; streams retornados por `OPEN` possuem seu
`FILE` e o fecham no `CLOSE`, na destruicao ou na coleta. A imagem serializa os
streams padrão e os fechados, mas rejeita descritores de arquivo ainda abertos.
