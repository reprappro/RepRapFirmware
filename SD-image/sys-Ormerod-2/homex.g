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
G91               ; set movement to relative mode
G1 Z-5 F200       ; move Z down 5mm
G90               ; set movement to absolute mode
