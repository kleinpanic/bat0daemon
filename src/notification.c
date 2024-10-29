// Notification.c 

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#include "battery_monitor.h"
#include "process_monitor.h"
#include "log_message.h"
#include <glob.h>
#include <sys/stat.h>

#define CSS_STYLE "\
    * { \
        background-color: #333333; \
        color: white; \
    } \
    button { \
        background-color: #555555; \
        color: white; \
    } \
"

extern int battery_saving_mode_active;

typedef enum {
    INIT_SYSTEMD,
    INIT_SYSVINIT,
    INIT_OPENRC,
    INIT_UNKNOWN
} init_system_t;

init_system_t detect_init_system() {
    struct stat sb;

    if (stat("/run/systemd/system", &sb) == 0) {
        return INIT_SYSTEMD;
    } else if (stat("/sbin/init", &sb) == 0) {
        // Additional checks can be added here
        return INIT_SYSVINIT;
    } else if (stat("/run/openrc", &sb) == 0) {
        return INIT_OPENRC;
    }

    return INIT_UNKNOWN;
}

// Function to find the battery path dynamically
char *get_battery_device_path(const char *file_name) {
    glob_t glob_result;
    char pattern[PATH_MAX];

    snprintf(pattern, sizeof(pattern), "/sys/class/power_supply/BAT*/%s", file_name);

    if (glob(pattern, 0, NULL, &glob_result) == 0) {
        if (glob_result.gl_pathc > 0) {
            char *battery_path = strdup(glob_result.gl_pathv[0]);
            globfree(&glob_result);
            return battery_path;
        }
    }

    globfree(&glob_result);
    return NULL;
}

// Function to get the battery level
int get_battery_level() {
    char *capacity_path = get_battery_device_path("capacity");
    if (capacity_path == NULL) {
        log_message("Failed to find battery capacity file");
        return -1;
    }

    FILE *file = fopen(capacity_path, "r");
    free(capacity_path);

    if (file == NULL) {
        perror("Failed to open capacity file");
        log_message("Failed to open capacity file");
        return -1;
    }

    int battery_level;
    if (fscanf(file, "%d", &battery_level) != 1) {
        perror("Failed to read battery level");
        log_message("Failed to read battery level");
        fclose(file);
        return -1;
    }

    fclose(file);
    return battery_level;
}

// Function to get the base directory of the executable
char *get_base_directory() {
    static char base_dir[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", base_dir, PATH_MAX);
    if (count != -1) {
        dirname(base_dir);
    }
    return base_dir;
}

// Function to log messages to a file
void log_message(const char *message) {
    char log_file[PATH_MAX];
    snprintf(log_file, PATH_MAX, "/tmp/battery_monitor.log");

    FILE *log_file_ptr = fopen(log_file, "a");
    if (log_file_ptr) {
        fprintf(log_file_ptr, "%s\n", message);
        fclose(log_file_ptr);
    } else {
        perror("Failed to open log file");
    }
}

char *get_backlight_device_path(const char *file_name) {
    glob_t glob_result;
    char pattern[PATH_MAX];

    snprintf(pattern, sizeof(pattern), "/sys/class/backlight/*/%s", file_name);

    if (glob(pattern, 0, NULL, &glob_result) == 0) {
        if (glob_result.gl_pathc > 0) {
            char *backlight_path = strdup(glob_result.gl_pathv[0]);
            globfree(&glob_result);
            return backlight_path;
        }
    }

    globfree(&glob_result);
    return NULL;
}

// Function to set the screen brightness
int set_brightness(int brightness) {
    char *brightness_path = get_backlight_device_path("brightness");
    char *max_brightness_path = get_backlight_device_path("max_brightness");

    if (brightness_path == NULL || max_brightness_path == NULL) {
        log_message("Failed to find backlight brightness files");
        free(brightness_path);
        free(max_brightness_path);
        return -1;
    }

    int max_brightness = 100;
    int new_brightness = 0;
    char buffer[10];

    // Open max brightness file
    int fd = open(max_brightness_path, O_RDONLY);
    if (fd == -1) {
        perror("Failed to open max brightness file");
        log_message("Failed to open max brightness file");
        return -1;
    }

    // Read max brightness value
    if (read(fd, buffer, sizeof(buffer)) != -1) {
        max_brightness = atoi(buffer);
    } else {
        perror("Failed to read max brightness");
        log_message("Failed to read max brightness");
        close(fd);
        return -1;
    }
    close(fd);

    // Calculate the new brightness
    new_brightness = max_brightness * brightness / 100;

    // Write the new brightness value to the brightness file
    fd = open(brightness_path, O_WRONLY);
    if (fd == -1) {
        perror("Failed to open brightness file");
        log_message("Failed to open brightness file");
        return -1;
    }

    snprintf(buffer, sizeof(buffer), "%d", new_brightness);
    if (write(fd, buffer, strlen(buffer)) == -1) {
        perror("Failed to write to brightness file");
        log_message("Failed to write to brightness file");
        close(fd);
        return -1;
    }

    free(brightness_path);
    free(max_brightness_path);
    close(fd);
    return 0;
}

// Function to activate battery saving mode
int activate_battery_saving_mode() {
    log_message("Activating battery saving mode");

    // Get the current PID of the running program
    pid_t current_pid = getpid();

    // Suspend high CPU processes
    log_message("Suspending high CPU processes");
    if (run_battery_saving_mode(current_pid) == -1) {
        log_message("Failed to suspend high CPU processes");
        return -1;
    }

    // Suspend user daemons
    log_message("Suspending user daemons");
    if (suspend_user_daemons() == -1) {
        log_message("Failed to suspend user daemons");
        return -1;
    }

    // Set the brightness to 50% for battery saving
    if (set_brightness(50) == -1) {
        log_message("Failed to set brightness to 50%");
        return -1;
    }

    // Set the battery-saving mode active flag
    battery_saving_mode_active = 1;

    return 0;
}

// Function to enter sleep mode
int enter_sleep_mode() {
    log_message("Entering sleep mode");

    init_system_t init_sys = detect_init_system();

    switch (init_sys) {
        case INIT_SYSTEMD:
            return system("systemctl suspend");
        case INIT_SYSVINIT:
            return system("pm-suspend");
        case INIT_OPENRC:
            return system("loginctl suspend");
        default:
            log_message("Unknown init system, cannot enter sleep mode");
            return -1;
    }
}

// Function to apply custom CSS styles to the GTK widgets
void apply_css(GtkWidget *widget, const char *css) {
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, css, -1, NULL);
    GtkStyleContext *context = gtk_widget_get_style_context(widget);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_object_unref(provider);
}

// Function to check the battery status and close the dialog if charging
gboolean check_battery_status(gpointer user_data) {
    GtkWidget *dialog = GTK_WIDGET(user_data);
    if (is_charging()) {
        log_message("Battery started charging, closing notification");
        gtk_widget_destroy(dialog);
        gtk_main_quit();
        return FALSE;
    }
    return TRUE;
}

// Signal handler for SIGINT
void handle_sigint(int sig) {
    log_message("SIGINT caught, ignoring Ctrl-C");
}

// Function to set up signal handling
void setup_signal_handling() {
    signal(SIGINT, handle_sigint);
}

// Function to ignore Enter key and other keyboard inputs
gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_Escape) {
        return TRUE;  // Prevent default behavior for Enter and Escape keys
    }
    return FALSE;  // Allow other keys if needed
}

// Function to handle dialog response
void on_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    switch (response_id) {
        case GTK_RESPONSE_OK:
            log_message("User clicked OK");
            break;
        case GTK_RESPONSE_APPLY:
            log_message("User activated Battery Saving Mode");
            activate_battery_saving_mode();
            break;
        case GTK_RESPONSE_CLOSE:
            log_message("User triggered Sleep Mode");
            enter_sleep_mode();
            break;
        default:
            break;
    }
    gtk_widget_destroy(GTK_WIDGET(dialog));
    gtk_main_quit();
}

// Function to show the notification dialog
void show_notification(const char *message, const char *title) {
    log_message("Showing notification");

    GtkWidget *dialog;
    gtk_init(0, NULL);

    dialog = gtk_message_dialog_new(NULL,
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_INFO,
                                    GTK_BUTTONS_NONE,
                                    "%s", message);
    gtk_dialog_add_button(GTK_DIALOG(dialog), "OK", GTK_RESPONSE_OK);
    gtk_dialog_add_button(GTK_DIALOG(dialog), "Battery Saving Mode", GTK_RESPONSE_APPLY);

    if (g_strcmp0(title, "Critical Battery Warning") == 0) {
        gtk_dialog_add_button(GTK_DIALOG(dialog), "Sleep", GTK_RESPONSE_CLOSE);
    }

    gtk_window_set_title(GTK_WINDOW(dialog), title);

    // Apply CSS styles
    apply_css(dialog, CSS_STYLE);

    // Set up the callback to check battery status and close if charging
    g_timeout_add(1000, check_battery_status, dialog);

    // Connect the dialog response to handle button clicks and ensure proper cleanup
    g_signal_connect(dialog, "response", G_CALLBACK(on_dialog_response), NULL);

    // Connect key-press-event signal to disable Enter key behavior
    g_signal_connect(dialog, "key-press-event", G_CALLBACK(on_key_press), NULL);

    // Set up signal handling to trap SIGINT
    setup_signal_handling();

    // Show the dialog and enter the GTK main loop
    gtk_widget_show_all(dialog);
    gtk_main();
}

// Function to check if the battery is charging
int is_charging() {
    char *status_path = get_battery_device_path("status");
    if (status_path == NULL) {
        log_message("Failed to find battery status file");
        return -1;
    }

    FILE *file = fopen(status_path, "r");
    free(status_path);

    if (file == NULL) {
        perror("Failed to open status file");
        log_message("Failed to open status file");
        return -1;
    }

    char status[16];
    if (fscanf(file, "%15s", status) != 1) {
        perror("Failed to read battery status");
        log_message("Failed to read battery status");
        fclose(file);
        return -1;
    }

    fclose(file);
    return (strcmp(status, "Charging") == 0);
}
