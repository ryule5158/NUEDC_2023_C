# 运行AD9280到ProMax实时数据面的自检查仿真。

set script_path [info script]
if {[file pathtype $script_path] eq "absolute"} {
    set script_dir [file dirname $script_path]
} else {
    set script_dir [file join [pwd] [file dirname $script_path]]
}
set root_dir [file dirname $script_dir]
set project_dir [file join $root_dir "Vivado" "Simulation" "promax_sim"]
file mkdir $project_dir
set testbench [file join $root_dir "sim" "tb_ad9280_promax_core.sv"]
set rtl_files [concat \
    [list [file join $root_dir "rtl" "ad9280_promax_core.sv"]] \
    [glob [file join $root_dir "rtl" "promax" "*.sv"]]]

create_project -force promax_sim $project_dir -part xc7z020clg400-2
set_property target_language Verilog [current_project]
set_property simulator_language Mixed [current_project]
set_property XPM_LIBRARIES {XPM_FIFO} [current_project]
add_files -norecurse $rtl_files
add_files -fileset sim_1 -norecurse $testbench
set_property file_type SystemVerilog [get_files [concat $rtl_files [list $testbench]]]
set_property top tb_ad9280_promax_core [get_filesets sim_1]
set_property xsim.simulate.runtime {all} [get_filesets sim_1]
update_compile_order -fileset sources_1
update_compile_order -fileset sim_1
launch_simulation -mode behavioral
close_sim
close_project
