G91
G1 Z5 F200
G90
;M558 P1 ; uncomment this line if you upgrade to a 4-wire probe
G1 X-240 F2000 S1
G1 X3 F2000
G1 X-30 F200 S1
;M558 P2 ; uncomment this line if you upgrade to a 4-wire probe
G1 X45 F2000
G1 Y250 F2000 S1
G1 Y197 F2000
G1 Y250 F200 S1
G30
G1 Z5 F200
G1 X0 Y0 F6000
