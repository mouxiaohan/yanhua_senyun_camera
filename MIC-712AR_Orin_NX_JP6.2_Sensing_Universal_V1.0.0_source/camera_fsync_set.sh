#!/bin/bash

I2C_BUS=2
DESER_ADDR="0x29"
TARGET_FPS=30

echo "Advantech MIC series 96712 MFP2_framesync setting !"
echo "----------------------------------------------------------"
echo "Target I2C Bus: /dev/i2c-${I2C_BUS}"
echo "Target MAX96712 I2C Address: ${DESER_ADDR}"
echo "Target FPS: ${TARGET_FPS}"
echo "----------------------------------------------------------"

echo "fcp0 video[0-3] board setting start."

echo "setting 96712 fysnc ."
sudo i2ctransfer -f -y ${I2C_BUS} w3@${DESER_ADDR} 0x04 0xa0 0x08
sudo i2ctransfer -f -y ${I2C_BUS} w3@${DESER_ADDR} 0x04 0xaf 0x9f
sleep  0.2
sudo i2ctransfer -f -y ${I2C_BUS} w3@${DESER_ADDR} 0x03 0x06 0x83
sudo i2ctransfer -f -y ${I2C_BUS} w3@${DESER_ADDR} 0x03 0x07 0xa7
sudo i2ctransfer -f -y ${I2C_BUS} w3@${DESER_ADDR} 0x03 0x3d 0xa7
sudo i2ctransfer -f -y ${I2C_BUS} w3@${DESER_ADDR} 0x03 0x74 0xa7
sudo i2ctransfer -f -y ${I2C_BUS} w3@${DESER_ADDR} 0x03 0xaa 0xa7
sleep  0.2

echo "camera 0 channel :96717-mfp7 settings"
sudo i2ctransfer -f -y ${I2C_BUS} w3@0x42 0x02 0xd3 0x85
sudo i2ctransfer -f -y ${I2C_BUS} w3@0x42 0x00 0x10 0x21
sleep  0.2
sudo i2ctransfer -f -y ${I2C_BUS} w3@0x42 0x02 0xd3 0x84
sudo i2ctransfer -f -y ${I2C_BUS} w3@0x42 0x02 0xd5 0x07
sleep  0.2

echo "camera 1 channel :96717-mfp7 settings"
sudo i2ctransfer -f -y ${I2C_BUS} w3@0x44 0x02 0xd3 0x85
sudo i2ctransfer -f -y ${I2C_BUS} w3@0x44 0x00 0x10 0x21
sleep  0.2
sudo i2ctransfer -f -y ${I2C_BUS} w3@0x44 0x02 0xd3 0x84
sudo i2ctransfer -f -y ${I2C_BUS} w3@0x44 0x02 0xd5 0x07
sleep  0.2

echo "camera 2 channel:96717-mfp7 settings"
sudo i2ctransfer -f -y ${I2C_BUS} w3@0x46 0x02 0xd3 0x85
sudo i2ctransfer -f -y ${I2C_BUS} w3@0x46 0x00 0x10 0x21
sleep  0.2
sudo i2ctransfer -f -y ${I2C_BUS} w3@0x46 0x02 0xd3 0x84
sudo i2ctransfer -f -y ${I2C_BUS} w3@0x46 0x02 0xd5 0x07
sleep  0.2

echo "camera 3 channel:96717-mfp7 settings"
sudo i2ctransfer -f -y ${I2C_BUS} w3@0x48 0x02 0xd3 0x85
sudo i2ctransfer -f -y ${I2C_BUS} w3@0x48 0x00 0x10 0x21
sleep  0.2
sudo i2ctransfer -f -y ${I2C_BUS} w3@0x48 0x02 0xd3 0x84
sudo i2ctransfer -f -y ${I2C_BUS} w3@0x48 0x02 0xd5 0x07
sleep  0.2

echo "fcp1 board setting end."

echo "----------------------------------------------------------"
echo "Configuring PWM for External Trigger (FSYNC)..."
PWM_PERIOD=$(( 1000000000 / TARGET_FPS ))
PWM_DUTY=$(( PWM_PERIOD / 10 ))
echo "  -> Target FPS : ${TARGET_FPS}"
echo "  -> Period     : ${PWM_PERIOD} ns"
echo "  -> Duty Cycle : ${PWM_DUTY} ns"

if [ ! -d /sys/class/pwm/pwmchip2/pwm0 ]; then
    echo 0 | sudo tee /sys/class/pwm/pwmchip2/export > /dev/null
fi

echo 0 | sudo tee /sys/class/pwm/pwmchip2/pwm0/enable > /dev/null
echo ${PWM_PERIOD} | sudo tee /sys/class/pwm/pwmchip2/pwm0/period > /dev/null
echo ${PWM_DUTY} | sudo tee /sys/class/pwm/pwmchip2/pwm0/duty_cycle > /dev/null
echo 1 | sudo tee /sys/class/pwm/pwmchip2/pwm0/enable > /dev/null

echo "PWM started successfully!"
echo "----------------------------------------------------------"
echo "setting all end!"
