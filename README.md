# Battery Monitor Daemon

A battery monitor daemon built in C using the GTK framework. The application monitors your system's battery level and provides notifications when the battery is low or critically low. It also implements battery-saving features like reducing screen brightness and managing background processes to help preserve battery life.

---

## Table of Contents

- [Features](#features)
- [Installation](#installation)
  - [Dependencies](#dependencies)
  - [Building and Installing the Application](#building-and-installing-the-application)
- [Configuration](#configuration)
  - [Adjusting Battery Thresholds](#adjusting-battery-thresholds)
  - [Configuring Process Management](#configuring-process-management)
- [Uninstallation](#uninstallation)
- [Contributing](#contributing)

## Features

---

- **Configurable Battery Thresholds**: Receive notifications when the battery level falls below user-defined thresholds for low and critical levels. Adjust these thresholds easily via a configuration file.

- **Dynamic Monitoring Interval**: The application adjusts its battery level check interval based on the current battery percentage to optimize performance and responsiveness.

- **Battery Saving Mode**:
  - Reduces screen brightness to 50% when the battery is low.
  - Suspends high CPU-consuming processes and user daemons to conserve battery life.
  - Allows users to specify which processes to ignore during suspension.

- **Logging**: Activity is logged to `/tmp/battery_monitor.log` for debugging and monitoring purposes.

- **Systemd Service**: Runs as a user-level systemd service, starting automatically upon login.

- **Version Checking**: Supports version checking for both the application and the accompanying scripts.

---

## Installation

### Dependencies

Ensure you have the following dependencies installed on your system:

- **gcc**: GNU Compiler Collection for compiling the C code.
- **make**: Utility for directing compilation.
- **pkg-config**: Helper tool used during compilation.
- **GTK+ 3 Development Libraries**: Library for creating graphical user interfaces.

**On Debian/Ubuntu:**

```bash
sudo apt-get update
sudo apt-get install build-essential pkg-config libgtk-3-dev
```

**On Fedora:**

```bash
sudo dnf install gcc make pkgconf-pkg-config gtk3-devel
```

**On Arch Linux:**

```bash
sudo pacman -S base-devel pkgconf gtk3
```

### Building and Installing the Application

1. **Clone the Repository**

   ```bash
   git clone https://github.com/kleinpanic/bat0daemon.git
   cd bat0daemon
   ```

2. **Run the Installation Script**

   ```bash
   ./install.sh
   ```

   The `install.sh` script will:

   - Check for and install any missing dependencies.
   - Build the application using `make`.
   - Install the `battery_monitor` binary to `/usr/local/bin`.
   - Install the `battery_daemon.sh` script to `/usr/local/bin`.
   - Set up a user-level systemd service to run the application automatically on login.
   - Create a default configuration file at `~/.config/battery_monitor/config.conf` if it does not exist.

**Note**: The `install.sh` script may prompt for your password to use `sudo` for installation.

---

## Configuration

The application uses a configuration file located at `~/.config/battery_monitor/config.conf`. If the file does not exist, the `install.sh` script will create one with default values.

### Adjusting Battery Thresholds

Edit the configuration file to adjust battery thresholds:

```ini
# ~/.config/battery_monitor/config.conf

threshold_low=25
threshold_critical=5
threshold_high=80
```

- **threshold_low**: Battery percentage at which the application will send a low battery notification.
- **threshold_critical**: Battery percentage at which the application will send a critical battery notification.
- **threshold_high**: Battery percentage above which the application checks the battery level less frequently.

### Configuring Process Management

Specify processes to ignore when the application suspends high CPU-consuming processes or user daemons:

```ini
ignore_processes_for_kill=process1, process2, process3
ignore_processes_for_sleep=process4, process5, process6
```

- **ignore_processes_for_kill**: List of processes to ignore when suspending high CPU-consuming processes.
- **ignore_processes_for_sleep**: List of processes to ignore when suspending user daemons.

**Example**:

```ini
ignore_processes_for_kill=firefox, code
ignore_processes_for_sleep=dropbox, slack
```

---

## Uninstallation

To remove the application and its associated files:

1. **Stop and Disable the Systemd Service**

   ```bash
   systemctl --user stop battery_monitor.service
   systemctl --user disable battery_monitor.service
   ```

2. **Remove Installed Files**

   ```bash
   sudo rm /usr/local/bin/battery_monitor
   sudo rm /usr/local/bin/battery_daemon.sh
   ```

3. **Remove the Systemd Service File**

   ```bash
   rm ~/.config/systemd/user/battery_monitor.service
   ```

4. **Reload the Systemd Daemon**

   ```bash
   systemctl --user daemon-reload
   ```

5. **Remove Configuration and Log Files (Optional)**

   ```bash
   rm -rf ~/.config/battery_monitor
   rm /tmp/battery_monitor.log
   ```

6. **Clean Up Build Files**

   ```bash
   make clean
   ```

---

## Contributing

Contributions are welcome! If you have ideas for new features, improvements, or bug fixes, feel free to fork the repository and submit a pull request. Let's make this application even better together.

---

**Additional Notes**:

- **Battery Monitoring**: The application dynamically finds the battery device path, supporting systems with different battery naming conventions (e.g., `BAT0`, `BAT1`).

- **Process Suspension**: The application can suspend non-critical background processes to conserve battery life when in battery-saving mode. Critical system processes are automatically excluded.

- **Customization**: Users can tailor the application's behavior extensively through the configuration file.

---

