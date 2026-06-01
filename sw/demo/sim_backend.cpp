// sim_backend.cpp — Verilator lifecycle for x86 simulation.
//
// The globals 'top' and 'tick()' must have external linkage because:
//   - mock_axidma.cpp   declares:  extern Vfir_top* top; extern void tick();
//   - axilite_ctrl.cpp  declares:  extern Vfir_top *top; extern void tick();

#include "backend.h"

#include "Vfir_top.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

// ---- externally visible ----------------------------------------------------
Vfir_top      *top       = nullptr;   // used by mock_axidma & axilite_ctrl

static VerilatedVcdC *g_trace    = nullptr;
static vluint64_t      g_sim_time = 0;

// Required by the Verilated FST infrastructure.
double sc_time_stamp() { return static_cast<double>(g_sim_time); }

void tick()
{
    top->aclk = 0;
    top->eval();
    if (g_trace) g_trace->dump(g_sim_time);
    ++g_sim_time;

    top->aclk = 1;
    top->eval();
    if (g_trace) g_trace->dump(g_sim_time);
    ++g_sim_time;
}
// ----------------------------------------------------------------------------

bool backend_init(int argc, char *argv[])
{
    Verilated::commandArgs(argc, argv);

    top = new Vfir_top;

    Verilated::traceEverOn(true);
    g_trace = new VerilatedVcdC;
    top->trace(g_trace, 99);
    g_trace->open("fir_test.vcd");

    // Reset sequence: hold aresetn low for 8 cycles.
    top->aclk          = 0;
    top->aresetn       = 0;
    top->s_axis_tvalid = 0;
    top->s_axis_tlast  = 0;
    top->s_axis_tdata  = 0;
    top->m_axis_tready = 0;
    top->eval();

    for (int i = 0; i < 8; ++i)
        tick();

    top->aresetn = 1;
    for (int i = 0; i < 4; ++i)
        tick();

    return true;
}

void backend_shutdown()
{
    if (top) {
        top->final();
        delete top;
        top = nullptr;
    }
    if (g_trace) {
        g_trace->close();
        delete g_trace;
        g_trace = nullptr;
    }
}
