#include "auditoria.h"
#include "mmu.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t mix64(uint64_t h, uint64_t value) {
    h ^= value + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    return h;
}

uint64_t auditoria_checksum_mapping(const simulator_t *sim) {
    uint64_t h = 0xcbf29ce484222325ULL;

    for (size_t i = 0; i < sim->process_count; i++) {
        const process_t *p = &sim->processes[i];
        if (!p->active) continue;

        h = mix64(h, p->pid);
        for (uint32_t vpn = 0; vpn < sim->num_pages; vpn++) {
            const page_table_entry_t *e = &p->page_table[vpn];
            uint64_t value = ((uint64_t)e->present << 63) |
                             ((uint64_t)e->frame_number << 16) |
                             (uint64_t)vpn;
            h = mix64(h, value);
        }
    }

    return h;
}

static uint64_t checksum_full_state(const simulator_t *sim) {
    uint64_t h = 0x84222325cbf29ce4ULL;

    for (size_t i = 0; i < sim->process_count; i++) {
        const process_t *p = &sim->processes[i];
        if (!p->active) continue;

        h = mix64(h, p->pid);
        for (uint32_t vpn = 0; vpn < sim->num_pages; vpn++) {
            const page_table_entry_t *e = &p->page_table[vpn];
            uint64_t bits = ((uint64_t)e->present << 3) |
                            ((uint64_t)e->referenced << 2) |
                            ((uint64_t)e->modified << 1);
            uint64_t value = ((uint64_t)e->frame_number << 32) |
                             ((uint64_t)e->last_access_t << 8) |
                             bits;
            h = mix64(h, value ^ vpn);
        }
    }

    return h;
}

bool auditoria_verify_consistency(const simulator_t *sim) {
    for (size_t i = 0; i < sim->process_count; i++) {
        const process_t *p = &sim->processes[i];
        if (!p->active) continue;

        for (uint32_t vpn = 0; vpn < sim->num_pages; vpn++) {
            const page_table_entry_t *e = &p->page_table[vpn];
            if (!e->present) continue;

            uint32_t start = e->frame_number;
            uint32_t need = mmu_frames_needed(vpn);
            if (start + need > NUM_FRAMES) return false;

            for (uint32_t j = 0; j < need; j++) {
                const ram_frame_t *f = &sim->ram[start + j];
                if (!f->occupied || f->pid != p->pid || f->vpn != vpn) {
                    return false;
                }
                if ((j == 0u) != f->page_start) return false;
            }
        }
    }

    for (uint32_t f = 0; f < NUM_FRAMES; f++) {
        if (!sim->ram[f].occupied || !sim->ram[f].page_start) continue;

        const process_t *p = NULL;
        for (size_t i = 0; i < sim->process_count; i++) {
            if (sim->processes[i].active && sim->processes[i].pid == sim->ram[f].pid) {
                p = &sim->processes[i];
                break;
            }
        }
        if (p == NULL) return false;

        uint32_t vpn = sim->ram[f].vpn;
        if (vpn >= sim->num_pages) return false;
        const page_table_entry_t *e = &p->page_table[vpn];
        if (!e->present || e->frame_number != f) return false;
    }

    return true;
}

void auditoria_audit_state(const simulator_t *sim, const char *reason) {
    printf("[AUDITORIA] %-18s map=%016" PRIx64
           " estado=%016" PRIx64 " consistencia=%s\n",
           reason,
           auditoria_checksum_mapping(sim),
           checksum_full_state(sim),
           auditoria_verify_consistency(sim) ? "OK" : "FALHA");
}

double auditoria_fragmentation_percent(const simulator_t *sim,
                                       uint64_t *allocated_out,
                                       uint64_t *waste_out) {
    uint64_t allocated = 0;
    uint64_t used = 0;

    for (size_t i = 0; i < sim->process_count; i++) {
        const process_t *p = &sim->processes[i];
        if (!p->active) continue;

        for (uint32_t vpn = 0; vpn < sim->num_pages; vpn++) {
            const page_table_entry_t *e = &p->page_table[vpn];
            if (!e->present) continue;

            uint32_t size = mmu_page_size(vpn);
            uint32_t page_used = p->used_bytes[vpn];
            if (page_used > size) page_used = size;
            allocated += size;
            used += page_used;
        }
    }

    uint64_t waste = allocated - used;
    if (allocated_out != NULL) *allocated_out = allocated;
    if (waste_out != NULL) *waste_out = waste;

    if (allocated == 0) return 0.0;
    return 100.0 * (double)waste / (double)allocated;
}

static void print_page_usage_bar(const process_t *p, uint32_t vpn) {
    uint32_t size_kb = mmu_page_size(vpn) / 1024u;
    uint32_t used = p->used_bytes[vpn];
    uint32_t used_kb = (used + 1023u) / 1024u;
    if (used_kb > size_kb) used_kb = size_kb;

    printf("  pid=%u vpn=%u [", p->pid, vpn);
    for (uint32_t i = 0; i < size_kb; i++) {
        putchar(i < used_kb ? 'U' : 'F');
    }
    printf("] usado=%uB pagina=%uKB\n", used, size_kb);
}

void auditoria_print_fragmentation_map(const simulator_t *sim) {
    char map[MAP_KB_UNITS + 1u];
    memset(map, '.', MAP_KB_UNITS);
    map[MAP_KB_UNITS] = '\0';

    for (size_t i = 0; i < sim->process_count; i++) {
        const process_t *p = &sim->processes[i];
        if (!p->active) continue;

        for (uint32_t vpn = 0; vpn < sim->num_pages; vpn++) {
            const page_table_entry_t *e = &p->page_table[vpn];
            if (!e->present) continue;

            uint32_t start_kb = e->frame_number * (BASE_FRAME_SIZE / 1024u);
            uint32_t size_kb = mmu_page_size(vpn) / 1024u;
            uint32_t used_kb = (p->used_bytes[vpn] + 1023u) / 1024u;
            if (used_kb > size_kb) used_kb = size_kb;

            for (uint32_t k = 0; k < size_kb && start_kb + k < MAP_KB_UNITS; k++) {
                map[start_kb + k] = (k < used_kb) ? 'U' : 'F';
            }
        }
    }

    uint64_t allocated = 0;
    uint64_t waste = 0;
    double frag = auditoria_fragmentation_percent(sim, &allocated, &waste);

    printf("\n[MAPA ASCII] U=usado F=fragmentacao interna .=RAM livre\n");
    for (uint32_t i = 0; i < MAP_KB_UNITS; i += 64u) {
        printf("  %03uKB: ", i);
        for (uint32_t j = 0; j < 64u && i + j < MAP_KB_UNITS; j++) {
            putchar(map[i + j]);
        }
        putchar('\n');
    }
    printf("  Fragmentacao atual: %.2f%% (%" PRIu64 " bytes desperdicados / "
           "%" PRIu64 " bytes alocados)\n", frag, waste, allocated);

    printf("  Paginas residentes:\n");
    for (size_t i = 0; i < sim->process_count; i++) {
        const process_t *p = &sim->processes[i];
        if (!p->active) continue;
        for (uint32_t vpn = 0; vpn < sim->num_pages; vpn++) {
            if (p->page_table[vpn].present) print_page_usage_bar(p, vpn);
        }
    }
    putchar('\n');
}

void auditoria_write_metrics_row(simulator_t *sim, uint32_t timestamp) {
    if (sim->metrics_csv == NULL) return;

    double frag = auditoria_fragmentation_percent(sim, NULL, NULL);
    fprintf(sim->metrics_csv,
            "%" PRIu64 ",%u,%.2f,%.4f,%" PRIu64 ",%" PRIu64 "\n",
            sim->trace_accesses,
            timestamp,
            sim->hw.swap_temperature,
            frag,
            sim->total_page_faults,
            sim->io_cycles);
}

void auditoria_log_fading_event(simulator_t *sim,
                                process_t *p,
                                const bool *before_ref) {
    bool found = false;

    for (uint32_t vpn = 0; vpn < sim->num_pages; vpn++) {
        if (before_ref[vpn] && !p->page_table[vpn].referenced) {
            printf("[FADING] acesso_global=%" PRIu64
                   " -> MMU zerou R de pid=%u vpn=%u.\n",
                   sim->hw.global_access_count, p->pid, vpn);
            found = true;
        }
    }

    if (!found) {
        printf("[FADING] acesso_global=%" PRIu64
               " -> evento eletrico ocorreu; a pagina sorteada nao estava presente "
               "ou ja tinha R=0.\n",
               sim->hw.global_access_count);
    }

    auditoria_audit_state(sim, "fading do bit R");
}

static bool is_op_token(const char *s) {
    return s != NULL && s[0] != '\0' && s[1] == '\0' &&
           (s[0] == 'R' || s[0] == 'r' || s[0] == 'W' || s[0] == 'w');
}

static bool parse_u32(const char *s, uint32_t *out) {
    if (s == NULL || *s == '\0') return false;

    char *end = NULL;
    unsigned long value = strtoul(s, &end, 0);
    if (*end != '\0' || value > UINT32_MAX) return false;
    *out = (uint32_t)value;
    return true;
}

int auditoria_parse_trace_line(char *line, trace_entry_t *entry) {
    char *comment = strchr(line, '#');
    if (comment != NULL) *comment = '\0';

    for (char *c = line; *c != '\0'; c++) {
        if (*c == ',' || *c == ';' || *c == '\t') *c = ' ';
    }

    char *tokens[5] = {0};
    int count = 0;
    char *tok = strtok(line, " \r\n");
    while (tok != NULL && count < 5) {
        tokens[count++] = tok;
        tok = strtok(NULL, " \r\n");
    }

    if (count == 0) return 0;
    if (count != 4) return -1;

    if (!parse_u32(tokens[0], &entry->timestamp) ||
        !parse_u32(tokens[1], &entry->pid) ||
        !is_op_token(tokens[2]) ||
        !parse_u32(tokens[3], &entry->virtual_address)) {
        return -1;
    }

    entry->is_write = (tokens[2][0] == 'W' || tokens[2][0] == 'w');
    return 1;
}

void auditoria_print_summary(const simulator_t *sim) {
    double avg_frag = (sim->frag_samples == 0)
                        ? 0.0
                        : sim->frag_sum_percent / (double)sim->frag_samples;

   
    printf("             RESUMO FINAL - ANTIKIT-8\n");
    printf("Total de acessos do trace:          %" PRIu64 "\n", sim->trace_accesses);
    printf("Acessos globais contabilizados MMU: %" PRIu64 "\n", sim->hw.global_access_count);
    printf("Total global de Page Faults:        %" PRIu64 "\n", sim->total_page_faults);
    for (size_t i = 0; i < sim->process_count; i++) {
        const process_t *p = &sim->processes[i];
        if (p->active) {
            printf("  Processo %-5u -> Page Faults: %" PRIu64 "\n",
                   p->pid, p->page_faults);
        }
    }
    printf("Temperatura maxima do Swap:         %.1f C\n", sim->max_swap_temperature);
    printf("Ciclos desperdicados em E/S:        %" PRIu64 "\n", sim->io_cycles);
    printf("Ciclos totais de CPU (hardware):    %" PRIu64 "\n", sim->hw.total_cpu_cycles);
    printf("Fragmentacao interna media:         %.2f%%\n", avg_frag);
    printf("Checksum final (present/frame):     %016" PRIx64 "\n",
           auditoria_checksum_mapping(sim));
    printf("Consistencia tabela/RAM:            %s\n",
           auditoria_verify_consistency(sim) ? "OK" : "FALHA");
}
