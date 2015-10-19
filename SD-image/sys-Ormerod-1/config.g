; Configuration file for RepRap Ormerod 1
; RepRapPro Ltd
;
; Copy this file to config.g if you have an Ormerod 1
; If you are updating a config.g that you already have you
; may wish to go through it and this file checking what you
; want to keep from your old file.
; 
; For G-code definitions, see http://reprap.org/wiki/G-code
; 
M111 S0                             ; Debug off
M550 PMy RepRapPro Ormerod 1        ; Machine name (can be anything you like). With DHCP enabled connect to (example) http://reprapproormerod1 (machine name with no spaces).
M551 Preprap                        ; Machine password (currently not used)
M540 P0xBE:0xEF:0xDE:0xAD:0xFE:0xED ; MAC Address
;M552 P0.0.0.0                       ; Un-comment for DHCP
M552 P192.168.1.14                  ; IP address, comment for DHCP
M553 P255.255.255.0                 ; Netmask
M554 P192.168.1.1                   ; Gateway, comment for DHCP
M555 P2                             ; Set output to look like Marlin
G21                                 ; Work in millimetres
G90                                 ; Send absolute corrdinates...
M83                                 ; ...but relative extruder moves
M574 X0 Y2 Z0 S1                    ; set endstop configuration (Y endstop only, at high end, active high)
M906 X800 Y1000 Z800 E1000          ; Set motor currents (mA)
;M305 P0 R4700 H0 L0                 ; Set the heated bed thermistor series resistor to 4K7
;M305 P1 R4700 H0 L0                 ; Set the hot end thermistor series resistor to 4K7
;M305 P2 R4700 H0 L0                 ; Set the second hot end thermistor series resistor to 4K7
M569 P0 S1                          ; Reverse the X axis
M92 E420:420                        ; Set extruder steps per mm
M558 P1                             ; Use an unmodulated Z probe
G31 Z0.8 P600                       ; Set the probe height and threshold (deliberately too high to avoid bed crashes on initial setup)
M556 S78 X0 Y0 Z0                   ; Put your axis compensation here
M201 X1000 Y1000 Z20 E1000          ; Accelerations (mm/s^2)
M203 X15000 Y15000 Z180 E3600       ; Maximum speeds (mm/min)
M566 X1800 Y1800 Z30 E20            ; Maximum instantaneous speed changes mm/minute
M208 X-24 S1                        ; Set axis minimum (adjust to make X=0 the edge of the bed)
M208 X214 Y210                      ; Set axis maxima (adjust to suit your machine)
M563 P0 D0 H1                       ; Define tool 0
G10 P0 S-273 R-273                  ; Set tool 0 operating and standby temperatures
;M563 P1 D1 H2                          ; Define tool 1 Uncomment if you have a dual colour upgrade
;G10 P1 X19 S-273 R-273                ; Set tool 1 operating and standby temperatures  Uncomment if you have a dual colour upgrade

