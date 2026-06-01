##############################################################
# create_fir_project.tcl
#
# Vivado 2023.1 — Zybo Z7-10 FIR filter + AXI DMA project
#
# Block design:
#
#   ┌──────────────────────────────────────────────────────────────┐
#   │                  ZYNQ7 Processing System                     │
#   │  M_AXI_GP0 ──────────────────────────────┐  S_AXI_HP0 ←─┐    │
#   │  IRQ_F2P ← mm2s_introut + s2mm_introut   │              │    │
#   └──────────────────────────────────────────│──────────────│────┘
#                                              │              │
#                              ┌───────────────┘              │
#                              │                              │
#                     ┌────────▼────────┐           ┌─────────┴─────────┐
#                     │ AXI Interconnect│           │  AXI SmartConnect │
#                     └──┬──────┬───────┘           └──┬────────────────┘
#                        │      │                      │
#               ┌─────────▼┐  ┌──▼──────────┐    ┌────▲──────────────┐
#               │ AXI DMA  │  │  fir_top_0  │    │ M_AXI_MM2S        │
#               │(no SG)   │  │  (5-tap FIR)│    │ M_AXI_S2MM        │
#               │0x40400000│  │  s_axi ctrl │    └───────────────────┘
#               └──┬────┬──┘  └─────────────┘
#     M_AXIS_MM2S ─┘    └─ S_AXIS_S2MM
#          │    AXI4-Stream     ▲
#          └────────────────────┘
#            FIR s_axis / m_axis
#
# Address map:
#   0x40400000  AXI DMA S_AXI_LITE  64 KB   (GP0, pinned)
#   <auto>      FIR top s_axi       64 KB   (GP0, Vivado assigns freely)
#   0x00000000  PS DDR via HP0     512 MB   (DMA buffers)
#
# Usage:
#   vivado -mode batch -source create_fir_project.tcl
#
# Prerequisites:
#   • Vivado 2023.1
#   • Digilent Zybo Z7-10 board files installed
#     https://github.com/Digilent/vivado-boards
#     Copy new/board_files/zybo-z7-10 → <Vivado>/2023.1/data/boards/board_files/
##############################################################


# ============================================================
#  Configuration
# ============================================================
set SCRIPT_DIR [file dirname [file normalize [info script]]]
set RTL_DIR    [file normalize [file join $SCRIPT_DIR "../rtl"]]

set CFG(proj_name)  "zybo_fir_hw"
set CFG(proj_dir)   [file normalize "$SCRIPT_DIR"]
set CFG(part)       "xc7z010clg400-1"
set CFG(bd_name)    "system"
set CFG(top_module) "system_wrapper"    ;# explicit top — no alias mapping
set CFG(xsa_path)   [file normalize [file join $SCRIPT_DIR "zybo_fir_hw.xsa"]]

# Auto-detect core count (Linux), cap at 8
set CFG(jobs) 4
catch {
    set _ncpu [llength [glob /sys/devices/system/cpu/cpu\[0-9\]*]]
    set CFG(jobs) [expr {min($_ncpu, 8)}]
    unset _ncpu
}


# ============================================================
#  Helper procedures  (same pattern as create_zybo_z710_project.tcl)
# ============================================================
proc step {msg} {
    set ts [clock format [clock seconds] -format "%H:%M:%S"]
    puts "INFO \[$ts\]: $msg"
}

# Pin an AXI address segment.  Prints a WARN (not error) if the segment
# name doesn't match this Vivado version — auto-assigned value is kept.
proc assign_addr {seg offset_hex range} {
    if {[catch {
        set_property offset $offset_hex  [get_bd_addr_segs $seg]
        set_property range  $range       [get_bd_addr_segs $seg]
    } err]} {
        puts "WARN: Could not pin address for '$seg' — auto-assigned address used"
        puts "WARN: ($err)"
    }
}

proc die {msg} {
    puts "\nERROR: $msg\n"
    exit 1
}


# ============================================================
#  Pre-flight checks
# ============================================================
step "Checking prerequisites..."

if {[file exists $CFG(proj_dir)/$CFG(proj_name)]} {
    die "Project directory already exists: $CFG(proj_dir)/$CFG(proj_name)\
\n       Delete it first or change CFG(proj_dir)."
}

if {[llength [get_board_parts digilentinc.com:zybo-z7-10:* -quiet]] == 0} {
    die "Zybo Z7-10 board files not found.\
\n       Install from: https://github.com/Digilent/vivado-boards\
\n       Copy new/board_files/zybo-z7-10 to Vivado's board_files/ directory."
}
set CFG(board_part) [lindex [lsort \
    [get_board_parts digilentinc.com:zybo-z7-10:* -quiet]] end]

step "All checks passed.  Board: $CFG(board_part)  Jobs: $CFG(jobs)"
step "RTL source directory: $RTL_DIR"


# ============================================================
#  Create project
# ============================================================
step "Creating project '$CFG(proj_name)' in $CFG(proj_dir)/$CFG(proj_name)"
create_project $CFG(proj_name) $CFG(proj_dir)/$CFG(proj_name) \
    -part $CFG(part)
set_property board_part      $CFG(board_part) [current_project]
set_property target_language Verilog          [current_project]


# ============================================================
#  Add RTL sources
# ============================================================
step "Adding RTL sources from $RTL_DIR ..."

add_files -norecurse [list \
    $RTL_DIR/fir_top.v            \
    $RTL_DIR/my_fir_v1_0.v        \
    $RTL_DIR/my_fir_v1_0_S_AXI.sv \
    $RTL_DIR/my_fir_v1_0_S_AXIS.sv \
    $RTL_DIR/my_fir_v1_0_M_AXIS.sv \
    $RTL_DIR/FIR_transposed.sv    \
    $RTL_DIR/transposed_block.v   \
]

# Mark .sv files as SystemVerilog (fir_top.v is plain Verilog — skip it)
foreach sv_file [glob -nocomplain $RTL_DIR/*.sv] {
    catch {
        set_property file_type {SystemVerilog} [get_files [file tail $sv_file]]
    }
}
update_compile_order -fileset sources_1


# ============================================================
#  Block Design
# ============================================================
step "Building block design '$CFG(bd_name)' ..."
create_bd_design $CFG(bd_name)
current_bd_design $CFG(bd_name)


# ------------------------------------------------------------
#  1. Zynq PS7
# ------------------------------------------------------------
step "  Adding Zynq PS7..."
create_bd_cell -type ip \
    -vlnv xilinx.com:ip:processing_system7:5.5 ps7_0

# Board preset fills in DDR, MIO (UART1, SD0, Ethernet, USB)
apply_bd_automation \
    -rule xilinx.com:bd_rule:processing_system7 \
    -config {make_external "FIXED_IO, DDR" apply_board_preset "1"
             Master "Disable" Slave "Disable"} \
    [get_bd_cells ps7_0]

set_property -dict {
    CONFIG.PCW_USE_M_AXI_GP0              1
    CONFIG.PCW_USE_S_AXI_HP0              1
    CONFIG.PCW_FPGA0_PERIPHERAL_FREQMHZ   100
    CONFIG.PCW_EN_CLK0_PORT               1
    CONFIG.PCW_USE_FABRIC_INTERRUPT       1
    CONFIG.PCW_IRQ_F2P_INTR               1
} [get_bd_cells ps7_0]

# GP0 and HP0 run on the same 100 MHz FCLK_CLK0
connect_bd_net \
    [get_bd_pins ps7_0/FCLK_CLK0] \
    [get_bd_pins ps7_0/M_AXI_GP0_ACLK]
connect_bd_net \
    [get_bd_pins ps7_0/FCLK_CLK0] \
    [get_bd_pins ps7_0/S_AXI_HP0_ACLK]


# ------------------------------------------------------------
#  2. Processor System Reset  (100 MHz domain)
# ------------------------------------------------------------
step "  Adding proc_sys_reset..."
create_bd_cell -type ip \
    -vlnv xilinx.com:ip:proc_sys_reset:5.0 rst_ps7_0_100M

connect_bd_net \
    [get_bd_pins ps7_0/FCLK_CLK0]     \
    [get_bd_pins rst_ps7_0_100M/slowest_sync_clk]
connect_bd_net \
    [get_bd_pins ps7_0/FCLK_RESET0_N]  \
    [get_bd_pins rst_ps7_0_100M/ext_reset_in]


# ------------------------------------------------------------
#  3. AXI Interconnect  (GP0 master → 2 control slaves)
#     M00: AXI DMA S_AXI_LITE   (0x40400000)
#     M01: FIR fir_top_0 s_axi  (Vivado auto-assigns in GP0)
# ------------------------------------------------------------
step "  Adding AXI Interconnect (1M → 2S)..."
create_bd_cell -type ip \
    -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_interconnect_0
set_property CONFIG.NUM_MI {2} [get_bd_cells axi_interconnect_0]

foreach pin {ACLK S00_ACLK M00_ACLK M01_ACLK} {
    connect_bd_net \
        [get_bd_pins ps7_0/FCLK_CLK0] \
        [get_bd_pins axi_interconnect_0/$pin]
}
connect_bd_net \
    [get_bd_pins rst_ps7_0_100M/interconnect_aresetn] \
    [get_bd_pins axi_interconnect_0/ARESETN]
foreach pin {S00_ARESETN M00_ARESETN M01_ARESETN} {
    connect_bd_net \
        [get_bd_pins rst_ps7_0_100M/peripheral_aresetn] \
        [get_bd_pins axi_interconnect_0/$pin]
}
connect_bd_intf_net \
    [get_bd_intf_pins ps7_0/M_AXI_GP0] \
    [get_bd_intf_pins axi_interconnect_0/S00_AXI]


# ------------------------------------------------------------
#  4. AXI DMA  (no scatter-gather, 32-bit stream, MM2S + S2MM)
# ------------------------------------------------------------
step "  Adding AXI DMA..."
create_bd_cell -type ip \
    -vlnv xilinx.com:ip:axi_dma:7.1 axi_dma_0

set_property -dict {
    CONFIG.c_include_sg             0
    CONFIG.c_m_axi_mm2s_data_width  32
    CONFIG.c_m_axi_s2mm_data_width  32
    CONFIG.c_mm2s_burst_size        16
    CONFIG.c_s2mm_burst_size        16
} [get_bd_cells axi_dma_0]

# Control registers ← GP0 via interconnect M00
connect_bd_intf_net \
    [get_bd_intf_pins axi_interconnect_0/M00_AXI] \
    [get_bd_intf_pins axi_dma_0/S_AXI_LITE]

# Connect AXI DMA clock pins explicitly (Vivado 2023.1 pin names)
foreach clk_pin {s_axi_lite_aclk m_axi_mm2s_aclk m_axi_s2mm_aclk} {
    connect_bd_net \
        [get_bd_pins ps7_0/FCLK_CLK0] \
        [get_bd_pins axi_dma_0/$clk_pin]
}
# Connect AXI DMA reset
connect_bd_net \
    [get_bd_pins rst_ps7_0_100M/peripheral_aresetn] \
    [get_bd_pins axi_dma_0/axi_resetn]

# DMA interrupt lines → PS IRQ_F2P via xlconcat
create_bd_cell -type ip \
    -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_0
set_property CONFIG.NUM_PORTS {2} [get_bd_cells xlconcat_0]
connect_bd_net \
    [get_bd_pins axi_dma_0/mm2s_introut] [get_bd_pins xlconcat_0/In0]
connect_bd_net \
    [get_bd_pins axi_dma_0/s2mm_introut]  [get_bd_pins xlconcat_0/In1]
connect_bd_net \
    [get_bd_pins xlconcat_0/dout]          [get_bd_pins ps7_0/IRQ_F2P]

# DMA memory ports (MM2S read + S2MM write) → PS HP0
# AXI SmartConnect arbitrates the two DMA masters onto the single HP0 slave
step "  Connecting DMA memory ports to HP0 via SmartConnect..."
create_bd_cell -type ip \
    -vlnv xilinx.com:ip:smartconnect:1.0 axi_smc_0
set_property CONFIG.NUM_SI {2} [get_bd_cells axi_smc_0]

connect_bd_net \
    [get_bd_pins ps7_0/FCLK_CLK0] [get_bd_pins axi_smc_0/aclk]
# aresetn pin exists on newer SmartConnect versions — skip silently if absent
catch {
    connect_bd_net \
        [get_bd_pins rst_ps7_0_100M/peripheral_aresetn] \
        [get_bd_pins axi_smc_0/aresetn]
}
connect_bd_intf_net \
    [get_bd_intf_pins axi_dma_0/M_AXI_MM2S] \
    [get_bd_intf_pins axi_smc_0/S00_AXI]
connect_bd_intf_net \
    [get_bd_intf_pins axi_dma_0/M_AXI_S2MM] \
    [get_bd_intf_pins axi_smc_0/S01_AXI]
connect_bd_intf_net \
    [get_bd_intf_pins axi_smc_0/M00_AXI] \
    [get_bd_intf_pins ps7_0/S_AXI_HP0]


# ------------------------------------------------------------
#  5. FIR top — module reference
#     Vivado infers AXI interfaces from port-name prefixes:
#       s_axi_*  → AXI4-Lite slave  "s_axi"
#       s_axis_* → AXI4-Stream slave "s_axis"
#       m_axis_* → AXI4-Stream master "m_axis"
# ------------------------------------------------------------
step "  Adding fir_top module reference..."
create_bd_cell -type module -reference fir_top fir_top_0

# Single clock and reset for the entire FIR IP
connect_bd_net \
    [get_bd_pins ps7_0/FCLK_CLK0] \
    [get_bd_pins fir_top_0/aclk]
connect_bd_net \
    [get_bd_pins rst_ps7_0_100M/peripheral_aresetn] \
    [get_bd_pins fir_top_0/aresetn]

# AXI-Lite control ← GP0 via interconnect M01
connect_bd_intf_net \
    [get_bd_intf_pins axi_interconnect_0/M01_AXI] \
    [get_bd_intf_pins fir_top_0/s_axi]

# AXI4-Stream data path:
#   DMA MM2S (memory → stream) → FIR input
#   FIR output → DMA S2MM (stream → memory)
connect_bd_intf_net \
    [get_bd_intf_pins axi_dma_0/M_AXIS_MM2S] \
    [get_bd_intf_pins fir_top_0/s_axis]
connect_bd_intf_net \
    [get_bd_intf_pins fir_top_0/m_axis] \
    [get_bd_intf_pins axi_dma_0/S_AXIS_S2MM]


# ============================================================
#  Address assignment
# ============================================================
step "Assigning address map..."

# Pin AXI DMA control registers to 0x40400000 BEFORE calling
# assign_bd_address.  Creating the segment explicitly is reliable
# across Vivado versions; it avoids the fragile post-assign name search.
#
# Slave segment path format: <cell>/<interface>/<seg_name>
# AXI DMA S_AXI_LITE register map is always named "Reg" in Xilinx IPs.
step "  Pre-pinning DMA control at 0x40400000 (64K)..."
if {[catch {
    create_bd_addr_seg \
        -range 0x00010000 -offset 0x40400000 \
        [get_bd_addr_spaces ps7_0/Data]      \
        [get_bd_addr_segs  axi_dma_0/S_AXI_LITE/Reg] \
        SEG_axi_dma_0_Reg
    step "  DMA segment pinned at 0x40400000."
} err]} {
    puts "WARN: create_bd_addr_seg failed — will retry after assign_bd_address"
    puts "WARN: ($err)"
    set _dma_pin_fallback 1
}

# Auto-assign every remaining unmapped segment (FIR AXI-Lite, DMA HP0, etc.)
assign_bd_address

# Fallback: if pre-pinning failed, use the conventional hardcoded segment name
# (same pattern as the template project — works when the name is predictable).
if {[info exists _dma_pin_fallback]} {
    unset _dma_pin_fallback
    assign_addr {ps7_0/Data/SEG_axi_dma_0_Reg} 0x40400000 64K
}

# FIR AXI-Lite: intentionally NOT pinned — Vivado assigns freely in GP0.
# SW accesses it via UIO mmap (MMU handles physical→virtual), so the
# physical address is irrelevant to both RTL and application code.

step "Address map:"
foreach seg [get_bd_addr_segs] {
    puts "  [get_property OFFSET $seg]\t[get_property RANGE $seg]\t$seg"
}


# ============================================================
#  Validate and save block design
# ============================================================
step "Validating block design..."
validate_bd_design
save_bd_design
step "Block design '$CFG(bd_name)' saved."


# ============================================================
#  HDL Wrapper — explicit top, no alias mapping
# ============================================================
step "Creating HDL wrapper ($CFG(top_module))..."
make_wrapper -files [get_files $CFG(bd_name).bd] -top

# Wrapper lands in .srcs (Vivado ≤2022) or .gen (Vivado ≥2022.2)
set wrapper_candidates [concat \
    [glob -nocomplain \
        $CFG(proj_dir)/$CFG(proj_name)/$CFG(proj_name).srcs/sources_1/bd/$CFG(bd_name)/hdl/*.v] \
    [glob -nocomplain \
        $CFG(proj_dir)/$CFG(proj_name)/$CFG(proj_name).gen/sources_1/bd/$CFG(bd_name)/hdl/*.v]]

if {[llength $wrapper_candidates] == 0} {
    die "HDL wrapper not found after make_wrapper.\
\n       Check $CFG(proj_dir)/$CFG(proj_name) for *_wrapper.v"
}
set wrapper_file [lindex $wrapper_candidates 0]
step "Wrapper: $wrapper_file"
add_files -norecurse $wrapper_file

# Explicit top module — no alias mapping (per project convention)
set_property top         $CFG(top_module) [current_fileset]
set_property top_lib     xil_defaultlib   [current_fileset]
update_compile_order -fileset sources_1


# ============================================================
#  Synthesis
# ============================================================
step "Launching synthesis (jobs=$CFG(jobs))..."
launch_runs synth_1 -jobs $CFG(jobs)
wait_on_run synth_1

if {[get_property PROGRESS [get_runs synth_1]] ne "100%"} {
    die "Synthesis failed. Check synth_1 log:\
\n       $CFG(proj_dir)/$CFG(proj_name)/$CFG(proj_name).runs/synth_1/runme.log"
}
step "Synthesis complete."


# ============================================================
#  Implementation + Bitstream
# ============================================================
step "Launching implementation and bitstream (jobs=$CFG(jobs))..."
launch_runs impl_1 -to_step write_bitstream -jobs $CFG(jobs)
wait_on_run impl_1

if {[get_property PROGRESS [get_runs impl_1]] ne "100%"} {
    die "Implementation failed. Check impl_1 log:\
\n       $CFG(proj_dir)/$CFG(proj_name)/$CFG(proj_name).runs/impl_1/runme.log"
}
step "Implementation and bitstream complete."


# ============================================================
#  Export XSA
# ============================================================
step "Exporting hardware platform (XSA)..."
write_hw_platform -fixed -include_bit -force $CFG(xsa_path)

step "Done."
step "XSA written to: $CFG(xsa_path)"
step "Next step: petalinux-config --get-hw-description $CFG(xsa_path)"
