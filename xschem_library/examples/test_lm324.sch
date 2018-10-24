G {} 
V {} 
S {} 
T {LM324 OPAMP EXAMPLE} 450 -510 0 0 0.4 0.4 {} 
N 520 -340 560 -340 {lab=P} 
N 640 -400 640 -370 {lab=VCC} 
N 640 -230 640 -200 {lab=VSS} 
N 770 -300 960 -300 {lab=Q} 
N 900 -300 900 -280 {lab=Q} 
N 820 -300 820 -280 {lab=Q} 
N 770 -300 770 -150 {lab=Q} 
N 560 -150 770 -150 {lab=Q} 
N 560 -260 560 -150 {lab=Q} 
C {devices/code} 320 -210 0 0 {name=STIMULI value="

.option RUNLVL=5 post
.param VCC=10
.param VSS=0
.temp  25
vvcc vcc 0 dc VCC
vvss vss 0 dc VSS
vp p 0 pwl 0 0 100n 2 200u 2 201u 3 400u 3 401u 1

.tran 0.1u 500u
"} 
C {examples/lm324} 640 -300 0 0 {name=x1} 
C {devices/lab_pin} 640 -200 0 0 {name=p10 lab=VSS} 
C {devices/lab_pin} 640 -400 0 0 {name=p1 lab=VCC} 
C {devices/lab_pin} 960 -300 0 1 {name=p2 lab=Q} 
C {devices/lab_pin} 520 -340 0 0 {name=p3 lab=P} 
C {devices/res} 900 -250 0 0 {name=r1 m=1 value=10k} 
C {devices/capa} 820 -250 0 0 {name=c1 m=1 value=1n} 
C {devices/lab_pin} 820 -220 0 0 {name=p5 lab=VSS} 
C {devices/lab_pin} 900 -220 0 0 {name=p6 lab=VSS} 
C {devices/title} 160 -30 0 0 {name=l1 author="Stefan Schippers"} 