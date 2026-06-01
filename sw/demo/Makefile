# Makefile — FIR filter DMA verification
#
# Targets:
#   make sim    — build fir_test_sim (x86, Verilator)
#   make hw     — build fir_test_hw  (aarch64, PetaLinux cross-compile)
#   make clean  — remove build artefacts

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
SW_DIR          := ../verialtor
OBJ_DIR         := ../obj_dir
VERILATOR_ROOT  ?= $(shell verilator --getenv VERILATOR_ROOT 2>/dev/null || echo /usr/local/share/verilator)
VERILATOR_CMD   ?= verilator
VERILATOR_INC   := $(VERILATOR_ROOT)/include

CROSS_COMPILE   := aarch64-linux-gnu-

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
COMMON_SRCS := main.cpp \
               $(SW_DIR)/axilite_ctrl.cpp

SIM_SRCS := $(COMMON_SRCS) \
            sim_backend.cpp \
            $(SW_DIR)/mock_axidma.cpp \
            $(OBJ_DIR)/Vfir_top.cpp \
            $(OBJ_DIR)/Vfir_top__Syms.cpp \
            $(OBJ_DIR)/Vfir_top___024root__DepSet_h7b40a2f7__0.cpp \
            $(OBJ_DIR)/Vfir_top___024root__DepSet_h7b40a2f7__0__Slow.cpp \
            $(OBJ_DIR)/Vfir_top___024root__DepSet_h9d0ba783__0.cpp \
            $(OBJ_DIR)/Vfir_top___024root__DepSet_h9d0ba783__0__Slow.cpp \
            $(OBJ_DIR)/Vfir_top___024root__Slow.cpp \
            $(OBJ_DIR)/Vfir_top__TraceDecls__0__Slow.cpp \
            $(OBJ_DIR)/Vfir_top__Trace__0.cpp \
            $(OBJ_DIR)/Vfir_top__Trace__0__Slow.cpp \
            $(VERILATOR_INC)/verilated.cpp \
            $(VERILATOR_INC)/verilated_vcd_c.cpp \
            $(VERILATOR_INC)/verilated_fst_c.cpp \
            $(VERILATOR_INC)/verilated_threads.cpp

SIM_DEPS := $(COMMON_SRCS) \
            sim_backend.cpp \
            $(SW_DIR)/mock_axidma.cpp \
            $(SW_DIR)/libaxidma.h \
            $(SW_DIR)/axilite_ctrl.h \
            backend.h

HW_SRCS := $(COMMON_SRCS) \
           hw_backend.cpp

# ---------------------------------------------------------------------------
# Flags
# ---------------------------------------------------------------------------
SIM_CXXFLAGS := -std=c++17 -DSIMULATION \
                -I. \
                -I$(SW_DIR) \
                -I$(OBJ_DIR) \
                -I$(VERILATOR_INC) \
                -I$(VERILATOR_INC)/vltstd \
                -Wall -Wextra -Wno-unused-parameter

HW_CXXFLAGS  := -std=c++17 \
                -I. \
                -I$(SW_DIR) \
                -Wall -Wextra -Wno-unused-parameter

HW_LDFLAGS   := -laxidma

# ---------------------------------------------------------------------------
# Rules
# ---------------------------------------------------------------------------
.PHONY: sim hw clean gen-model

sim: gen-model fir_test_sim

hw: fir_test_hw

gen-model: $(OBJ_DIR)
	$(VERILATOR_CMD) -Wall -sv -Wno-fatal --cc ../../rtl/fir_top.v \
		../../rtl/my_fir_v1_0.v \
		../../rtl/my_fir_v1_0_S_AXI.sv \
		../../rtl/my_fir_v1_0_S_AXIS.sv \
		../../rtl/my_fir_v1_0_M_AXIS.sv \
		../../rtl/FIR_transposed.sv \
		../../rtl/transposed_block.v \
		--top-module fir_top -Mdir $(OBJ_DIR) --trace

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

fir_test_sim: $(SIM_DEPS)
	g++ $(SIM_CXXFLAGS) $(SIM_SRCS) -lpthread -lz -o $@

fir_test_hw: $(HW_SRCS) $(SW_DIR)/libaxidma.h $(SW_DIR)/axilite_ctrl.h backend.h
	$(CROSS_COMPILE)g++ $(HW_CXXFLAGS) $(HW_SRCS) $(HW_LDFLAGS) -o $@

clean:
	rm -f fir_test_sim fir_test_hw fir_test.vcd
