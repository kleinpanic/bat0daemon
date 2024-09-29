#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

void show_notification(const char *message, const char *title);
int get_battery_level();
int is_charging();
int activate_battery_saving_mode();
int enter_sleep_mode();
int kill_processes(const char *filename);
int set_brightness(int brightness);
void log_message(const char *message);

// New function declaration for process monitoring
int get_high_cpu_processes(char *process_list[], int max_processes);

#endif // BATTERY_MONITOR_H
