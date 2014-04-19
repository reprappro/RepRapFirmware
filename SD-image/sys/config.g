; RepRapPro Ormerod 2
; Standard configuration G Codes
M111 S1; Debug on
M550 PRepRapPro Ormerod 2; Set the machine's name
M551 Preprap; Set the password
M552 P192.168.1.14; Set the IP address
M553 P255.255.255.0; Set netmask
M554 P192.168.1.1; Set the gateway
M555 P2; Emulate Marlin USB output
G21 ; Work in mm
G90 ; Absolute positioning
M83 ; Extrusions relative
M906 X800 Y1000 Z800 E800 ; Motor currents (mA)
M563 P1 D0 H1 ; Define tool 1
M92 E447 ; Set extruder steps/mm
M558 P2 ; Turn Z Probe on - modulated probe
G31 Z3 P435 ; Set Z probe height and threshold
M557 P0 X61 Y-7 ; Set bed plane probe points...
M557 P1 X61 Y173 ; ...
M557 P2 X227 Y173 ; ...
M557 P3 X227 Y-7; .
M556 S78 X0 Y0 Z0; Axis compensation

