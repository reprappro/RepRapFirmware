; Configuration file for RepRap Ormerod 2
; RepRapPro Ltd
;
; Copy this file to config.g if you have an Ormerod 2
; If you are updating a config.g that you already have you
; may wish to go through it and this file checking what you
; want to keep from your old file.
; 
M111 S0                             ; Debug off
M550 PMy RepRapPro Ormerod 2        ; Machine name (can be anything you like)
M551 Preprap                        ; Machine password (currently not used)
M540 P0xBE:0xEF:0xDE:0xAD:0xFE:0xED ; MAC Address
M552 P192.168.1.14                  ; IP address
M553 P255.255.255.0                 ; Netmask
M554 P192.168.1.1                   ; Gateway
M555 P2                             ; Set output to look like Marlin
G21                                 ; Work in millimetres
G90                                 ; Send absolute corrdinates...
M83                                 ; ...but relative extruder moves
M906 X800 Y1000 Z800 E800           ; Set motor currents (mA)
;M305 P0 R4700                       ; Set the heated bed thermistor series resistor to 4K7
;M305 P1 R4700                       ; Set the hot end thermistor series resistor to 4K7
M569 P0 S0                          ; Reverse the X motor
M92 E420                            ; Set extruder steps per mm
M558 P2                             ; Use a modulated Z probe
G31 Z0.8 P600                       ; Set the probe height and threshold (deliberately too high to avoid bed crashes on initial setup)
M557 P0 X45 Y0                      ; Four... 
M557 P1 X45 Y190                    ; ...probe points...
M557 P2 X215 Y190                   ; ...for bed...
M557 P3 X215 Y0                     ; ...levelling
M556 S78 X0 Y0 Z0                   ; Put your axis compensation here
M201 X500 Y500 Z15 E500             ; Accelerations (mm/s^2)
M203 X15000 Y15000 Z100 E3600       ; Maximum speeds (mm/min)
M566 X1200 Y1200 Z30 E1200          ; Minimum speeds mm/minute
M563 P1 D0 H1                       ; Define tool 1
G10 P1 S-273 R-273                  ; Set tool 1 operating and standby temperatures

