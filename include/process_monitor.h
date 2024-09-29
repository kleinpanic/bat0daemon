#ifndef PROCESS_MONITOR_H
#define PROCESS_MONITOR_H

int get_high_cpu_processes(char *process_list[], int max_processes);
void free_process_list(char *process_list[], int count);

#endif // PROCESS_MONITOR_H

