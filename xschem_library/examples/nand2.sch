G {} 
V {} 
S {} 
N 290 -280 290 -220 {lab=#net1} 
N 290 -410 290 -340 {lab=#net2} 
N 290 -380 440 -380 {lab=#net2} 
N 440 -410 440 -380 {lab=#net2} 
N 440 -500 440 -470 {lab=#net3} 
N 290 -500 440 -500 {lab=#net3} 
N 290 -500 290 -470 {lab=#net3} 
N 290 -160 290 -130 {lab=#net4} 
N 210 -190 250 -190 {lab=#net5} 
N 400 -440 400 -400 {lab=#net6} 
N 240 -400 400 -400 {lab=#net6} 
N 240 -400 240 -310 {lab=#net6} 
N 210 -440 210 -190 {lab=#net5} 
N 210 -440 250 -440 {lab=#net5} 
N 160 -310 250 -310 {lab=#net6} 
N 160 -190 210 -190 {lab=#net5} 
N 440 -380 500 -380 {} 
C {ipin} 160 -310 0 0 {name=p1 lab=A} 
C {opin} 500 -380 0 0 {name=p2 lab=Z} 
C {ipin} 160 -190 0 0 {name=p3 lab=B} 
C {nmos4} 270 -310 0 0 {name=m1 model=nmos w=WN l=LN m=1} 
C {pmos4} 270 -440 0 0 {name=m2 model=pmos w=WP l=LP m=1} 
C {pmos4} 420 -440 0 0 {name=m3 model=pmos w=WP l=LP m=1} 
C {nmos4} 270 -190 0 0 {name=m4 model=nmos w=WN l=LN m=1} 
C {vdd} 290 -500 0 0 {name=l1 lab=VCC} 
C {gnd} 290 -130 0 0 {name=l2 lab=VSS} 
C {lab_pin} 290 -310 0 1 {name=l3 sig_type=std_logic lab=VSS} 
C {lab_pin} 290 -190 0 1 {name=l4 sig_type=std_logic lab=VSS} 
C {lab_pin} 290 -440 0 1 {name=l5 sig_type=std_logic lab=VCC} 
C {lab_pin} 440 -440 0 1 {name=l6 sig_type=std_logic lab=VCC} 
C {title} 160 -30 0 0 {name=l7 author="Stefan Schippers"} 
