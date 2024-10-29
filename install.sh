#!/usr/bin/env bash

# Function to check if a command exists
check_dependency() {
    if ! command -v "$1" &> /dev/null; then
        echo "$1 is not installed. Installing..."
        sudo apt-get install -y "$1"
    else
        echo "$1 is already installed."
    fi
}

# Function to check if a package is installed (for libraries)
check_library() {
    if ! dpkg -s "$1" &> /dev/null; then
        echo "$1 is not installed. Installing..."
        sudo apt-get install -y "$1"
    else
        echo "$1 is already installed."
    fi
}

# Step 1: Check for dependencies and install if not present
dependencies=("gcc" "make" "pkg-config")
libraries=("libgtk-3-dev")  # Add other required development libraries here

echo "Checking for build dependencies..."
for dep in "${dependencies[@]}"; do
    check_dependency "$dep"
done

echo "Checking for required libraries..."
for lib in "${libraries[@]}"; do
    check_library "$lib"
done

# Step 2: Build the application
echo "Building the application..."
make clean
make

if [ $? -ne 0 ]; then
    echo "Build failed. Exiting."
    exit 1
fi

# Step 3: Find the current working directory and script location
SCRIPT_DIR=$(pwd)
SRC_SCRIPT="$SCRIPT_DIR/battery_monitor"
BASH_SCRIPT="$SCRIPT_DIR/battery_daemon.sh"

# Step 4: Check if battery_monitor exists in common locations
INSTALL_PATH="/usr/local/bin/battery_monitor"
EXISTING_VERSION=""
NEW_VERSION=""

if [ -f "$INSTALL_PATH" ]; then
    echo "Existing installation of battery_monitor found at $INSTALL_PATH"
    # Retrieve the version of the existing installation
    EXISTING_VERSION=$("$INSTALL_PATH" --version | awk '{print $NF}')
    echo "Existing battery_monitor version: $EXISTING_VERSION"
else
    echo "No existing installation of battery_monitor found."
fi

# Retrieve the version of the new build
if [ -f "$SRC_SCRIPT" ]; then
    NEW_VERSION=$("$SRC_SCRIPT" --version | awk '{print $NF}')
    echo "New battery_monitor version: $NEW_VERSION"
else
    echo "New build of battery_monitor not found. Exiting."
    exit 1
fi

# Function to compare versions
version_greater() {
    # Returns 0 if $1 > $2
    [ "$(printf '%s\n' "$2" "$1" | sort -V | head -n1)" != "$1" ]
}

# Step 5: Compare versions and install if new version is greater
if [ -z "$EXISTING_VERSION" ] || version_greater "$NEW_VERSION" "$EXISTING_VERSION"; then
    echo "Installing new version of battery_monitor..."

    # Copy the binary to /usr/local/bin
    sudo cp "$SRC_SCRIPT" "$INSTALL_PATH"
    sudo chmod +x "$INSTALL_PATH"

    echo "battery_monitor installed successfully."
else
    echo "Existing battery_monitor is up to date or newer. No installation needed."
fi

# Step 6: Check and install battery_daemon.sh
DAEMON_INSTALL_PATH="/usr/local/bin/battery_daemon.sh"
EXISTING_DAEMON_VERSION=""
NEW_DAEMON_VERSION=""

if [ -f "$DAEMON_INSTALL_PATH" ]; then
    echo "Existing installation of battery_daemon.sh found at $DAEMON_INSTALL_PATH"
    # Retrieve the version of the existing daemon
    EXISTING_DAEMON_VERSION=$("$DAEMON_INSTALL_PATH" --version | awk '{print $NF}')
    echo "Existing battery_daemon.sh version: $EXISTING_DAEMON_VERSION"
else
    echo "No existing installation of battery_daemon.sh found."
fi

# Retrieve the version of the new daemon script
if [ -f "$BASH_SCRIPT" ]; then
    NEW_DAEMON_VERSION=$("$BASH_SCRIPT" --version | awk '{print $NF}')
    echo "New battery_daemon.sh version: $NEW_DAEMON_VERSION"
else
    echo "battery_daemon.sh script not found in the current directory. Exiting."
    exit 1
fi

# Compare versions and install if new version is greater
if [ -z "$EXISTING_DAEMON_VERSION" ] || version_greater "$NEW_DAEMON_VERSION" "$EXISTING_DAEMON_VERSION"; then
    echo "Installing new version of battery_daemon.sh..."

    # Copy the daemon script to /usr/local/bin
    sudo cp "$BASH_SCRIPT" "$DAEMON_INSTALL_PATH"
    sudo chmod +x "$DAEMON_INSTALL_PATH"

    echo "battery_daemon.sh installed successfully."
else
    echo "Existing battery_daemon.sh is up to date or newer. No installation needed."
fi

# Step 7: Copy battery_monitor.service to user systemd folder
SYSTEMD_SERVICE="$SCRIPT_DIR/battery_monitor.service"
USER_SYSTEMD_DIR="$HOME/.config/systemd/user"

if [[ -f "$SYSTEMD_SERVICE" ]]; then
    echo "Found battery_monitor.service."

    # Create the user systemd directory if it doesn't exist
    if [[ ! -d "$USER_SYSTEMD_DIR" ]]; then
        echo "Creating user systemd directory: $USER_SYSTEMD_DIR"
        mkdir -p "$USER_SYSTEMD_DIR"
    fi

    echo "Copying battery_monitor.service to $USER_SYSTEMD_DIR"
    cp "$SYSTEMD_SERVICE" "$USER_SYSTEMD_DIR/"
else
    echo "battery_monitor.service not found in the current directory!"
    exit 1
fi

# Step 8: Reload the systemd daemon, enable and restart the service (user-level)
echo "Reloading systemd user daemon..."
systemctl --user daemon-reload

echo "Enabling battery_monitor.service..."
systemctl --user enable battery_monitor.service

echo "Restarting battery_monitor.service..."
systemctl --user restart battery_monitor.service

# Check if the service was successfully started
if systemctl --user is-active --quiet battery_monitor.service; then
    echo "Service started successfully!"
else
    echo "Failed to start the service."
    exit 1
fi

# Step 9: Create default configuration if not exists
CONFIG_DIR="$HOME/.config/battery_monitor"
CONFIG_FILE="$CONFIG_DIR/config.conf"
DEFAULT_CONFIG="$SCRIPT_DIR/config.conf"

if [[ ! -d "$CONFIG_DIR" ]]; then
    echo "Creating configuration directory at $CONFIG_DIR"
    mkdir -p "$CONFIG_DIR"
fi

if [[ ! -f "$CONFIG_FILE" ]]; then
    if [[ -f "$DEFAULT_CONFIG" ]]; then
        echo "Copying default config.conf to $CONFIG_FILE"
        cp "$DEFAULT_CONFIG" "$CONFIG_FILE"
    else
        echo "Default config.conf not found!"
        # Optionally, create a default config file here
        cat <<EOF > "$CONFIG_FILE"
# Default configuration for battery_monitor

threshold_low=25
threshold_critical=5
threshold_high=80

# Add other configuration options as needed
EOF
        echo "Created default configuration file."
    fi
else
    echo "Configuration file already exists at $CONFIG_FILE"
fi

echo "Installation and setup complete."

