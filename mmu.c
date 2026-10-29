#include "mmu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint32_t mmu_page_size(uint32_t vpn) {
    return (vpn % 2u == 0u) ? PAGE_SIZE_EVEN : PAGE_SIZE_ODD;
}

uint32_t mmu_frames_needed(uint32_t vpn) {
    return mmu_page_size(vpn) / BASE_FRAME_SIZE;
}

uint32_t mmu_count_virtual_pages(void) {
    uint32_t total = 0;
    uint32_t vpn = 0;

    while (total < VIRTUAL_MEMORY_SIZE) {
        total += mmu_page_size(vpn);
        vpn++;
    }

    return vpn;
}

bool mmu_translate_address(uint32_t address,
                           uint32_t num_pages,
                           uint32_t *vpn,
                           uint32_t *offset) {
    if (address >= VIRTUAL_MEMORY_SIZE || vpn == NULL || offset == NULL) {
        return false;
    }

    const uint32_t pair_span = PAGE_SIZE_EVEN + PAGE_SIZE_ODD;
    uint32_t pair = address / pair_span;
    uint32_t inside_pair = address % pair_span;

    if (inside_pair < PAGE_SIZE_EVEN) {
        *vpn = pair * 2u;
        *offset = inside_pair;
    } else {
        *vpn = pair * 2u + 1u;
        *offset = inside_pair - PAGE_SIZE_EVEN;
    }

    return *vpn < num_pages;
}

process_t *mmu_find_process(simulator_t *sim, uint32_t pid) {
    for (size_t i = 0; i < sim->process_count; i++) {
        if (sim->processes[i].active && sim->processes[i].pid == pid) {
            return &sim->processes[i];
        }
    }
    return NULL;
}

process_t *mmu_get_or_create_process(simulator_t *sim, uint32_t pid) {
    process_t *p = mmu_find_process(sim, pid);
    if (p != NULL) {
        return p;
    }

    if (sim->process_count >= MAX_PROCESSES) {
        fprintf(stderr, "Erro: limite de %d processos atingido.\n", MAX_PROCESSES);
        return NULL;
    }

    p = &sim->processes[sim->process_count++];
    memset(p, 0, sizeof(*p));
    p->active = true;
    p->pid = pid;
    p->page_table = calloc(sim->num_pages, sizeof(page_table_entry_t));
    p->used_bytes = calloc(sim->num_pages, sizeof(uint32_t));

    if (p->page_table == NULL || p->used_bytes == NULL) {
        free(p->page_table);
        free(p->used_bytes);
        p->page_table = NULL;
        p->used_bytes = NULL;
        p->active = false;
        fprintf(stderr, "Erro: falha de memoria ao criar processo %u.\n", pid);
        return NULL;
    }

    printf("[SO] Processo %u criado com %u entradas na tabela de paginas.\n",
           pid, sim->num_pages);
    return p;
}

int mmu_find_free_run(const simulator_t *sim, uint32_t need) {
    uint32_t run = 0;
    uint32_t start = 0;

    for (uint32_t i = 0; i < NUM_FRAMES; i++) {
        if (!sim->ram[i].occupied) {
            if (run == 0) start = i;
            run++;
            if (run >= need) return (int)start;
        } else {
            run = 0;
        }
    }

    return -1;
}

void mmu_release_page_frames(simulator_t *sim, process_t *p, uint32_t vpn) {
    page_table_entry_t *e = &p->page_table[vpn];
    uint32_t start = e->frame_number;
    uint32_t need = mmu_frames_needed(vpn);

    for (uint32_t i = 0; i < need && start + i < NUM_FRAMES; i++) {
        memset(&sim->ram[start + i], 0, sizeof(sim->ram[start + i]));
    }
}

bool mmu_place_page(simulator_t *sim,
                    process_t *p,
                    uint32_t vpn,
                    uint32_t current_t,
                    int start_frame) {
    if (start_frame < 0) return false;

    uint32_t need = mmu_frames_needed(vpn);
    if ((uint32_t)start_frame + need > NUM_FRAMES) return false;

    for (uint32_t i = 0; i < need; i++) {
        ram_frame_t *frame = &sim->ram[(uint32_t)start_frame + i];
        if (frame->occupied) return false;
    }

    for (uint32_t i = 0; i < need; i++) {
        ram_frame_t *frame = &sim->ram[(uint32_t)start_frame + i];
        frame->occupied = true;
        frame->page_start = (i == 0u);
        frame->pid = p->pid;
        frame->vpn = vpn;
    }

    page_table_entry_t *e = &p->page_table[vpn];
    e->frame_number = (uint32_t)start_frame;
    e->present = true;
    e->referenced = false;
    e->modified = false;
    e->last_access_t = current_t;

    printf("[RAM] mapeada pid=%u vpn=%u (%u KB) em frame-base %d%s.\n",
           p->pid, vpn, mmu_page_size(vpn) / 1024u, start_frame,
           need == 2u ? "-dois quadros contiguos" : "");
    return true;
}

void mmu_update_page_usage(process_t *p, uint32_t vpn, uint32_t offset) {
    uint32_t touched = offset + 1u;
    if (touched > p->used_bytes[vpn]) {
        p->used_bytes[vpn] = touched;
    }
}

void mmu_destroy_processes(simulator_t *sim) {
    for (size_t i = 0; i < sim->process_count; i++) {
        free(sim->processes[i].page_table);
        free(sim->processes[i].used_bytes);
        sim->processes[i].page_table = NULL;
        sim->processes[i].used_bytes = NULL;
    }
}
