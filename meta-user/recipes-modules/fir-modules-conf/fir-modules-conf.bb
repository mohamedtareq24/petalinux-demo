SUMMARY = "Module loading config for FIR filter project"
DESCRIPTION = "\
  Installs /etc/modules-load.d/fir.conf so systemd-modules-load loads \
  axidma and uio_pdrv_genirq on every boot, and installs the matching \
  /etc/modprobe.d/fir.conf so uio_pdrv_genirq gets of_id=generic-uio. \
"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

# No source files needed — inline do_install creates the content.
SRC_URI = ""

do_configure[noexec] = "1"
do_compile[noexec]   = "1"

do_install() {
    # modprobe options: set of_id so the driver binds to generic-uio DT nodes
    install -d ${D}${sysconfdir}/modprobe.d
    cat > ${D}${sysconfdir}/modprobe.d/fir.conf <<'EOF'
# Load uio_pdrv_genirq with the generic-uio OF id so it probes
# the FIR AXI-Lite node (compatible = "generic-uio") in the device tree.
options uio_pdrv_genirq of_id=generic-uio
EOF

    # modules-load.d: systemd-modules-load loads these at boot
    install -d ${D}${sysconfdir}/modules-load.d
    cat > ${D}${sysconfdir}/modules-load.d/fir.conf <<'EOF'
# bperez77 AXI DMA character device driver
axidma
# Generic UIO platform driver (options are in modprobe.d/fir.conf)
uio_pdrv_genirq
EOF
}

FILES:${PN} = " \
    ${sysconfdir}/modprobe.d/fir.conf \
    ${sysconfdir}/modules-load.d/fir.conf \
"

# axidma module must be present; uio_pdrv_genirq comes from the kernel package
RDEPENDS:${PN} = "axidma"
