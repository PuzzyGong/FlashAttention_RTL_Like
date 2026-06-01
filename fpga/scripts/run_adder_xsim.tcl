set root [file normalize [file join [file dirname [info script]] .. ..]]
set rtl  [file join $root fpga rtl adder.v]
set tb   [file join $root fpga tb adder_tb.v]

puts [exec xvlog $rtl $tb]
puts [exec xelab adder_tb -s adder_tb_sim]
puts [exec xsim adder_tb_sim -runall]
