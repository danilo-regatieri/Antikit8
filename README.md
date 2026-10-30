# Antikit-8 — Gerenciamento de Memória

Trabalho prático da disciplina de Sistemas Operacionais.

O projeto implementa, em C, um simulador de gerenciamento de memória para o hardware Antikit-8, incluindo paginação assimétrica, tratamento de Page Faults, substituição de páginas com WSClock, controle térmico do Swap, auditoria por checksum e análise de fragmentação interna.

## Integrantes

- Bruno Aparecido Almeida Rocha — RA 821591
- Danilo Mitsunari Regatieri — RA 831830
- Isadora Berbel Kamada Ribeiro — RA 844978

## Divisão de trabalho

### Bruno Aparecido Almeida Rocha — Arquiteto da MMU

Arquivos principais:

- `mmu.c`
- `mmu.h`

Responsável por:

- tradução de endereços virtuais;
- cálculo de VPN e Offset;
- tratamento das páginas assimétricas de 4 KB e 8 KB;
- gerenciamento das estruturas da RAM física;
- busca, reserva e liberação de quadros.

### Danilo Mitsunari Regatieri — Engenheiro de Algoritmos

Arquivos principais:

- `wsclock.c`
- `wsclock.h`

Responsável por:

- algoritmo de substituição baseado em WSClock;
- tratamento de Page Faults;
- segunda chance utilizando o bit R;
- preferência por páginas limpas;
- Page Out de páginas modificadas;
- controle térmico do dispositivo de Swap.

### Isadora Berbel Kamada Ribeiro — Auditor e Analista de Métricas

Arquivos principais:

- `auditoria.c`
- `auditoria.h`

Responsável por:

- parser do arquivo de trace;
- auditoria por checksum;
- detecção do Fading do bit R;
- mapa ASCII da memória;
- cálculo da fragmentação interna;
- consolidação das métricas finais.

## Estrutura do projeto

```text
Antikit8/
├── docs/
│   └── Relatorio_Tecnico_Antikit8.pdf
├── traces/
│   ├── teste1_limites.txt
│   ├── teste2_wsclock.txt
│   ├── teste2_dirty_stress.txt
│   ├── teste3_fading.txt
│   └── teste4_fragmentacao.txt
├── main.c
├── simulator.h
├── mmu.c
├── mmu.h
├── wsclock.c
├── wsclock.h
├── auditoria.c
├── auditoria.h
├── hardware_specs.c
├── hardware_specs.h
├── memory_trace.txt
├── Makefile
├── DIVISAO_TRABALHO.md
└── README.md
```

Os arquivos `hardware_specs.c` e `hardware_specs.h` foram fornecidos pelo professor e permanecem inalterados.

## Compilação

Em Linux:

```bash
make
```

Ou diretamente com GCC:

```bash
gcc -Wall -Wextra -Wpedantic -std=c11 main.c mmu.c wsclock.c auditoria.c hardware_specs.c -o antikit8
```

## Execução

Execução principal:

```bash
./antikit8 memory_trace.txt metrics.csv
```

No Windows:

```powershell
.\antikit8.exe memory_trace.txt metrics.csv
```

## Testes de validação

### Teste 1 — Tradução assimétrica

```bash
./antikit8 traces/teste1_limites.txt resultados/teste1.csv
```

Resultados esperados:

```text
12287   -> VPN 1, offset 8191
12288   -> VPN 2, offset 0
1048575 -> VPN 170, offset 4095
```

### Teste 2 — WSClock com páginas limpas

```bash
./antikit8 traces/teste2_wsclock.txt resultados/teste2_wsclock.csv
```

### Teste 2 — Estresse térmico com páginas modificadas

```bash
./antikit8 traces/teste2_dirty_stress.txt resultados/teste2_dirty_stress.csv
```

Nesse cenário, o algoritmo escalona os Page Outs para permitir o resfriamento do Swap e evitar o Thermal Throttling.

### Teste 3 — Fading do bit R e auditoria

```bash
./antikit8 traces/teste3_fading.txt resultados/teste3_fading.csv
```

O log mostra os eventos de Fading do bit R e, imediatamente depois, o checksum e a verificação de consistência.

### Teste 4 — Fragmentação interna

```bash
./antikit8 traces/teste4_fragmentacao.txt resultados/teste4_fragmentacao.csv
```

O mapa ASCII utiliza:

- `U` — memória utilizada;
- `F` — fragmentação interna;
- `.` — região livre da RAM.

## Resultados principais

A validação confirmou:

- tradução correta dos limites das páginas assimétricas;
- funcionamento do WSClock;
- preferência por páginas limpas;
- controle térmico do Swap;
- detecção do Fading do bit R;
- auditoria com checksum;
- consistência entre tabela de páginas e RAM;
- visualização da fragmentação interna.

No teste térmico com páginas modificadas, a temperatura máxima atingida foi de 65,0 °C sem ultrapassar o limite e sem ativar o Thermal Throttling.

## Relatório técnico

O relatório completo está disponível em:

```text
docs/Relatorio_Tecnico_Antikit8.pdf
```

## Versão entregue

A versão utilizada para a entrega está identificada por uma tag no GitHub.

```text
v1.1-entrega-final
```

Repositório:

```text
https://github.com/danilo-regatieri/Antikit8
```