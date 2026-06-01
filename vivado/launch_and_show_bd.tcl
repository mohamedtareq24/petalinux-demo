##############################################################
# launch_and_show_bd.tcl
#
# Runs the full create_fir_project flow, then opens the block
# design in the Vivado GUI for inspection.
##############################################################

set SCRIPT_DIR [file dirname [file normalize [info script]]]

# Run the full flow (creates project, runs synth/impl/bitstream/XSA)
source [file join $SCRIPT_DIR "create_fir_project.tcl"]

# Open the block design in the GUI
open_bd_design [get_files system.bd]
regenerate_bd_layout

puts "INFO: Block design is now open in the canvas."
