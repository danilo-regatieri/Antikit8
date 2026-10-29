#ifndef WSCLOCK_H
#define WSCLOCK_H

#include "simulator.h"

/*
 * Integrante 2 - Engenheiro de Algoritmos
 * Danilo Mitsunari Regatieri - RA: 831830
 * Responsabilidade: WSClock, substituicao de paginas e controle termico do Swap.
 */

bool wsclock_map_page(simulator_t *sim, process_t *p,
                      uint32_t vpn, uint32_t current_t);

#endif
