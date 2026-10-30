#include "auditoria.h"
#include "hardware_specs.h"
#include "mmu.h"
#include "simulator.h"
#include "wsclock.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Integracao dos tres modulos do trabalho.
 * MMU: Bruno Aparecido Almeida Rocha - RA 821591
 * WSClock/Swap: Danilo Mitsunari Regatieri - RA 831830
 * Auditoria/Metricas: Isadora Berbel Kamada Ribeiro - RA 844978
 */

static bool process_access(simulator_t *sim, const trace_entry_t *entry) {
    process_t *p = mmu_get_or_create_process(sim, entry->pid);
    if (p == NULL) return false;

    uint32_t vpn = 0;
    uint32_t offset = 0;
    if (!mmu_translate_address(entry->virtual_address,
                               sim->num_pages,
                               &vpn,
                               &offset)) {
        fprintf(stderr,
                "[ERRO] t=%u pid=%u endereco virtual %u fora do espaco de 1 MB.\n",
                entry->timestamp, entry->pid, entry->virtual_address);
        return true;
    }

    sim->trace_accesses++;
    printf("\n[ACESSO #%-5" PRIu64 "] t=%u pid=%u %c VA=%u (0x%05X) "
           "-> VPN=%u offset=%u\n",
           sim->trace_accesses,
           entry->timestamp,
           entry->pid,
           entry->is_write ? 'W' : 'R',
           entry->virtual_address,
           entry->virtual_address,
           vpn,
           offset);

    bool before_ref[MAX_VIRTUAL_PAGES] = {false};
    bool fading_due = ((sim->hw.global_access_count + 1u) % FADING_THRESHOLD == 0u);
    if (fading_due) {
        for (uint32_t i = 0; i < sim->num_pages; i++) {
            before_ref[i] = p->page_table[i].referenced;
        }
    }

    bool hit = mmu_access_memory(&sim->hw,
                                 p->page_table,
                                 sim->num_pages,
                                 entry->virtual_address,
                                 entry->is_write,
                                 entry->timestamp);

    if (fading_due) {
        auditoria_log_fading_event(sim, p, before_ref);
    }

    if (!hit) {
        sim->total_page_faults++;
        p->page_faults++;
        printf("[PAGE FAULT] pid=%u vpn=%u nao esta na RAM.\n", p->pid, vpn);

        if (!wsclock_map_page(sim, p, vpn, entry->timestamp)) {
            fprintf(stderr, "Erro: nao foi possivel obter espaco fisico para a pagina.\n");
            return false;
        }

        /* O acesso do trace conta uma vez. Depois do mapeamento registramos
           o efeito do acesso sem chamar a MMU novamente. */
        p->page_table[vpn].referenced = true;
        p->page_table[vpn].last_access_t = entry->timestamp;
        if (entry->is_write) p->page_table[vpn].modified = true;
        auditoria_audit_state(sim, "pos-page-fault");
    } else {
        printf("[HIT] pid=%u vpn=%u frame-base=%u R=%d M=%d\n",
               p->pid,
               vpn,
               p->page_table[vpn].frame_number,
               p->page_table[vpn].referenced,
               p->page_table[vpn].modified);
    }

    mmu_update_page_usage(p, vpn, offset);

    double frag = auditoria_fragmentation_percent(sim, NULL, NULL);
    sim->frag_sum_percent += frag;
    sim->frag_samples++;
    auditoria_write_metrics_row(sim, entry->timestamp);

    if (sim->trace_accesses % 10u == 0u) {
        auditoria_print_fragmentation_map(sim);
    }

    return true;
}

static void simulator_init(simulator_t *sim, FILE *metrics_csv) {
    memset(sim, 0, sizeof(*sim));
    sim->num_pages = mmu_count_virtual_pages();
    sim->max_swap_temperature = BASE_SWAP_TEMP;
    sim->metrics_csv = metrics_csv;
    hardware_init(&sim->hw);

    if (metrics_csv != NULL) {
        fprintf(metrics_csv,
                "access,timestamp,swap_temperature,fragmentation_percent,page_faults,io_cycles\n");
    }

    srand(42);
}

int main(int argc, char **argv) {
    const char *trace_path = (argc >= 2) ? argv[1] : "memory_trace.txt";
    const char *metrics_path = (argc >= 3) ? argv[2] : "metrics.csv";

    FILE *trace = fopen(trace_path, "r");
    if (trace == NULL) {
        fprintf(stderr,
                "Erro: nao foi possivel abrir '%s'.\n"
                "Uso: %s [memory_trace.txt] [metrics.csv]\n",
                trace_path, argv[0]);
        return EXIT_FAILURE;
    }

    FILE *metrics = fopen(metrics_path, "w");
    if (metrics == NULL) {
        fprintf(stderr, "Aviso: nao foi possivel criar '%s'; seguindo sem CSV.\n",
                metrics_path);
    }

    simulator_t sim;
    simulator_init(&sim, metrics);

    printf("Antikit-8 - Simulador de Gerenciamento de Memoria\n");
    printf("Espaco virtual por processo: %u bytes\n", VIRTUAL_MEMORY_SIZE);
    printf("Paginas virtuais assimetricas: %u entradas (4KB/8KB alternadas)\n",
           sim.num_pages);
    printf("RAM fisica: %u bytes = %u quadros-base de 4KB\n",
           PHYSICAL_RAM_SIZE, (unsigned)NUM_FRAMES);
    printf("Algoritmo de substituicao: WSClock adaptado (tau=%u)\n",
           WSCLOCK_TAU);
    printf("Trace: %s\n\n", trace_path);

    char line[512];
    uint64_t line_no = 0;
    bool ok = true;

    while (fgets(line, sizeof(line), trace) != NULL) {
        line_no++;
        trace_entry_t entry;
        int parsed = auditoria_parse_trace_line(line, &entry);

        if (parsed == 0) continue;
        if (parsed < 0) {
            fprintf(stderr,
                    "[TRACE] linha %" PRIu64
                    " invalida. Formato esperado: timestamp pid R|W endereco\n",
                    line_no);
            ok = false;
            continue;
        }

        if (!process_access(&sim, &entry)) {
            ok = false;
            break;
        }
    }

    auditoria_print_summary(&sim);

    mmu_destroy_processes(&sim);
    fclose(trace);
    if (metrics != NULL) fclose(metrics);

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
