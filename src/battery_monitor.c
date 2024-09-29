#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "battery_monitor.h"

// Define the battery thresholds
#define THRESHOLD_LOW 15
#define THRESHOLD_CRITICAL 5
#define THRESHOLD_HIGH 75

// Track if notifications have been sent
int notified_low = 0;
int notified_critical = 0;

int main() {
    log_message("Battery monitor started");

    while (1) {
        if (is_charging()) {
            // Reset notifications if the battery is charging
            log_message("Battery is charging, notifications reset");
            notified_low = 0;
            notified_critical = 0;
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
