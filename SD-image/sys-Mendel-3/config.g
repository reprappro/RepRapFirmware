; Configuration file for RepRap Mendel 3
; RepRapPro Ltd
;
; Copy this file to config.g if you have a Mendel 3
; If you are updating a config.g that you already have you
; may wish to go through it and this file checking what you
; want to keep from your old file.
; 
; For G-code definitions, see http://reprap.org/wiki/G-code
; 
M111 S0                             ; Debug off
M550 PRepRapPro Mendel 3            ; Machine name (can be anything you like). With DHCP enabled connect to (example) http://reprappromendel3 (machine name with no spaces).
M551 Preprap                        ; Machine password (currently not used)
M540 P0xBE:0xEF:0xDE:0xAD:0xFE:0x14 ; MAC Address
;M552 P0.0.0.0                       ; Un-comment for DHCP
M552 P192.168.1.14                  ; IP address, comment for DHCP
M553 P255.255.255.0                 ; Netmask
M554 P192.168.1.1                   ; Gateway, comment for DHCP
M555 P2                             ; Set output to look like Marlin
G21                                 ; Work in millimetres
G90                                 ; Send absolute corrdinates...
M83                                 ; ...but relative extruder moves
M906 X800 Y1000 Z800 E800           ; Set motor currents (mA)
M305 P0 R4700                       ; Set the heated bed thermistor series resistor to 4K7
M305 P1 R4700                       ; Set the hot end thermistor series resistor to 4K7
M569 P0 S1                          ; Reverse the X motor
M569 P3 S0                          ; Reverse the extruder motor (T0)
M569 P4 S0                          ; Reverse the extruder motor (T1)
M569 P5 S0                          ; Reverse the extruder motor (T2)
M92 E660                            ; Set extruder steps per mm
M558 P2                             ; Use a modulated Z probe
G31 Z0.8 P600                       ; Set the probe height and threshold (deliberately too high to avoid bed crashes on initial setup)
M556 S75 X0 Y0 Z0                   ; Put your axis compensation here
M201 X500 Y500 Z15 E500             ; Accelerations (mm/s^2)  
M203 X15000 Y15000 Z100 E3600       ; Maximum speeds (mm/min)
M566 X200 Y200 Z30 E20              ; Minimum speeds mm/minute    
M563 P0 D0 H1                       ; Define tool 0
G10 P0 S-273 R-273                  ; Set tool 0 operating and standby temperatures
;M563 P1 D1 H2                       ; Define tool 1, uncomment if you have a dual colour upgrade
;G10 P1 S-273 R-273                  ; Set tool 1 operating and standby temperatures, uncomment if you have a dual colour upgrade
;M563 P2 D2 H3                       ; Define tool 2, uncomment if you have a tri colour upgrade
;G10 P2 S-273 R-273                  ; Set tool 2 operating and standby temperatures, uncomment if you have a dual colour upgrade
