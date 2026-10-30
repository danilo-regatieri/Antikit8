#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "hardware_specs.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define BASE_FRAME_SIZE PAGE_SIZE_EVEN
#define NUM_FRAMES (PHYSICAL_RAM_SIZE / BASE_FRAME_SIZE)
#define MAX_PROCESSES 128
#define MAX_VIRTUAL_PAGES ((VIRTUAL_MEMORY_SIZE / PAGE_SIZE_EVEN) + 1)
#define WSCLOCK_TAU 20u
#define MAP_KB_UNITS (PHYSICAL_RAM_SIZE / 1024u)

typedef struct {
    bool occupied;
    bool page_start;
    uint32_t pid;
    uint32_t vpn;
} ram_frame_t;

typedef struct {
    bool active;
    uint32_t pid;
    page_table_entry_t *page_table;
    uint32_t *used_bytes;
    uint64_t page_faults;
} process_t;

typedef struct {
    hardware_state_t hw;
    process_t processes[MAX_PROCESSES];
    size_t process_count;
    ram_frame_t ram[NUM_FRAMES];
    uint32_t num_pages;
    size_t clock_hand;

    uint64_t trace_accesses;
    uint64_t total_page_faults;
    uint64_t io_cycles;
    float max_swap_temperature;

    double frag_sum_percent;
    uint64_t frag_samples;

    FILE *metrics_csv;
} simulator_t;

typedef struct {
    uint32_t timestamp;
    uint32_t pid;
    bool is_write;
    uint32_t virtual_address;
} trace_entry_t;

#endif
