#!/bin/bash

# Prevent overwrite customize image
# sudo cp boot/Image /boot/Image
# sudo cp dtb/MIC_SGX_YUV_GMSL2/tegra234-camera-yuv-gmslx4-overlay-0x6b.dtbo /boot/tegra234-camera-yuv-gmslx4-overlay.dtbo
sudo cp dtb/MIC_SGX_YUV_GMSL2/tegra234-camera-yuv-gmslx4-overlay.dtbo /boot/tegra234-camera-yuv-gmslx4-overlay.dtbo

sudo cp ./ko/MIC_SGX_YUV_GMSL2/tegra-camera.ko /lib/modules/5.15.148-tegra/updates/drivers/media/platform/tegra/camera/
sudo cp ./ko/MIC_SGX_YUV_GMSL2/nvhost-nvcsi-t194.ko /lib/modules/5.15.148-tegra/updates/drivers/video/tegra/host/nvcsi/
sudo cp ./ko/MIC_SGX_YUV_GMSL2/capture-ivc.ko /lib/modules/5.15.148-tegra/updates/drivers/platform/tegra/rtcpu/

sudo rm -rf /lib/modules/5.15.148-tegra/updates/drivers/media/i2c/max9296.ko
sudo rm -rf /lib/modules/5.15.148-tegra/updates/drivers/media/i2c/max9295.ko
sudo rm -rf /lib/modules/5.15.148-tegra/updates/drivers/media/i2c/max96712.ko
