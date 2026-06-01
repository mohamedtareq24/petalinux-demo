SUMMARY  = "Xilinx AXI DMA kernel driver (bperez77/xilinx_axidma)"
HOMEPAGE = "https://github.com/bperez77/xilinx_axidma"
LICENSE  = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=4e905fb1f0726930632bec5bf36aa051"

inherit module

# All sources are local; patched for Linux 6.x compatibility
# (added #include <linux/of.h> to axi_dma.c).
SRC_URI = "file://Makefile \
           file://Kbuild \
           file://axi_dma.c \
           file://axidma.h \
           file://axidma_chrdev.c \
           file://axidma_dma.c \
           file://axidma_of.c \
           file://axidma_ioctl.h \
           file://LICENSE \
          "

# All files land flat in ${WORKDIR}; Kbuild references them directly.
S = "${WORKDIR}"

# The 'module' class provides do_compile and do_install automatically
# when ${S}/Kbuild exists. It calls:
#   make -C ${STAGING_KERNEL_BUILDDIR} M="${S}" modules
# and installs axidma.ko to /lib/modules/${KERNEL_VERSION}/extra/

KERNEL_MODULE_AUTOLOAD += "axidma"

FILES:${PN} += "/lib/modules/${KERNEL_VERSION}/extra/axidma.ko"
RPROVIDES:${PN} += "kernel-module-axidma"
