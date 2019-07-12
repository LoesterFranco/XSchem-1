
proc read_hspice_log {f} {
  global current_probe voltage
  set fd [open $f]
  while {[gets $fd line] >= 0} {
    regsub -all {\r} $line {} line
    regsub {^ *\+} $line { + } line
    if [regexp {\*\*\*\*\*\*.*operating point information} $line] {
      set vnodes 1
    }
    if [regexp {\*\*\*\*.*voltage sources} $line] {
      set vnodes 0
      set vsources 1
    }
    if [regexp {total voltage source power} $line] {
      set vsources 0
    }
    if { [lindex $line 0] == {element}  && $vsources } {
      set  vsource_name $line
    }
    if { [lindex $line 0] == {current}  && $vsources } {
      for {set i 1} {$i < [llength $line]} { incr i } {
        set vsource [lindex $vsource_name $i]
        set current [lindex $line $i]
        if {[regexp {^0:} $vsource]} {
          if {[regsub {^.*:} $vsource {} vsource]} {
            set current_probe($vsource) $current
            puts "$vsource --> $current"
          }
        }
      }
    }
    if { [lindex $line 0] == {+} && $vnodes} {
      regsub -all {=} $line { = } line
      for { set i 1} {$i < [llength $line]} { set i [expr $i+3]} {
        set node [lindex $line $i]
        set volt [lindex $line [expr $i+2]]
        if {[regsub {^0:} $node {} node]} {
          set voltage($node) $volt
          puts "$node --> $volt"
        }
      }
    }
  }
  close $fd
}

proc get_voltage { n } {
  global voltage
  if { abs($voltage([string tolower $n])) < 1e-3 } {
    return [format %.4e $voltage([string tolower $n])]
  } else {
    return [format %.4g $voltage([string tolower $n])]
  }
  # return DELETE
}

proc get_diff_voltage { p m } {
  global voltage
  return [format %.4e [expr $voltage([string tolower $p]) - $voltage([string tolower $m]) ] ]
  # return DELETE
}

proc get_current { n } {
  global current_probe
  if {abs($current_probe([string tolower $n])) <1e-3} {
    return [format %.4e $current_probe([string tolower $n])]
  } else {
    return [format %.4g $current_probe([string tolower $n])]
  }
  # return DELETE
}


proc annotate {} {
  ### disable screen redraw and undo when looping to speed up performance
  ### but save state on undo stack before doing backannotations.
  xschem push_undo
  xschem set no_undo 1
  xschem set no_draw 1
  
  read_hspice_log [xschem get netlist_dir]/hspice.out
  set lastinst [xschem get lastinst]
  for { set i 0 } { $i < $lastinst } {incr i } {
    set name [xschem getprop instance $i name]
    set type [xschem getprop instance $i cell::type]
    if { $type == "probe"  || $type == "devices/probe"} { 
      set net [xschem instance_net $i p]
      if {[catch {xschem setprop instance $i voltage [get_voltage $net] fast} err]} {
        puts "1 error : $err net: $net"
      }
    }
    if { $type == "current_probe"  || $type == "devices/current_probe"} { 
      if {[catch {xschem setprop instance $i current [get_current $name] fast} err]} {
        puts "2 error : $err"
      }
    }
    if { $type == "differential_probe"  || $type == "devices/differential_probe"} { 
      set netp [xschem instance_net $i p]
      set netm [xschem instance_net $i m]
      if {[catch {xschem setprop instance $i voltage [get_diff_voltage $netp $netm] fast} err]} {
        puts "3 error : $err"
      }
    }
    # puts "$i $name $type"
  }
  
  # re-enable undo and draw
  xschem set no_undo 0
  xschem set no_draw 0
  xschem redraw
  
  ### xschem setprop instructions have not altered circuit topology so 
  ### in this case a connectivity rebuild is not needed.
  # xschem rebuild_connectivity
  #
  #
}
