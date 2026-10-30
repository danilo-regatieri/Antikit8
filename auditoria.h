#ifndef AUDITORIA_H
#define AUDITORIA_H

#include "simulator.h"


uint64_t auditoria_checksum_mapping(const simulator_t *sim);
bool auditoria_verify_consistency(const simulator_t *sim);
void auditoria_audit_state(const simulator_t *sim, const char *reason);

double auditoria_fragmentation_percent(const simulator_t *sim,
                                       uint64_t *allocated_out,
                                       uint64_t *waste_out);
void auditoria_print_fragmentation_map(const simulator_t *sim);
void auditoria_write_metrics_row(simulator_t *sim, uint32_t timestamp);
void auditoria_log_fading_event(simulator_t *sim, process_t *p,
                                const bool *before_ref);

int auditoria_parse_trace_line(char *line, trace_entry_t *entry);
void auditoria_print_summary(const simulator_t *sim);

#endif
