Dans le noyau cloné, on a bien le device tree à jour avec les composants Coresight:
`poky/build/tmp/work-shared/zedboard-zynq7/kernel-source/arch/arm/boot/dts/zynq-7000.dtsi`

Configuration Yocto:
```bash
Build Configuration:
BB_VERSION           = "1.44.0"
BUILD_SYS            = "x86_64-linux"
NATIVELSBSTRING      = "universal"
TARGET_SYS           = "arm-poky-linux-gnueabi"
MACHINE              = "zedboard-zynq7"
DISTRO               = "poky"
DISTRO_VERSION       = "3.0.3"
TUNE_FEATURES        = "arm vfp cortexa9 neon thumb callconvention-hard"
TARGET_FPU           = "hard"
meta                 
meta-poky            
meta-yocto-bsp       = "zeus:d6c3a4db81576aec3a7ceab17969faff97e94aa0"
meta-xilinx-bsp      = "zeus:b82343ac5f013926839627cee9dae7106c008ae9"
meta-oe              = "zeus:2b5dd1eb81cd08bc065bc76125f2856e9383e98b"
```

https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/18841862/Install+and+Build+with+Xilinx+Yocto
- boot.scr ?

Finally let's install the images onto an SD card.  The directory structure is slightly different depending on whether you are installing with boot.scr or extlinux.conf

```bash
sudo cp
x boot.bin
core-image-minimal-zedboard-zynq7.cpio.gz.u-boot
u-boot.img
uEnv.txt
uImage
zynq-zed.dtb
-- boot.scr
```

Notes sur le wiki Xilinx
```bash
cp boot.scr /mnt/partition1
cp boot.bin /mnt/partition1
cp zynq-zed.dtb /mnt/partition1
cp uImage /mnt/partition1
```
