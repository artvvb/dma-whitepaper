# recreate the project from a clean repo
set script_dir [file normalize [file dirname [info script]]]
set repo_path [file dirname $script_dir]
set vivado_version [version -short]
set build_when_checked_out 0
set wait_on_build 0

set vivado_year [lindex [split $vivado_version "."] 0]
set proj_name [file rootname [file tail $xpr_path]]
set xpr_path [file join $script_dir ${proj_name}.xpr]
set info_script [file join $script_dir project_info.tcl]

puts "INFO: Creating new project \"$proj_name\" in [file dirname $xpr_path]"

# Create project
create_project $proj_name [file dirname $xpr_path]

source $info_script

# Capture board information for the project
puts "INFO: Capturing board information from $info_script"
set_project_properties_post_create_project $proj_name
set obj [get_projects $proj_name]
set part_name [get_property "part" $obj]

# Uncomment the following 3 lines to greatly increase build speed while working with IP cores (and/or block diagrams)
puts "INFO: Configuring project IP handling properties"
set_property "corecontainer.enable" "0" $obj
set_property "ip_cache_permissions" "read write" $obj
set_property "ip_output_repo" "[file normalize "$script_dir/cache"]" $obj

# Create 'sources_1' fileset (if not found)
if {[string equal [get_filesets -quiet sources_1] ""]} {
    puts "INFO: Creating sources_1 fileset"
    create_fileset -srcset sources_1
}

# Create 'constrs_1' fileset (if not found)
if {[string equal [get_filesets -quiet constrs_1] ""]} {
    puts "INFO: Creating constrs_1 fileset"
    create_fileset -constrset constrs_1
}

# Capture project-specific IP settings
puts "INFO: capturing IP-related settings from $info_script"
set_project_properties_pre_add_repo $proj_name

# # Set IP repository paths
# puts "INFO: Setting IP repository paths"
# set obj [get_filesets sources_1]
# set_property "ip_repo_paths" "[file normalize $script_dir/repo]" $obj

# Refresh IP Repositories
puts "INFO: Refreshing IP repositories"
update_ip_catalog -rebuild

# Add hardware description language sources
puts "INFO: Adding HDL sources"
add_files -quiet [file join $script_dir ${proj_name}.srcs sources_1]

# # Add constraints
# puts "INFO: Adding constraints"
# add_files -quiet -norecurse -fileset constrs_1 $script_dir/src/constraints

# Recreate block design
# TODO: handle multiple block designs
set ipi_tcl_files [glob -nocomplain [file join ${proj_name}.gen sources_1 bd * hw_handoff *.tcl]]

# Use TCL script to rebuild block design
puts "INFO: Rebuilding block design from script"
# # Create local source directory for bd
# if {[file exist "[file rootname $xpr_path].srcs"] == 0} {
# 	file mkdir "[file rootname $xpr_path].srcs"
# }
# if {[file exist "[file rootname $xpr_path].srcs/sources_1"] == 0} {
# 	file mkdir "[file rootname $xpr_path].srcs/sources_1"
# }
# if {[file exist "[file rootname $xpr_path].srcs/sources_1/bd"] == 0} {
# 	file mkdir "[file rootname $xpr_path].srcs/sources_1/bd"
# }

# Force Non-Remote BD Flow
set origin_dir [pwd]
cd [file join [file rootname $xpr_path].srcs sources_1]
set run_remote_bd_flow 0
if {[set result [catch { source [lindex $ipi_tcl_files 0] } resulttext]]} {
	# remember global error state
	set einfo $::errorInfo
	set ecode $::errorCode
	catch {cd $origin_dir}
	return -code $result -errorcode $ecode -errorinfo $einfo $resulttext
}
cd $origin_dir

# Make sure IPs are upgraded to the most recent version
foreach ip [get_ips -filter "IS_LOCKED==1"] {
    upgrade_ip -vlnv [get_property UPGRADE_VERSIONS $ip] $ip
    export_ip_user_files -of_objects $ip -no_script -sync -force -quiet
}

# Generate the wrapper for the root design
catch {
	# catch block prevents projects without a block design from erroring at this step
	set bd_name [get_bd_designs -of_objects [get_bd_cells /]]
	set bd_file [get_files $bd_name.bd]
	set wrapper_file [make_wrapper -files $bd_file -top -force]
	import_files -quiet -force -norecurse $wrapper_file

	set obj [get_filesets sources_1]
	set_property "top" "${bd_name}_wrapper" $obj
}

# Create 'synth_1' run (if not found)
if {[string equal [get_runs -quiet synth_1] ""]} {
    puts "INFO: Creating synth_1 run"
    create_run -name synth_1 -part $part_name -flow {Vivado Synthesis $vivado_year} -strategy "Vivado Synthesis Defaults" -constrset constrs_1
} else {
    set_property strategy "Vivado Synthesis Defaults" [get_runs synth_1]
    set_property flow "Vivado Synthesis $vivado_year" [get_runs synth_1]
}
puts "INFO: Configuring synth_1 run"
set obj [get_runs synth_1]
set_property "part" $part_name $obj

# Set the current synth run
puts "INFO: Setting current synthesis run"
current_run -synthesis [get_runs synth_1]

# Create 'impl_1' run (if not found)
if {[string equal [get_runs -quiet impl_1] ""]} {
    puts "INFO: Creating impl_1 run"
    create_run -name impl_1 -part $part_name -flow {Vivado Implementation $vivado_year} -strategy "Vivado Implementation Defaults" -constrset constrs_1 -parent_run synth_1
} else {
    set_property strategy "Vivado Implementation Defaults" [get_runs impl_1]
    set_property flow "Vivado Implementation $vivado_year" [get_runs impl_1]
}
puts "INFO: Configuring impl_1 run"
set obj [get_runs impl_1]
set_property "part" $part_name $obj

# Set the current impl run
puts "INFO: Setting current implementation run"
current_run -implementation [get_runs impl_1]

# Capture project-specific IP settings
puts "INFO: capturing run settings from $info_script"
set_project_properties_post_create_runs $proj_name

puts "INFO: Project created: [file tail $proj_name]"
puts "INFO: Exiting digilent_vivado_checkout"