# Battery 0 Dameon
A battery daemon running the GTK framework built in C. Application monitors file structure /sys/class/power_suppply/BAT0/capacity file and if you have it its accounts for BAT1 (I'm pretty sure. In theory it does but I have not tested it as I dont have that file structure). 

## Table of Contents

- [Features](#features)
- [Installation](#installation)
  - [Dependencies](#dependencies)
  - [Building the Application](#building-the-application)
  - [Installing the Application](#installing-the-application)
- [Uninstallation](#uninstallation)
- [Contributing](#contributing)

## features

- GTK notification for 15 percent or less, and GTK notification for 5 percent or less.
    - to change, go to src/battery_monitor.c, change #define THRESHOLD_LOW or #define THRESHOLD_CRITICAL to custom values.
- Dynamic battery percentage changing depending on last read value.
    - When above THRESHOLD_HIGH, checks every 5 minutes.
    - When Below critical, checks every 30 seconds. 
    - When low, checks every minute. 
- Logging file created in /tmp/battery_monitor.log
- Battery saving mode changes brightness to help preserve battery
- Battery saving mode attempt to manage background procress is still in progress. 

## Installation 

### Dependencies 
Ensure you have the following dependencies installed on your system:
- **gcc**: GNU Compiler Collection for compiling the C code.
- **make**: Utility for directing compilation.
- **GTK+**: Library for creating Graphical User Interfaces
- **Standard C Headers**: Standard Libraries found in glibc

**On Debian/Ubuntu:**

```bash
sudo apt-get update
sudo apt-get install build-essential libgtk-3-dev
```

**On Fedora:**

```bash
sudo dnf install gcc make gtk3-devel
```

**On Arch Linux:**

```bash
sudo pacman -S base-devel gtk3
```

### Building the Application

1. **Clone the Repository**

   ```bash
   git clone https://github.com/kleinpanic/bat0daemon.git
   cd bat0daemon
   ```

2. **Build the Application**

   ```bash
   make
   ```

   This will compile the source code and create the executable in the base directory.

### Installing the Application

Optionally, you can install the application system-wide:

```bash
make
./install.sh
```

## Uninstallation

To remove the application and its associated files:

1. **Remove the Executable**

   If installed system-wide:

   ```bash
   make clean
   ```

2. **Delete Configuration and Data Files**

   ```bash
   sudo rm -rf ~/usr/local/bin/bat0daemon
   *add more*
   ```


## Contributing

Contributions are welcome! If you have ideas for new features or improvements, feel free to fork the repository and submit a pull request. Let's make this application even better together.

---
