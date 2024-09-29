#!/usr/bin/env bash

# Path to the battery monitor binary
BINARY_PATH="/usr/local/bin/battery_monitor"

# Log file
LOG_FILE="/tmp/battery_monitor.log"

# Create Log Directory if it doesn't exist
LOG_DIR=$(dirname "$LOG_FILE")

if [ ! -d "$LOG_DIR" ]; then
    mkdir -p "$LOG_DIR"
fi 

if [ ! -d "$LOG_FILE" ]; then
    echo "Creating Find the log file"
    touch "$LOG_FILE"
fi
    
if [ ! -w "$LOG_FILE" ]; then
    echo "Cannot Write to the Log: $LOG_FILE. Attempting to change perms..."
    sudo chown $USER "$LOG_FILE" || {
        echo "Failed to change perms. Exiting"
        exit 1
    }
fi 

# Log starting message and environment variables
echo "Starting battery monitor script" >> "$LOG_FILE"
echo "DISPLAY=$DISPLAY" >> "$LOG_FILE"
echo "XAUTHORITY=$XAUTHORITY" >> "$LOG_FILE"

# Check if binary exists and is executable
if [ ! -x "$BINARY_PATH" ]; then
    echo "Binary not found or not executable: $BINARY_PATH" >> "$LOG_FILE"
    exit 1
fi

check_x_server() {
    if xset q > /dev/null 2>&1; then
        echo "X Server is running."
        return 0
    else 
        echo "X Server is not running yet."
        return 1
    fi
}

until check_x_server; do 
    sleep 1
done

while true; do 
    if pgrep -x "dwm" > /dev/null; then
        echo "dwm running"
        # Start the battery monitor and redirect output to log file
        exec "$BINARY_PATH" >> "$LOG_FILE" 2>&1
        
        while pgrep -x "dwm" > /dev/null; do 
            sleep 1 
        done
    else 
        echo "waiting for DWM to start"
        sleep 1
    fi 
done
