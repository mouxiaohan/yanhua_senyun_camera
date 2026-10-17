#!/bin/bash
<<COMMENT
#
# 2026-05-08 Orin NX / Nano V1.0
# Modified for Independent Multi-Camera Resolution & Background Init
#
COMMENT

[ -t 1 ] && clear
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
START_DIR="$SCRIPT_DIR"

# Function for red text output
red_print(){
    echo -e "\e[1;31m$1\e[0m"
}

# Function for green text output
green_print(){
    echo -e "\e[1;32m$1\e[0m"
}

MODULE_DIR="${SCRIPT_DIR}/ko/MIC_SGX_YUV_GMSL2"
CLOCK_CONFIG="${SCRIPT_DIR}/clock_config.sh"
FSYNC_SCRIPT="${SCRIPT_DIR}/camera_fsync_set.sh"
STREAM_TIMEOUT="${STREAM_TIMEOUT:-5}"

usage(){
    echo "Usage:"
    echo "  $0"
    echo "  $0 key1 key2 key3 key4"
    echo "  $0 key1 key2 key3 key4 res0 res1 res2 res3"
    echo ""
    echo "Camera type: GMSL=0, GMSL2/6G=1, GMSL2/3G=2"
    echo "Resolution index: 0-5"
    echo ""
    echo "Example:"
    echo "  $0 1 1 1 1 1 1 1 1"
}

# Supported camera resolutions
camera_array=(
    [0]=1920x1080
    [1]=1920x1536
    [2]=2880x1860
    [3]=3840x2160
    [4]=1280x720
    [5]=3840x2160-ISX028
)

is_camera_type(){
    case "$1" in
        0|1|2) return 0 ;;
        *) return 1 ;;
    esac
}

is_resolution_index(){
    local index="$1"
    [[ "$index" =~ ^[0-9]+$ ]] && [ -n "${camera_array[$index]}" ]
}

read_camera_type(){
    local port="$1"
    local value

    while true; do
        green_print "Port ${port} type:" >&2
        read -r value
        if is_camera_type "$value"; then
            printf '%s\n' "$value"
            return
        fi
        red_print "Invalid camera type: $value (use 0, 1, or 2)" >&2
    done
}

read_resolution_index(){
    local video="$1"
    local value

    while true; do
        green_print "Select resolution index [0-5] for /dev/video${video}:" >&2
        read -r value
        if is_resolution_index "$value"; then
            printf '%s\n' "$value"
            return
        fi
        red_print "Invalid resolution index: $value" >&2
    done
}

declare -a camera_modes
declare -a cam_indices
args=("$@")

case "$1" in
    -h|--help)
        usage
        exit 0
        ;;
esac

if [ "$#" -ne 0 ] && [ "$#" -ne 4 ] && [ "$#" -ne 8 ]; then
    usage
    exit 1
fi

if [ "$#" -ge 4 ]; then
    for port in {0..3}; do
        camera_modes[$port]="${args[$port]}"
        if ! is_camera_type "${camera_modes[$port]}"; then
            red_print "Invalid key$((port + 1)): ${camera_modes[$port]} (use 0, 1, or 2)"
            exit 1
        fi
    done
fi

if [ "$#" -eq 8 ]; then
    for port in {0..3}; do
        cam_indices[$port]="${args[$((port + 4))]}"
        if ! is_resolution_index "${cam_indices[$port]}"; then
            red_print "Invalid resolution index for /dev/video${port}: ${cam_indices[$port]}"
            exit 1
        fi
    done
fi

# Check and install v4l-utils if missing
if ! command -v v4l2-ctl >/dev/null 2>&1; then
    red_print "v4l2-ctl not found, installing v4l-utils..."
    sudo apt update
    sudo apt install -y v4l-utils
fi

red_print "This package is for MIC-Orin-NX-Nano-Series and Sensing camera on JetPack-6.2-L4T-36.4.3"

# 將原本位於 0x29 的 MAX96712 修改至 0x6b << 1 == 0xd6
# sudo i2ctransfer -y 2 w3@0x29 0x00 0x00 0xd6

# Navigate to the kernel module directory
if [ ! -d "$MODULE_DIR" ]; then
    red_print "Kernel module directory not found: $MODULE_DIR"
    exit 1
fi
cd "$MODULE_DIR" || exit 1

# Insert max96712 driver if not already loaded
if [ "`sudo lsmod | grep max96712`" == "" ];then
    sudo insmod max96712.ko
fi

# Insert GMSL2 wrapper module with user-defined modes
if [ "`sudo lsmod | grep gmsl2`" == "" ];then
    if [ "$#" -lt 4 ]; then
        green_print "Select camera type for each port: (GMSL=0, GMSL2/6G=1, GMSL2/3G=2)"
        for port in {0..3}; do
            camera_modes[$port]=$(read_camera_type "$((port + 1))")
        done
    else
        green_print "Using camera types: ${camera_modes[0]} ${camera_modes[1]} ${camera_modes[2]} ${camera_modes[3]}"
    fi

    sudo insmod sgx-yuv-gmsl2.ko GMSLMODE_1=${camera_modes[0]},${camera_modes[1]},${camera_modes[2]},${camera_modes[3]}
fi

echo "---------------------------------------------------"
green_print "Available camera resolutions:"
for i in "${!camera_array[@]}"; do
    echo "  $i : ${camera_array[$i]}"
done
echo "---------------------------------------------------"

# Prompt the user for resolution selection for each available camera
for port in {0..3}; do
    if [ -e "/dev/video${port}" ]; then
        if [ -n "${cam_indices[$port]}" ]; then
            green_print "Using resolution index ${cam_indices[$port]} for /dev/video${port}"
        else
            cam_indices[$port]=$(read_resolution_index "$port")
        fi
    else
        red_print "/dev/video${port} not found, skipping selection."
    fi
done

# Configure system clocks for maximum performance
if [ -f "$CLOCK_CONFIG" ]; then
    sudo chmod a+x "$CLOCK_CONFIG"
    sudo "$CLOCK_CONFIG"
else
    red_print "clock_config.sh not found: $CLOCK_CONFIG"
fi

echo "---------------------------------------------------"
green_print "Initializing cameras in the background with custom resolutions..."

# Apply specific resolution settings to each camera and trigger stream
for port in {0..3}; do
    # Check if a resolution index was successfully assigned to this port
    if [ -n "${cam_indices[$port]}" ]; then
        index=${cam_indices[$port]}

        # Parse width and height from the selected resolution string
        string=(`echo ${camera_array[index]} | sed 's/-.*//' | tr 'x' ' '`)

        # 1. Set sensor mode and bypass mode controls for this specific camera
        v4l2-ctl --set-ctrl bypass_mode=0,sensor_mode=${index} -d /dev/video${port}

        # 2. Trigger a single frame stream in the background
        (
            if ! timeout "${STREAM_TIMEOUT}s" v4l2-ctl -d /dev/video${port} --set-fmt-video=width=${string[0]},height=${string[1]},pixelformat=UYVY --stream-mmap --stream-count=1 > /dev/null 2>&1; then
                red_print "  -> /dev/video${port}: Init stream timed out or failed"
            fi
        ) &

        sleep 0.2
	green_print "  -> /dev/video${port}: Init triggered (Mode: ${index} - ${camera_array[index]})"
    fi
done

# Wait for all background initialization processes to finish
wait

echo "---------------------------------------------------"
green_print "All available cameras have been successfully initialized!"

if [ -f "$FSYNC_SCRIPT" ]; then
    bash "$FSYNC_SCRIPT"
else
    red_print "camera_fsync_set.sh not found: $FSYNC_SCRIPT"
fi
