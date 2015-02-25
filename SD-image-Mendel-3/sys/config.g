; Configuration file for RepRap Mendel 3
; RepRapPro Ltd
;
; Copy this file to config.g if you have a Mendel 3
; If you are updating a config.g that you already have you
; may wish to go through it and this file checking what you
; want to keep from your old file.
; 
M111 S0                             ; Debug off
M550 PRepRapPro Mendel 3            ; Machine name (can be anything you like)
M551 Preprap                        ; Machine password (currently not used)
M540 P0xBE:0xEF:0xDE:0xAD:0xFE:0x14 ; MAC Address
M552 P192.168.1.14                  ; IP address
M553 P255.255.255.0                 ; Netmask
M554 P192.168.1.1                   ; Gateway
M555 P2                             ; Set output to look like Marlin
G21                                 ; Work in millimetres
G90                                 ; Send absolute corrdinates...
M83                                 ; ...but relative extruder moves
M906 X800 Y1000 Z800 E800            ; Set motor currents (mA)
;M305 P0 R4700                      ; Set the heated bed thermistor series resistor to 4K7
;M305 P1 R4700                      ; Set the hot end thermistor series resistor to 4K7
M569 P0 S1                          ; Reverse the X motor
M569 P3 S0                          ;
M92 E660                            ; Set extruder steps per mm
M558 P2                             ; Use a modulated Z probe
G31 Z0.6 P550                       ; Set the probe height and threshold (deliberately too high to avoid bed crashes on initial setup)
M557 P0 X90 Y0                      ; Four... 
M557 P1 X90 Y200                    ; ...probe points...
M557 P2 X260 Y200                   ; ...for bed...
M557 P3 X260 Y0                     ; ...levelling
M556 S78 X0 Y0 Z0                   ; Put your axis compensation here
M201 X3000 Y3000 Z150 E500          ; Accelerations (mm/s^2)  
M203 X15000 Y15000 Z100 E3600       ; Maximum speeds (mm/min)
M566 X1200 Y1200 Z30 E1200          ; Minimum speeds mm/minute    
M563 P1 D0 H1                       ; Define tool 1
G10 P1 S-273 R-273                  ; Set tool 1 operating and standby temperatures
