G {name=m4 model=enmm9 m=1 w=6u l=1u} 
V {} 
S {* LM324 OPERATIONAL AMPLIFIER "MACROMODEL" SUBCIRCUIT
* CREATED USING PARTS RELEASE 4.01 ON 09/08/89 AT 10:54
* (REV N/A)      SUPPLY VOLTAGE: 5V
* CONNECTIONS:   NON-INVERTING INPUT
*                | INVERTING INPUT
*                | | POSITIVE POWER SUPPLY
*                | | | NEGATIVE POWER SUPPLY
*                | | | | OUTPUT
*                | | | | |
* .SUBCKT LM324  1 2 3 4 5
*
  C1   11 12 5.544E-12
  C2    6  7 20.00E-12
  DC    5 53 DX
  DE   54  5 DX
  DLP  90 91 DX
  DLN  92 90 DX
  DP    4  3 DX
  EGND 99  0 POLY(2) (3,0) (4,0) 0 .5 .5
  FB    7 99 POLY(5) VB VC VE VLP VLN 0 15.91E6 -20E6 20E6 20E6 -20E6
  GA    6  0 11 12 125.7E-6
  GCM   0  6 10 99 7.067E-9
  IEE   3 10 DC 10.04E-6
  HLIM 90  0 VLIM 1K
  Q1   11  2 13 QX
  Q2   12  1 14 QX
  R2    6  9 100.0E3
  RC1   4 11 7.957E3
  RC2   4 12 7.957E3
  RE1  13 10 2.773E3
  RE2  14 10 2.773E3
  REE  10 99 19.92E6
  RO1   8  5 50
  RO2   7 99 50
  RP    3  4 30.31E3
  VB    9  0 DC 0
  VC 3 53 DC 2.100
  VE   54  4 DC .6
  VLIM  7  8 DC 0
  VLP  91  0 DC 40
  VLN   0 92 DC 40
.MODEL DX D(IS=800.0E-18)
.MODEL QX PNP(IS=800.0E-18 BF=250)
* .ENDS} 
C {ipin} 580 -210 0 0 {name=p1 lab=1} 
C {ipin} 580 -190 0 0 {name=p2 lab=2} 
C {ipin} 580 -170 0 0 {name=p3 lab=3} 
C {ipin} 580 -150 0 0 {name=p4 lab=4} 
C {opin} 720 -190 0 0 {name=p5 lab=5} 
C {title} 160 -30 0 0 {name=l1 author="Stefan Schippers"} 
