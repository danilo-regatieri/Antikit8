#include "wsclock.h"
#include "auditoria.h"
#include "mmu.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>

static uint32_t safe_swap_timestamp(simulator_t *sim, uint32_t requested_t) {
    uint32_t t = requested_t;
    if (t < sim->hw.last_swap_ms) {
        t = sim->hw.last_swap_ms;
    }

    if (t == sim->hw.last_swap_ms) {
        if (sim->hw.swap_temperature + 15.0f <= MAX_SAFE_TEMP) {
            return t;
        }

        float needed = 2.0f * (sim->hw.swap_temperature - 53.0f);
        uint32_t delay = 1u;
        if (needed > 1.0f) delay = (uint32_t)ceilf(needed);
        return sim->hw.last_swap_ms + delay;
    }

    float cooled = sim->hw.swap_temperature -
                   0.5f * (float)(t - sim->hw.last_swap_ms);
    if (cooled < BASE_SWAP_TEMP) cooled = BASE_SWAP_TEMP;

    if (cooled + 12.0f <= MAX_SAFE_TEMP) return t;

    float extra = 2.0f * (cooled + 12.0f - MAX_SAFE_TEMP);
    uint32_t delay = (uint32_t)ceilf(extra);
    if (delay < 1u) delay = 1u;
    return t + delay;
}

static void do_page_out(simulator_t *sim,
                        process_t *p,
                        uint32_t vpn,
                        uint32_t current_t) {
    page_table_entry_t *e = &p->page_table[vpn];
    if (!e->modified) return;

    uint32_t io_t = safe_swap_timestamp(sim, current_t);
    if (io_t != current_t) {
        printf("[SWAP] Page Out pid=%u vpn=%u escalonado de t=%u para t=%u "
               "para permitir resfriamento.\n",
               p->pid, vpn, current_t, io_t);
    } else {
        printf("[SWAP] Page Out pid=%u vpn=%u em t=%u.\n",
               p->pid, vpn, io_t);
    }

    uint64_t before = sim->hw.total_cpu_cycles;
    hardware_trigger_page_out(&sim->hw, io_t);
    uint64_t cost = sim->hw.total_cpu_cycles - before;
    sim->io_cycles += cost;

    if (sim->hw.swap_temperature > sim->max_swap_temperature) {
        sim->max_swap_temperature = sim->hw.swap_temperature;
    }

    printf("[SWAP] temperatura=%.1f C custo=%" PRIu64 " ciclos\n",
           sim->hw.swap_temperature, cost);
    e->modified = false;
}

static bool select_wsclock_victim(simulator_t *sim,
                                  uint32_t current_t,
                                  uint32_t *victim_pid,
                                  uint32_t *victim_vpn) {
    int clean_fallback = -1;
    int dirty_fallback = -1;
    uint32_t dirty_oldest_age = 0;

    for (uint32_t step = 0; step < NUM_FRAMES * 2u; step++) {
        uint32_t idx = (uint32_t)(sim->clock_hand % NUM_FRAMES);
        sim->clock_hand = (sim->clock_hand + 1u) % NUM_FRAMES;

        ram_frame_t *f = &sim->ram[idx];
        if (!f->occupied || !f->page_start) continue;

        process_t *p = mmu_find_process(sim, f->pid);
        if (p == NULL || f->vpn >= sim->num_pages) continue;

        page_table_entry_t *e = &p->page_table[f->vpn];
        uint32_t age = (current_t >= e->last_access_t)
                         ? (current_t - e->last_access_t)
                         : 0u;

        if (e->referenced) {
            e->referenced = false;
            printf("[WSCLOCK] pid=%u vpn=%u recebeu segunda chance (R:1->0).\n",
                   p->pid, f->vpn);
            auditoria_audit_state(sim, "WSClock limpa R");
            continue;
        }

        if (!e->modified) {
            if (age >= WSCLOCK_TAU) {
                *victim_pid = p->pid;
                *victim_vpn = f->vpn;
                return true;
            }
            if (clean_fallback < 0) clean_fallback = (int)idx;
        } else if (dirty_fallback < 0 || age > dirty_oldest_age) {
            dirty_fallback = (int)idx;
            dirty_oldest_age = age;
        }
    }

    int chosen = (clean_fallback >= 0) ? clean_fallback : dirty_fallback;
    if (chosen < 0) return false;

    *victim_pid = sim->ram[chosen].pid;
    *victim_vpn = sim->ram[chosen].vpn;
    return true;
}

static bool evict_one_page(simulator_t *sim, uint32_t current_t) {
    uint32_t pid = 0;
    uint32_t vpn = 0;

    if (!select_wsclock_victim(sim, current_t, &pid, &vpn)) return false;

    process_t *p = mmu_find_process(sim, pid);
    if (p == NULL) return false;

    page_table_entry_t *e = &p->page_table[vpn];
    printf("[WSCLOCK] vitima pid=%u vpn=%u frame=%u R=%d M=%d\n",
           pid, vpn, e->frame_number, e->referenced, e->modified);

    if (e->modified) do_page_out(sim, p, vpn, current_t);

    uint32_t old_frame = e->frame_number;
    mmu_release_page_frames(sim, p, vpn);
    e->present = false;
    e->referenced = false;
    e->modified = false;
    e->frame_number = 0;

    printf("[RAM] pid=%u vpn=%u removida do frame-base %u.\n",
           pid, vpn, old_frame);
    auditoria_audit_state(sim, "desmapeamento");
    return true;
}

bool wsclock_map_page(simulator_t *sim,
                      process_t *p,
                      uint32_t vpn,
                      uint32_t current_t) {
    uint32_t need = mmu_frames_needed(vpn);
    int start = mmu_find_free_run(sim, need);

    uint32_t attempts = 0;
    while (start < 0 && attempts < NUM_FRAMES) {
        if (!evict_one_page(sim, current_t)) return false;
        start = mmu_find_free_run(sim, need);
        attempts++;
    }

    if (start < 0) return false;
    if (!mmu_place_page(sim, p, vpn, current_t, start)) return false;

    auditoria_audit_state(sim, "mapeamento");
    return true;
}
