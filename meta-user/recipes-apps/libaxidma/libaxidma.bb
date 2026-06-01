SUMMARY  = "Xilinx AXI DMA userspace library (bperez77/xilinx_axidma)"
HOMEPAGE = "https://github.com/bperez77/xilinx_axidma"
LICENSE  = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=4e905fb1f0726930632bec5bf36aa051"

# Same source tree as the axidma kernel module recipe; share the git fetch.
SRC_URI = "git://github.com/bperez77/xilinx_axidma.git;protocol=https;branch=master"
SRCREV  = "${AUTOREV}"
PV      = "1.0+git${SRCPV}"

S = "${WORKDIR}/git"

do_compile() {
    # Force rebuild so the new GLOBAL_CFLAGS (with --hash-style=gnu) are used.
    rm -f "${S}/library/libaxidma.so" "${S}/outputs/libaxidma.so"
    oe_runmake -C ${S}                  \
        CC="${CC}"                      \
        AR="${AR}"                      \
        OUTPUT_DIR="${S}/outputs"       \
        GLOBAL_CFLAGS="${LDFLAGS}"      \
        library
}

do_install() {
    install -d ${D}${libdir}
    install -m 0755 ${S}/outputs/libaxidma.so ${D}${libdir}/

    install -d ${D}${includedir}
    install -m 0644 ${S}/include/libaxidma.h  ${D}${includedir}/
}

FILES:${PN}       = "${libdir}/libaxidma.so"
FILES:${PN}-dev   = "${includedir}/libaxidma.h"

# fir-demo links against this at build time
PROVIDES  += "libaxidma"
RPROVIDES:${PN} += "libaxidma"
