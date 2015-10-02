G91
G1 Z5 F200
G90
;M558 P1 ; uncomment this line if you upgrade to a 4-wire probe
G1 Y250 X-240 F2000 S1
G91
G1 X3 Y-3 F6000
G1 X-10 Y10 F200 S1
G90
;M558 P2 ; uncomment this line if you upgrade to a 4-wire probe
G1 X40 Y40 F6000
G30
G1 Z5
