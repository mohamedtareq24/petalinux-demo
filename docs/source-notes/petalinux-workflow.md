# FIR Filter PetaLinux Workflow

## Prerequisites

- Vivado 2023.1 — XSA must be built first:
  ```bash
  cd /mnt/sdb1/petalinux_fir/vivado
  vivado -mode batch -source create_fir_project.tcl
  # produces zybo_fir_hw.xsa in same directory
  ```

- PetaLinux 2023.1 sourced:
  ```bash
  source ~/tools/xilinx23/petalinux/settings.sh
  ```

---

## Step 1 — Create project

```bash
cd /mnt/sdb1/petalinux/zynq_projects
petalinux-create --type project --template zynq --name zybo_fir_linux
cd **zybo_fir_linux**
```

---

## Step 2 — Import hardware platform (XSA)

```bash
petalinux-config --get-hw-description \
    /mnt/sdb1/petalinux_fir/vivado/zybo_fir_hw.xsa
```

In the menuconfig that opens, just save and exit.  
This generates `components/plnx_workspace/device-tree/device-tree/pl.dtsi`.

---

## Step 3 — Copy meta-user files

```bash
cp -r /mnt/sdb1/petalinux_fir/meta-user/recipes-modules \
      project-spec/meta-user/

cp -r /mnt/sdb1/petalinux_fir/meta-user/recipes-apps/libaxidma \
      project-spec/meta-user/recipes-apps/

cp -r /mnt/sdb1/petalinux_fir/meta-user/recipes-apps/fir-demo \
      project-spec/meta-user/recipes-apps/

cp -r /mnt/sdb1/petalinux_fir/meta-user/recipes-bsp \
      project-spec/meta-user/

cp /mnt/sdb1/petalinux_fir/meta-user/conf/user-rootfsconfig \
   project-spec/meta-user/conf/user-rootfsconfig
```

---

## Step 4 — Verify device tree node labels

Check that the auto-generated pl.dtsi has the expected labels:
```bash
grep -r "axidma\|dma\|fir" \
    components/plnx_workspace/device-tree/device-tree/pl.dtsi
```

Expected output should include something like:
```
axi_dma_0: dma@40400000 { ... };
fir_top_0: fir_top_0@<auto-addr> { ... };
```

If the labels differ from `axi_dma_0` / `fir_top_0`, edit:
```
project-spec/meta-user/recipes-bsp/device-tree/files/system-user.dtsi
```
and update the `&fir_top_0` override and the `dmas` phandles.

---

## Step 5 — Kernel configuration

```bash
petalinux-config -c kernel
```

Enable the following (search with `/`):
| Symbol                      | Value | Purpose                       |
|-----------------------------|-------|-------------------------------|
| `CONFIG_XILINX_AXIDMA`      | `y`   | Xilinx DMA engine driver (axidma depends on it) |
| `CONFIG_UIO_PDRV_GENIRQ`    | `y`   | generic-uio driver for FIR AXI-Lite |
| `CONFIG_CMA`                | `y`   | Contiguous Memory Allocator   |
| `CONFIG_DMA_CMA`            | `y`   | DMA CMA integration           |

Save and exit.

### Set CMA pool size (optional but recommended for DMA)

In `project-spec/meta-user/conf/petalinuxbsp.conf` (create if absent) add:
```
APPEND += "cma=32M"
```

---

## Step 6 — Rootfs configuration

```bash
petalinux-config -c rootfs
```

Navigate to `user packages` and enable:
- `axidma`
- `libaxidma`
- `fir-demo`

Save and exit.

---

## Step 7 — Build

```bash
petalinux-build
```

Build log at `build/build.log`. First build takes ~30–60 minutes.

---

## Step 8 — Package boot image

```bash
petalinux-package --boot \
    --fsbl images/linux/zynq_fsbl.elf \
    --fpga images/linux/system.bit    \
    --u-boot                          \
    --force
```

---

## Step 9 — SD card

Format a micro-SD with two partitions (FAT32 boot + ext4 root), then:
```bash
# Boot partition (FAT32):
cp images/linux/BOOT.BIN   /media/<user>/boot/
cp images/linux/boot.scr   /media/<user>/boot/
cp images/linux/image.ub   /media/<user>/boot/

# Root filesystem (ext4):
sudo tar -xzf images/linux/rootfs.tar.gz -C /media/<user>/rootfs/
sync
```

---

## Step 10 — Runtime test

Boot the Zybo, then over serial/SSH:
```bash
# Verify axidma driver loaded
lsmod | grep axidma
ls /dev/axidma

# Verify FIR UIO device
ls /sys/class/uio/
# Should show a uioN with name "fir"

# Run the FIR demo
fir-demo
# Expected: "FIR verification passed."
```

---