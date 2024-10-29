// process_monitor.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <stdbool.h>
#include <signal.h>
#include "process_monitor.h"
#include "log_message.h"
#include <dirent.h>
#include <pwd.h>
#include <sys/stat.h>

#define BUFFER_SIZE 1024
#define CONFIG_FILE "/.config/battery_monitor/config.conf"
#define MAX_CRITICAL_PROCESSES 100
#define MAX_IGNORE_PROCESSES 100
#define MAX_SUSPENDED_PROCESSES 1024

pid_t suspended_pids[MAX_SUSPENDED_PROCESSES];
int suspended_count = 0;
pid_t suspended_high_cpu_pids[MAX_SUSPENDED_PROCESSES];
int suspended_high_cpu_count = 0;

bool dry_run = true;  // Set to 'true' for dry run, 'false' for normal operation

// CPU usage threshold to consider a process as high CPU-consuming
#define CPU_USAGE_THRESHOLD 1.0

// List of default critical processes (expanded with more essential processes)
const char *default_critical_processes[] = {
    "systemd", "Xorg", "dbus-daemon", "NetworkManager", "dwm", "DWM",
    "sddm", "gdm", "fprintd", "gnome-shell", "plasmashell", "kdeinit",
    "kwin", "xfce4-session", "cinnamon", "mate-session", "pulseaudio",
    "pipewire", "pipewire-pulse", "wireplumber", "lightdm", "udisksd",
    "upowerd", "bash", "st", "picom", "python3", "gvfsd", "xdg-document-portal",
    "at-spi-bus-launcher", "at-spi2-registr", "volumeicon", NULL
};

// Function to perform case-insensitive string comparison
int case_insensitive_compare(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == *b;
}

// Function to output messages based on dry run mode
void output_message(const char *message) {
    if (dry_run) {
        printf("%s\n", message);
    } else {
        log_message(message);
    }
}

// Helper function to remove leading/trailing whitespace
static char *trim_whitespace(char *str) {
    char *end;

    // Trim leading space
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) return str;  // All spaces?

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    // Write new null terminator character
    end[1] = '\0';

    return str;
}

// Function to dynamically build the list of critical processes
void build_critical_processes_list(char *critical_list[], int *count) {
    int index = *count;

    // Add default critical processes
    for (int i = 0; default_critical_processes[i] != NULL && index < MAX_IGNORE_PROCESSES; i++) {
        critical_list[index++] = strdup(default_critical_processes[i]);
    }

    *count = index;
}

// Function to dynamically get the user's home directory and build the config file path
char *get_config_file_path() {
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        output_message("Failed to get HOME environment variable");
        return NULL;
    }

    size_t path_len = strlen(home_dir) + strlen(CONFIG_FILE) + 1;
    char *config_file_path = malloc(path_len);
    if (config_file_path != NULL) {
        strcpy(config_file_path, home_dir);
        strcat(config_file_path, CONFIG_FILE);
    }

    return config_file_path;
}

// Function to parse the ignore list from the config file
int get_ignore_processes(char *ignore_list[], int max_ignores, const char *config_key) {
    char *config_file_path = get_config_file_path();
    if (config_file_path == NULL) {
        output_message("Could not determine the config file path");
        // Proceed with default critical processes
        int ignore_count = 0;
        build_critical_processes_list(ignore_list, &ignore_count);
        return ignore_count;
    }

    FILE *config_file = fopen(config_file_path, "r");
    free(config_file_path);

    int ignore_count = 0;

    if (config_file == NULL) {
        output_message("Failed to open config file");
        // Proceed with default critical processes
        build_critical_processes_list(ignore_list, &ignore_count);
        return ignore_count;
    }

    char buffer[BUFFER_SIZE];

    while (fgets(buffer, sizeof(buffer), config_file) != NULL) {
        if (strstr(buffer, config_key) != NULL) {
            char *token = strtok(buffer, "=");
            token = strtok(NULL, "=");  // Get the processes list after '='

            if (token != NULL) {
                token = strtok(token, ",");
                while (token != NULL && ignore_count < max_ignores) {
                    ignore_list[ignore_count] = strdup(trim_whitespace(token));
                    ignore_count++;
                    token = strtok(NULL, ",");
                }
            }
        }
    }

    fclose(config_file);

    // Add default critical processes if not already included
    build_critical_processes_list(ignore_list, &ignore_count);

    return ignore_count;
}

// Function to check if a process is critical (case-insensitive check)
int is_process_critical(const char *process_name, char *ignore_list[], int ignore_count) {
    for (int i = 0; i < ignore_count; i++) {
        if (case_insensitive_compare(process_name, ignore_list[i])) {
            return 1;  // Process is critical
        }
    }
    return 0;
}

// Main function to run battery saving mode
int run_battery_saving_mode(pid_t current_pid) {
    output_message("Running battery saving mode in process_monitor");

    FILE *fp;
    char buffer[BUFFER_SIZE];

    // Command to get high CPU-consuming processes excluding root processes
    const char *command = "ps -eo user:32,pid,pcpu,comm --no-headers --sort=-pcpu";

    fp = popen(command, "r");
    if (fp == NULL) {
        output_message("Failed to run command to get high CPU processes");
        return -1;
    }

    // Load ignore processes from config file
    char *ignore_list[MAX_IGNORE_PROCESSES];
    int ignore_count = get_ignore_processes(ignore_list, MAX_IGNORE_PROCESSES, "ignore_processes_for_kill");
    if (ignore_count == -1) {
        output_message("Failed to get ignore processes");
        pclose(fp);
        return -1;
    }

    // Process the list and handle processes accordingly
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        // Remove leading and trailing whitespace from the buffer
        char *line = trim_whitespace(buffer);

        // Skip empty lines
        if (strlen(line) == 0) {
            continue;
        }

        char user[33];  // +1 for null terminator
        char command_name[256];
        int pid;
        float cpu_usage;

        int items = sscanf(line, "%32s %d %f %255[^\n]", user, &pid, &cpu_usage, command_name);

        if (items == 4) {
            // Exclude root processes
            if (strcmp(user, "root") == 0) {
                continue;
            }

            // Exclude processes with CPU usage below threshold
            if (cpu_usage < CPU_USAGE_THRESHOLD) {
                continue;
            }

            if (pid == current_pid) {
                output_message("Skipping suspending the current process.");
                continue;
            }

            if (!is_process_critical(command_name, ignore_list, ignore_count)) {
                char message[512];
                snprintf(message, sizeof(message), "Process to be suspended: %s (PID: %d, CPU Usage: %.2f%%)", command_name, pid, cpu_usage);
                output_message(message);

                if (dry_run) {
                    snprintf(message, sizeof(message), "Dry run mode active: Would suspend process %s (PID: %d)", command_name, pid);
                    output_message(message);
                } else {
                    // Suspend the process
                    if (kill(pid, SIGSTOP) == -1) {
                        perror("Failed to suspend process");
                        output_message("Failed to suspend process");
                    } else {
                        // Add to the list of suspended processes
                        if (suspended_high_cpu_count < MAX_SUSPENDED_PROCESSES) {
                            suspended_high_cpu_pids[suspended_high_cpu_count++] = pid;
                            output_message("Process suspended successfully");
                        } else {
                            output_message("Maximum suspended processes limit reached.");
                        }
                    }
                }
            } else {
                char message[512];
                snprintf(message, sizeof(message), "Skipping critical process: %s (PID: %d)", command_name, pid);
                output_message(message);
            }
        } else {
            output_message("Failed to parse process information");
        }
    }

    // Clean up
    for (int i = 0; i < ignore_count; i++) {
        free(ignore_list[i]);
    }

    pclose(fp);
    return 0;
}

int resume_high_cpu_processes() {
    for (int i = 0; i < suspended_high_cpu_count; i++) {
        pid_t pid = suspended_high_cpu_pids[i];

        if (dry_run) {
            printf("Dry run: Would resume high CPU process PID: %d\n", pid);
        } else {
            if (kill(pid, SIGCONT) == 0) {
                output_message("Resumed high CPU process");
            } else {
                perror("Failed to resume high CPU process");
            }
        }
    }
    suspended_high_cpu_count = 0;  // Reset the count after resuming
    return 0;
}

int suspend_user_daemons() {
    DIR *proc_dir = opendir("/proc");
    if (proc_dir == NULL) {
        perror("Failed to open /proc directory");
        return -1;
    }

    struct dirent *entry;
    uid_t uid = getuid();  // Get the UID of the current user

    // Load ignore processes from config file for suspending daemons
    char *ignore_list[MAX_IGNORE_PROCESSES];
    int ignore_count = get_ignore_processes(ignore_list, MAX_IGNORE_PROCESSES, "ignore_processes_for_sleep");
    if (ignore_count == -1) {
        output_message("Failed to get ignore processes");
        closedir(proc_dir);
        return -1;
    }

    while ((entry = readdir(proc_dir)) != NULL) {
        if (!isdigit(entry->d_name[0]))
            continue;

        pid_t pid = atoi(entry->d_name);
        if (pid == getpid())  // Skip current process
            continue;

        char status_file[256];
        snprintf(status_file, sizeof(status_file), "/proc/%d/stat", pid);

        FILE *status_fp = fopen(status_file, "r");
        if (status_fp == NULL)
            continue;

        char comm[256];
        char state;
        int ppid, pgrp, session;
        unsigned int tty_nr;
        uid_t proc_uid = -1;

        // Read necessary fields from /proc/[pid]/stat
        fscanf(status_fp, "%*d (%255[^)]) %c %d %d %d %u", comm, &state, &ppid, &pgrp, &session, &tty_nr);
        fclose(status_fp);

        // Get UID of the process owner
        char proc_status_file[256];
        snprintf(proc_status_file, sizeof(proc_status_file), "/proc/%d/status", pid);

        FILE *proc_status_fp = fopen(proc_status_file, "r");
        if (proc_status_fp == NULL)
            continue;

        char line[256];
        while (fgets(line, sizeof(line), proc_status_fp)) {
            if (strncmp(line, "Uid:", 4) == 0) {
                sscanf(line, "Uid:\t%u", &proc_uid);
                break;
            }
        }
        fclose(proc_status_fp);

        // Check if process is owned by the user and has no controlling terminal
        if (proc_uid == uid && tty_nr == 0) {
            // Check if process is not critical
            if (!is_process_critical(comm, ignore_list, ignore_count)) {
                if (dry_run) {
                    printf("Dry run: Would suspend process PID: %d (%s)\n", pid, comm);
                } else {
                    if (suspended_count < MAX_SUSPENDED_PROCESSES) {
                        if (kill(pid, SIGSTOP) == 0) {
                            suspended_pids[suspended_count++] = pid;
                            output_message("Suspended process");
                        } else {
                            perror("Failed to suspend process");
                        }
                    } else {
                        output_message("Maximum suspended processes limit reached.");
                        break;
                    }
                }
            } else {
                // Log that we are skipping a critical process
                if (dry_run) {
                    printf("Skipping critical process: PID: %d (%s)\n", pid, comm);
                }
            }
        }
    }

    // Free ignore_list
    for (int i = 0; i < ignore_count; i++) {
        free(ignore_list[i]);
    }

    closedir(proc_dir);
    return 0;
}

int resume_user_daemons() {
    for (int i = 0; i < suspended_count; i++) {
        pid_t pid = suspended_pids[i];

        if (dry_run) {
            printf("Dry run: Would resume process PID: %d\n", pid);
        } else {
            if (kill(pid, SIGCONT) == 0) {
                output_message("Resumed process");
            } else {
                perror("Failed to resume process");
            }
        }
    }
    suspended_count = 0;  // Reset the count after resuming
    return 0;
}

