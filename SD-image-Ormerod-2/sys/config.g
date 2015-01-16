; Configuration file for RepRap Ormerod 2
; RepRapPro Ltd
;
; Copy this file to config.g if you have an Ormerod 2
; If you are updating a config.g that you already have you
; may wish to go through it and this file checking what you
; want to keep from your old file.
; 
M111 S0                             ; Debug off
M550 PRepRapPro Ormerod 2           ; Machine name (can be anything you like)
M551 Preprap                        ; Machine password (currently not used)
M540 PBE:EF:DE:AD:FE:ED 				; MAC Address
M552 P192.168.1.14                  ; IP address
M553 P255.255.255.0                 ; Netmask
M554 P192.168.1.1                   ; Gateway
M555 P2                             ; Set output to look like Marlin
G21                                 ; Work in millimetres
G90                                 ; Send absolute corrdinates...
M83                                 ; ...but relative extruder moves
M906 X800 Y800 Z800 E800				; Set motor currents (mA)
M569 P0 S0									; Reverse the X motor
M92 E420                            ; Set extruder steps per mm
M558 P2                             ; Use a modulated Z probe
G31 Z0.5 P542                       ; Set the probe height and threshold (deliberately too high to avoid bed crashes on initial setup)
M556 S78 X0 Y0 Z0                   ; Put your axis compensation here
M201 X500 Y500 Z15 E500             ; Accelerations (mm/s^2)           
M203 X15000 Y15000 Z300 E3600       ; Maximum speeds (mm/min)      
M566 X1200 Y1200 Z30 E1200          ; Minimum speeds mm/minute 
M563 P1 D0 H1                       ; Define tool 1
G10 P1 S-273 R-273                  ; Set tool 1 operating and standby temperatures
M563 P127 H1 D0:1:2:3:4                ; Define tool 127 to allow the web interface to control all drives

