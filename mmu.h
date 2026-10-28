#ifndef MMU_H
#define MMU_H

#include "simulator.h"

/*
 * Integrante 1 - Arquiteto da MMU
 * Bruno Aparecido Almeida Rocha - RA: 821591
 * Responsabilidade: traducao VPN/Offset e estruturas da RAM fisica.
 */

uint32_t mmu_page_size(uint32_t vpn);
uint32_t mmu_frames_needed(uint32_t vpn);
uint32_t mmu_count_virtual_pages(void);
bool mmu_translate_address(uint32_t address, uint32_t num_pages,
                           uint32_t *vpn, uint32_t *offset);

process_t *mmu_find_process(simulator_t *sim, uint32_t pid);
process_t *mmu_get_or_create_process(simulator_t *sim, uint32_t pid);

int mmu_find_free_run(const simulator_t *sim, uint32_t need);
void mmu_release_page_frames(simulator_t *sim, process_t *p, uint32_t vpn);
bool mmu_place_page(simulator_t *sim, process_t *p, uint32_t vpn,
                    uint32_t current_t, int start_frame);
void mmu_update_page_usage(process_t *p, uint32_t vpn, uint32_t offset);
void mmu_destroy_processes(simulator_t *sim);

#endif
