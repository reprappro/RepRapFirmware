G91               ; set movement to relative mode
G1 Z5 F200        ; move Z up 5mm
G90               ; set movement to absolute mode
G1 X30 Y0 F2000   ; move X and Y to probe point. ADJUST X and Y values to get Z probe over target.
G30               ; home Z, using values from G31 in config.g
G1 Z0 F200        ; move Z to Z=0
