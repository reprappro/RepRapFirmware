; Auto calibration routine for delta printers
; Before running this, you should have set up your zprobe X, Y and Z offsets to suit your build.
; This does a single iteration of auto calibration. Run this file multiple times until the values converge.
; Then transfer the values to your config.g file.

M561						; clear any bed transform, otherwise homing may be at the wrong height
M201 X500 Y500 Z500			; Set maximum acceleration for homing

; Probe the bed and do auto calibration
G1 X-64.95 Y-37.5 F12000
G4 P300
G30 P0 X-64.95 Y-37.5 Z-99999	; X tower
G4 P300
G30 P1 X64.95 Y-37.5 Z-99999	; Y tower
G4 P300
G30 P2 X0 Y75 Z-99999			; Z tower
G4 P300
G31 Z-0.3
G30 P3 X0 Y0 Z-99999 S0			; centre, and auto-calibrate
G31 Z-0.1
G1 X0 Y0 F15000

M201 X4000 Y4000 Z4000			; Reset default acceleration
