#!/bin/bash
<<COMMENT
#
# 2026-05-08 Orin NX / Nano V1.0
# Modified for Independent Multi-Camera Resolution & Background Init
#
COMMENT

clear
START_DIR=$(pwd)

# Function for red text output
red_print(){
    echo -e "\e[1;31m$1\e[0m"
}

# Function for green text output
green_print(){
    echo -e "\e[1;32m$1\e[0m"
}

# Check and install v4l-utils if missing
if ! command -v v4l2-ctl >/dev/null 2>&1; then
    red_print "v4l2-ctl not found, installing v4l-utils..."
    sudo apt update
    sudo apt install -y v4l-utils
fi

red_print "This package is for MIC-Orin-NX-Nano-Series and Sensing camera on JetPack-6.2-L4T-36.4.3"

# Supported camera resolutions
camera_array=(
    [0]=1920x1080
    [1]=1920x1536
    [2]=2880x1860
    [3]=3840x2160
    [4]=1280x720
    [5]=3840x2160-ISX028
)

# 將原本位於 0x29 的 MAX96712 修改至 0x6b << 1 == 0xd6
# sudo i2ctransfer -y 2 w3@0x29 0x00 0x00 0xd6

# Navigate to the kernel module directory
cd $PWD/ko/MIC_SGX_YUV_GMSL2

# Insert max96712 driver if not already loaded
if [ "`sudo lsmod | grep max96712`" == "" ];then
    sudo insmod max96712.ko
fi

# Insert GMSL2 wrapper module with user-defined modes
if [ "`sudo lsmod | grep gmsl2`" == "" ];then
    green_print "Select camera type for each port: (GMSL=0, GMSL2/6G=1, GMSL2/3G=2)"

    green_print "Port 1 type:"
    read key1
    green_print "Port 2 type:"
    read key2
    green_print "Port 3 type:"
    read key3
    green_print "Port 4 type:"
    read key4

    sudo insmod sgx-yuv-gmsl2.ko GMSLMODE_1=${key1},${key2},${key3},${key4}
fi

echo "---------------------------------------------------"
green_print "Available camera resolutions:"
for i in "${!camera_array[@]}"; do
    echo "  $i : ${camera_array[$i]}"
done
echo "---------------------------------------------------"

# Array to store the selected resolution index for each port
declare -a cam_indices

# Prompt the user for resolution selection for each available camera
for port in {0..3}; do
    if [ -e "/dev/video${port}" ]; then
        green_print "Select resolution index [0-5] for /dev/video${port}:"
        read cam_indices[$port]
    else
        red_print "/dev/video${port} not found, skipping selection."
    fi
done

# Configure system clocks for maximum performance
sudo chmod a+x ../../clock_config.sh
sudo ../../clock_config.sh

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
        v4l2-ctl -d /dev/video${port} --set-fmt-video=width=${string[0]},height=${string[1]},pixelformat=UYVY --stream-mmap --stream-count=1 > /dev/null 2>&1 &

        sleep 0.2
	green_print "  -> /dev/video${port}: Init triggered (Mode: ${index} - ${camera_array[index]})"
    fi
done

# Wait for all background initialization processes to finish
wait

echo "---------------------------------------------------"
green_print "All available cameras have been successfully initialized!"

bash $START_DIR/camera_fsync_set.sh
