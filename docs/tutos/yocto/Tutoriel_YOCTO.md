# Developping Embedded Linux for ZedBoard using Yocto
## 1. Introduction

Yocto provides tools and metadata for creating custom embedded systems with following main features :

- Images are tailored to specific hardware and use cases
- But metadata is generally arch-independent
- Unlike a distro, *kitchen sink* is not included (we know what we need in advance)

Other keywords and their meanings are explained here:

- An image is a collection of *baked* recipes (packages)
- A 'recipe' is a set of instructions for building *packages*
  - Where to get the source and which patches to apply
  - Dependencies (on libraries or other recipes, for example)git clone -b jethro git://git.yoctoproject.org/poky.git
  - Config/compile options, *install* customization
- A *layer* is a logical collection of recipes representing the core, a board support package (BSP), or an application stack

### 1.1. Download sources from git repositories

```bash
git clone -b jethro https://git.yoctoproject.org/git/poky
git clone -b jethro https://github.com/Xilinx/meta-xilinx
git clone -b jethro https://github.com/openembedded/meta-openembedded
git clone -b jethro https://github.com/openembedded/openembedded-core
```

### 1.2. Build configuration

```bash
cd poky
source oe-init-build-env
```

Once initialized, configure `bblayers.conf` by adding the `meta-xilinx` and `meta-openembedded` layers.

```
BBLAYERS ?= " <path to layer>/openembedded-core/meta
<path to layer>/meta-xilinx
<path to layer>/meta-openembedded/meta-oe
```

To build a specific target BSP, configure the associated machine in `local.conf`: 

- `MACHINE ?= "zedboard-zynq7"` 
- Change `PACKAGE_CLASSES '= "package_rpm"` by `PACKAGE_CLASSES '= "package_deb"`
- Then, add the following lines:
  - `IMAGE_FEATURES += "package-management" BB_NUMBER_THREADS = "nb"`
  - `PARALLEL_MAKE = "-j nb"` (where nb equals the number of cores)
  - `DISTRO_HOSTNAME = "zynq"`

Build the target file system image using bitbake: `bitbake core-image-minimal`. Once complete the images for the target machine will be available in the output directory `poky/build/tmp/deploy/images/`.

### 1.3. Xilinx dependencies

```bash
sudo apt install ia32-libs
export CROSS_COMPILE=arm-xilinx-linux-gnueabi-
source <XilinxToolsInstallationDirectory>/ISE_DS/settings64.sh
```

## 2. Bootloader

### 2.1. Download repositories

```bash
git clone https://github.com/Xilinx/u-boot-xlnx
git clone https://github.com/Xilinx/linux-xlnx
git clone https://github.com/Xilinx/device-tree-xlnx
```

### 2.2. Use Vivado to create HW description file (hdf)

Follow the tutorial on the following link to generate a `.hdf` file:  `http://zedboard.org/zh-hant/node/1454`

It will generate an `.hdf` file in `zed_base\zed_base.sdk\design_1_wrapper.hdf`

### 2.3. FSBL creation

(hint: it is assumed that `setting**.sh` has been sourced)

```bash
hsi
hsi% set hwdsgn [open_hw_design <your_hdf_file>]
hsi% generate_app -hw $hwdsgn -os standalone -proc ps7_cortexa9_0 -app zynq_fsbl -compile -sw fsbl -dir <directory_for_new_app>
```

### 2.4. U-boot

Based on: `http://www.wiki.xilinx.com/Build+U-Boot`.

```bash
cd u-boot-xlnx
sudo apt-get install libssl-dev
export CROSS_COMPILE=arm-xilinx-linux-gnueabi-
make zynq_zed_config
make
cd tools
export PATH=`pwd`:$PATH
```

## SD card

TODO

## References

- http://picozed.org/content/building-zedboard-images
- https://github.com/Xilinx/meta-xilinx
- http://www.wiki.xilinx.com/Prepare+Boot+Image
- http://wiki.elphel.com/index.php?title=Yocto_tests
- http://git.yoctoproject.org/cgit/cgit.cgi/meta-xilinx/tree/README
- `yocto/meta-xilinx/docs/BOOT.sdcard` (found on `meta-xilinx` repository)







