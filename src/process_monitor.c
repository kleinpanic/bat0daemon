#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include "log_message.h"

#define BUFFER_SIZE 1024
#define CONFIG_FILE "/.config/battery_monitor/config.config"
#define MAX_CRITICAL_PROCESSES 100
#define MAX_IGNORE_PROCESSES 100

// List of default critical processes (expanded with more essential processes)
const char *default_critical_processes[] = {"systemd", "Xorg", "dbus-daemon", "NetworkManager", "dwm", "DWM", "sddm", "gdm", "fprintd", NULL};

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

// Function to dynamically build the list of critical processes
void build_critical_processes_list(char *critical_list[], int *count) {
    int index = 0;

    // Add default critical processes
    for (int i = 0; default_critical_processes[i] != NULL; i++) {
        critical_list[index++] = strdup(default_critical_processes[i]);
    }

    *count = index;
}

// Helper function to remove leading/trailing whitespace
char *trim_whitespace(char *str) {
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

// Function to dynamically get the user's home directory and build the config file path
char *get_config_file_path() {
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        log_message("Failed to get HOME environment variable");
        return NULL;
    }

    char *config_file_path = malloc(strlen(home_dir) + strlen(CONFIG_FILE) + 1);
    if (config_file_path != NULL) {
        strcpy(config_file_path, home_dir);
        strcat(config_file_path, CONFIG_FILE);
    }

    return config_file_path;
}

// Function to parse the ignore list from the config file
int get_ignore_processes(char *ignore_list[], int max_ignores) {
    char *config_file_path = get_config_file_path();
    if (config_file_path == NULL) {
        log_message("Could not determine the config file path");
        return -1;
    }

    FILE *config_file = fopen(config_file_path, "r");
    free(config_file_path);

    if (config_file == NULL) {
        log_message("Failed to open config file");
        return -1;
    }

    char buffer[BUFFER_SIZE];
    int ignore_count = 0;

    while (fgets(buffer, sizeof(buffer), config_file) != NULL) {
        if (strstr(buffer, "ignore_processes_for_sleep") != NULL) {
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

    // Add default critical processes like dwm and Xorg if not already included
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

// Get the list of high CPU processes excluding ignored and root processes
int get_high_cpu_processes(char *process_list[], int max_processes) {
    FILE *fp;
    char buffer[BUFFER_SIZE];
    int process_count = 0;

    // Command to get top CPU-consuming processes excluding root processes
    const char *command = "ps -eo user,pid,comm,%cpu --sort=-%cpu | grep -vE '^root'";

    fp = popen(command, "r");
    if (fp == NULL) {
        log_message("Failed to run command to get high CPU processes");
        return -1;
    }

    // Load ignore processes from config file
    char *ignore_list[100];
    int ignore_count = get_ignore_processes(ignore_list, 100);

    // Parse each line from the process list
    while (fgets(buffer, sizeof(buffer), fp) != NULL && process_count < max_processes) {
        char user[50], command_name[100];
        int pid;
        float cpu_usage;

        if (sscanf(buffer, "%49s %d %99s %f", user, &pid, command_name, &cpu_usage) == 4) {
            if (!is_process_critical(command_name, ignore_list, ignore_count)) {
                // Allocate memory for the process info and store the PID
                process_list[process_count] = malloc(BUFFER_SIZE);
                snprintf(process_list[process_count], BUFFER_SIZE, "%d", pid);  // Only storing the PID
                process_count++;
            } else {
                char log_msg[200];
                snprintf(log_msg, sizeof(log_msg), "Skipping critical process: %s (PID: %d)", command_name, pid);
                log_message(log_msg);  // Log critical processes skipped
            }
        }
    }

    // Free ignore list
    for (int i = 0; i < ignore_count; i++) {
        free(ignore_list[i]);
    }

    pclose(fp);
    return process_count;
}

// Function to handle killing high CPU processes
void kill_high_cpu_processes(char *process_list[], int process_count, pid_t current_pid) {
    for (int i = 0; i < process_count; i++) {
        pid_t process_pid = atoi(process_list[i]);

        if (process_pid == current_pid) {
            log_message("Skipping killing the current process.");
            continue;
        }

        // Log the process PID before killing
        char log_msg[200];
        snprintf(log_msg, sizeof(log_msg), "Killing process (PID: %d)", process_pid);
        log_message(log_msg);

        // Kill the process by PID
        char command[50];
        snprintf(command, sizeof(command), "kill -9 %d", process_pid);
        if (system(command) == -1) {
            log_message("Failed to kill process");
        } else {
            log_message("Process killed successfully");
        }
    }
}

// Free the process list
void free_process_list(char *process_list[], int count) {
    for (int i = 0; i < count; i++) {
        free(process_list[i]);
    }
}
