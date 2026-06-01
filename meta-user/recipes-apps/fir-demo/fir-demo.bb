SUMMARY = "FIR filter demo — AXI DMA + AXI-Lite UIO test application"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://COPYING;md5=10533213da7cdc13a45abe77d5e28708"

DEPENDS = "libaxidma"

# All sources are flat file:// URIs — bitbake copies them into ${WORKDIR}.
# Set S = ${WORKDIR} so LIC_FILES_CHKSUM and do_compile find them there.
S = "${WORKDIR}"

# Source files live in the project repo, not inside the recipe.
# Pull the application entry point and hardware backend from sw/demo,
# while support sources still come from sw/verialtor.
FILESEXTRAPATHS:prepend := "${THISDIR}/../../../../sw/demo:${THISDIR}/../../../../sw/verialtor:"

SRC_URI = " \
    file://main.cpp        \
    file://backend.h       \
    file://hw_backend.cpp  \
    file://axilite_ctrl.cpp \
    file://axilite_ctrl.h  \
    file://libaxidma.h     \
    file://COPYING         \
"

# Minimal MIT COPYING stub — avoids LIC_FILES_CHKSUM failures.
# The real license text is in the axidma / libaxidma recipes above.
do_configure[noexec] = "1"

do_compile() {
    ${CXX} ${CXXFLAGS} \
        -std=c++17      \
        -I${WORKDIR}    \
        -o fir-demo     \
        ${WORKDIR}/main.cpp \
        ${WORKDIR}/hw_backend.cpp \
        ${WORKDIR}/axilite_ctrl.cpp \
        ${LDFLAGS} -laxidma
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 fir-demo ${D}${bindir}/fir-demo
}

FILES:${PN} = "${bindir}/fir-demo"
RDEPENDS:${PN} += "libaxidma axidma"
