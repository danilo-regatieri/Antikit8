# Divisão de Trabalho Obrigatória

A organização dos arquivos foi feita para refletir diretamente a divisão exigida pelo enunciado.

## Integrante 1 - Arquiteto da MMU
**Bruno Aparecido Almeida Rocha - RA: 821591**

Arquivos principais: `mmu.c` e `mmu.h`.

Responsabilidades:
- tradução de endereços virtuais em VPN e offset;
- tratamento das páginas assimétricas de 4 KB e 8 KB;
- criação das tabelas de páginas por processo;
- gerenciamento dos quadros-base da RAM física;
- reserva/liberação de quadros contíguos;
- atualização do uso de cada página.

## Integrante 2 - Engenheiro de Algoritmos
**Danilo Mitsunari Regatieri - RA: 831830**

Arquivos principais: `wsclock.c` e `wsclock.h`.

Responsabilidades:
- seleção de vítima pelo WSClock;
- segunda chance baseada no bit R;
- preferência por páginas limpas;
- tratamento de páginas sujas;
- Page Out;
- controle/escalonamento térmico do Swap.

## Integrante 3 - Auditor e Analista de Métricas
**Isadora Berbel Kamada Ribeiro - RA: 844978**

Arquivos principais: `auditoria.c` e `auditoria.h`.

Responsabilidades:
- parser do arquivo `memory_trace.txt`;
- checksum da tabela de páginas;
- verificação de consistência tabela/RAM;
- detecção/log do fading do bit R;
- mapa ASCII de fragmentação;
- cálculo da fragmentação interna;
- CSV e estatísticas finais.

