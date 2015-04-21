G91               ; set movement to relative mode
G1 Z5 F200        ; move Z up 5mm
G90               ; set movement to absolute mode
M558 P1           ; set probe to unmodulated mode
G1 X-240 F2000 S1 ; home X
G92 X0            ; set position to X=0
G1 X3 F200        ; move axis away from X=0
G1 X-5 F200 S1    ; home again, slower
G92 X0            ; set position to X=0
G1 X15 F2000      ; ADJUST the X value to put the nozzle on the edge of the bed
G92 X0            ; set position to X=0
M558 P2           ; set probe to modulated mode
G1 X45 F2000      ; move X, ADJUST the X value to get Z probe over target
G92 Y0            ; set position to Y=0
G1 Y-240 F2000 S1 ; home Y
G92 Y0            ; set position to Y=0
G1 Y3 F200        ; move Y axis out
G1 Y-5 F200 S1    ; home Y, slower
G92 Y0            ; set position to Y=0
G1 Y3 F2000       ; ADJUST the Y value to put the nozzle on the edge of the bed
G92 Y0            ; set position to Y=0
G1 Y0 F2000       ; move Y, ADJUST the Y value to get Z probe over target
G30               ; home Z, using values from G31 in config.g
G1 Z5 F200        ; Move Z up to Z=5
G1 X0 Y0 F2000    ; Move to X=0 Y=0 
G1 Z0 F200        ; Move to Z=0
