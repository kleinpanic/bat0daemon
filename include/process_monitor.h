#ifndef PROCESS_MONITOR_H
#define PROCESS_MONITOR_H

#include <stdbool.h>
#include <sys/types.h>

extern bool dry_run;
extern pid_t suspended_pids[];
extern int suspended_count;
extern pid_t suspended_high_cpu_pids[];
extern int suspended_high_cpu_count;

int run_battery_saving_mode(pid_t current_pid);
int get_ignore_processes(char *ignore_list[], int max_ignores, const char *config_key);
int is_process_critical(const char *process_name, char *ignore_list[], int ignore_count);

int suspend_user_daemons();
int resume_user_daemons();
int resume_high_cpu_processes();

#endif // PROCESS_MONITOR_H
