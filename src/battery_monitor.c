// Battery_monitor.c

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "battery_monitor.h"
#include "process_monitor.h"
#include "version.h"
#include <ctype.h>  
#include <string.h>  
#include <limits.h>

// Global Variables for Thresholds
int THRESHOLD_LOW = 15;       // Default values
int THRESHOLD_CRITICAL = 5;
int THRESHOLD_HIGH = 70;

// Function to trim leading and trailing whitespace
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

void load_thresholds_from_config() {
    char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        log_message("Failed to get HOME environment variable");
        return;
    }

    char config_file_path[PATH_MAX];
    snprintf(config_file_path, sizeof(config_file_path), "%s/.config/battery_monitor/config.config", home_dir);

    FILE *config_file = fopen(config_file_path, "r");
    if (config_file == NULL) {
        log_message("Failed to open config file, using default thresholds");
        return;
    }

    char buffer[256];

    while (fgets(buffer, sizeof(buffer), config_file) != NULL) {
        char *line = trim_whitespace(buffer);

        // Skip empty lines and comments
        if (strlen(line) == 0 || line[0] == '#') {
            continue;
        }

        char key[64];
        char value[64];

        if (sscanf(line, "%63[^=]=%63s", key, value) == 2) {
            if (strcmp(key, "threshold_low") == 0) {
                THRESHOLD_LOW = atoi(value);
            } else if (strcmp(key, "threshold_critical") == 0) {
                THRESHOLD_CRITICAL = atoi(value);
            } else if (strcmp(key, "threshold_high") == 0) {
                THRESHOLD_HIGH = atoi(value);
            }
        }
    }

    fclose(config_file);

    // Log the loaded thresholds
    char message[256];
    snprintf(message, sizeof(message), "Loaded thresholds: LOW=%d, CRITICAL=%d, HIGH=%d",
             THRESHOLD_LOW, THRESHOLD_CRITICAL, THRESHOLD_HIGH);
    log_message(message);
}

// Track if notifications have been sent
int notified_low = 0;
int notified_critical = 0;
int battery_saving_mode_active = 0;  // 0: inactive, 1: active

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        printf("Battery Monitor version %s\n", VERSION);
        return 0;
    }
    log_message("Battery monitor started");

    load_thresholds_from_config();

    while (1) {
        if (is_charging()) {
            // Reset notifications if the battery is charging
            log_message("Battery is charging, notifications reset");
            notified_low = 0;
            notified_critical = 0;

            if (battery_saving_mode_active) {
                // Resume suspended processes
                log_message("Battery is charging, resuming suspended processes");
                resume_high_cpu_processes();
                resume_user_daemons();
                battery_saving_mode_active = 0;
            }

            sleep(300); // Sleep for 5 minutes while charging
            continue;
        }

        int battery_level = get_battery_level();
        if (battery_level == -1) {
            log_message("Battery level read failed, retrying in 1 minute");
            sleep(60);
            continue;
        }

        // Dynamic sleep interval based on battery level
        int sleep_duration = 60; // Default 1 minute

        if (battery_level > THRESHOLD_HIGH) {
            sleep_duration = 300; // Sleep for 5 minutes
        } else if (battery_level <= THRESHOLD_CRITICAL) {
            sleep_duration = 30; // Sleep for 30 seconds when critically low
        } else if (battery_level <= THRESHOLD_LOW) {
            sleep_duration = 60; // Sleep for 1 minute when low
        }

        // Check if battery-saving mode is active and battery level has surpassed THRESHOLD_LOW
        if (battery_saving_mode_active && battery_level > THRESHOLD_LOW) {
            log_message("Battery level above threshold, resuming suspended processes");
            resume_high_cpu_processes();
            resume_user_daemons();
            battery_saving_mode_active = 0;
        }

        // Check if the battery level is below the critical threshold
        if (battery_level <= THRESHOLD_CRITICAL && !notified_critical) {
            log_message("Battery critically low, showing notification");
            show_notification("Battery is critically low, below 5%", "Critical Battery Warning");
            notified_critical = 1;
        } else if (battery_level <= THRESHOLD_LOW && !notified_low) {
            log_message("Battery low, showing notification");
            show_notification("Battery is low, below 15%", "Low Battery Warning");
            notified_low = 1;
        }

        // Reset notifications if battery level goes back up
        if (battery_level > THRESHOLD_LOW) {
            notified_low = 0;
        }
        if (battery_level > THRESHOLD_CRITICAL) {
            notified_critical = 0;
        }

        // Wait for the dynamically determined duration before checking again
        sleep(sleep_duration);
    }

    return 0;
}
