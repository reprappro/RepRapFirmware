G91               ; set movement to relative mode
G1 Z5 F200        ; move Z up 5mm
G1 X20 F2000      ; move X out 20mm
G90               ; set movement to absolute mode
G92 Y0            ; set position to Y=0
G1 Y-140 F2000 S1 ; home Y
G92 Y0            ; set position to Y=0
G1 Y3 F200        ; move Y axis out
G1 Y-5 F200 S1    ; home Y again, slower
G92 Y0            ; set position to Y=0
G1 Y0 F2000       ; ADJUST the Y value to put the nozzle on the edge of the bed
G92 Y0            ; set position to Y=0
G91               ; set movement to relative mode
G1 X-20 F2000     ; move X in 20mm
G1 Z-5 F200       ; move Z down 5mm
G90               ; set movement to absolute mode
