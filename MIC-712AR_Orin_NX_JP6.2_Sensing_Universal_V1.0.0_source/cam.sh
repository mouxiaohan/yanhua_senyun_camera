#!/bin/bash

# ==============================================================================
# 1. Parameter Validation (Check if argument is provided)
# ==============================================================================
if [ -z "$1" ]; then
    echo "Error: Please provide a camera index!"
    echo "Usage: sudo bash $0 <index>"
    echo "Example: sudo bash $0 0"
    exit 1
fi

# ==============================================================================
# 2. Range Validation (Restrict to 0~3)
# ==============================================================================
# Use Regular Expression to ensure the input is exactly one digit between 0 and 3
if ! [[ "$1" =~ ^[0-3]$ ]]; then
    echo "Error: Invalid camera index '$1'."
    echo "Only camera numbers from 0 to 3 are supported."
    exit 1
fi

CAM_INDEX=$1
DEVICE_PATH="/dev/video${CAM_INDEX}"

# ==============================================================================
# 3. Hardware Validation (Check if the device node actually exists)
# ==============================================================================
if [ ! -e "$DEVICE_PATH" ]; then
    echo "Error: Camera device $DEVICE_PATH not found."
    echo "Please use 'ls /dev/video*' to check currently connected cameras."
    exit 1
fi

echo "Preparing to launch camera: $DEVICE_PATH ..."

# ==============================================================================
# 4. Core Execution Block
# ==============================================================================
# Export DISPLAY variable to allow X11 window generation over SSH / Headless setup
export DISPLAY=:0

# Launch the GStreamer pipeline
gst-launch-1.0 v4l2src device=${DEVICE_PATH} ! xvimagesink -ev
