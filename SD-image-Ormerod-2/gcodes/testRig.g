;THIS IS THE GCODE FOR THE HOT END TEST RIG
;
;
	G1 Z15 F200			;Move Z axis up
	G1 X100 Y100 F2000	;Move head to centre of bed
	G10 P1 S0 R0		;Set temperature to 0
	T1					;Select hot end
	M116				;Wait for hot end to get to 0
	M302				;Ignore cold extrusion safety
	G1 E405	F200		;Extrude filament to 10mm before end of nozzle
	G1 E10	F50			;Extrude filament slowly to end of nozzle
	G1 E-20	F200		;Reverse 20mm
	G10 P1 S200 R200	;Set temperature to 200
	T1					;Select hot end
	M116				;Wait for hot end to reach 200
	G1 E70 F200			;Extrude 70mm
	G4 P2000			;Wait 2 seconds
	G1 E-20 F200		;Reverse 20mm (This will drop the extruded filament off the end of the nozzle on to the scales)
	G10 P1 S0 R0		;Set temperature to 0
	T1					;Select hot end
	M116				;Wait for hot end to reach 0
	G1 E-405 F200		;Reverse 485mm