// OpenSCAD file for producing the testpiece, gauge and thumbwheel for orthogonal axis compensation for RepRapPro Ormerod, Mendel and Huxley printers //
// Revised with text. You need to use OpenSCAD version 2015.03 for the test to render correctly, and Arial Black installed as a font //

long = 90; // Set length for testpiece here
chamfer=0.5;

// comment out the following lines to get the parts you want. Comment out the translations if you want the part at the origin.

Gauge();

translate ([12,12,0])
TestPiece();

translate ([34,34,0])
ThumbWheel();



module Gauge()
{
    union()
     {
         chamfer_cube(long+5,8,8);
         translate([5, 10, 4]) rotate([0,0,45]) chamfer_cube(5,5,8,ctr=true);
         
         difference ()
         {
             rotate([0,0,5]) chamfer_cube(8,long,8);
             translate ([-sin(5)*(long-5)-1,(long-5),-1]) cube([10,11,10]);
         }
         
         translate ([-sin(5)*(long-5),(long-5),0]) difference ()
         {
             chamfer_cube(8,10,8,x1=false);
             translate([4,5,4]) rotate([0,90,0]) cylinder(r=1.6,h=10,center=true,$fn=30);
             translate([4,5,4]) rotate([90,0,90]) cylinder(r=3.2,h=6,center=false,$fn=6);
         }
     }
}

module ThumbWheel()
{
	difference()
	{
		intersection ()
		{
			cylinder(r=10,h=4,center=false,$fn=50);
			cylinder(r1=10-chamfer,r2=10+4-chamfer,h=4,center=false,$fn=50);
		}
		translate ([0,0,-1]) cylinder(r=1.6,h=6,center=false,$fn=30);
		translate ([0,0,-0.02]) cylinder(r1=1.6+chamfer,r2=1.6,h=chamfer,center=false,$fn=30);
		translate([0,0,2]) cylinder(r=3.2,h=3,center=false,$fn=6);

		for(a = [0 : 36 : 360] )
		{
		rotate([0,0,a]) translate([4,0,3.2]) rotate([45,0,0])
            cube([7,1.5,1.5]);
		}
	}
}


module TestPiece()
{
 union()
 {
	difference ()
     {
         chamfer_cube(long,8,8);
         translate ([long-1,0.75,1]) rotate ([90,0,90]) 
         linear_extrude(height = 1.1) text("X", font="Arial Black", size=6);
     }

	difference ()
     {
         chamfer_cube(8,long,8);
         translate ([0.75,long+0.1,1]) rotate ([90,0,0]) 
         linear_extrude(height = 1.1) text("Y", font="Arial Black", size=6);
     }

	difference ()
     {
         chamfer_cube(10,10,long);
         translate ([1.5,1.5,long-1]) 
         linear_extrude(height = 1.1) text("Z", font="Arial Black", size=7);
     }


 }
}

module chamfer_cube (x,y,z,h=chamfer,ctr=false,x1=true,x2=true,y1=true,y2=true)
{
    dh = sqrt(2*pow(h,2));

	if (ctr==true)
    {
        translate ([-x/2,-y/2,-z/2]) difference ()
        {
            cube([x,y,z]);
            if (x1==true) {translate([x/2,-0.02,-0.02]) rotate ([45,0,0]) cube([x+1,dh,dh],center=true);}
            if (x2==true) {translate([x/2,y+0.02,-0.02]) rotate ([45,0,0]) cube([x+1,dh,dh],center=true);}
            if (y1==true) {translate([-0.02,y/2,-0.02]) rotate ([0,45,0]) cube([dh,y+1,dh],center=true);}
            if (y2==true) {translate([x+0.02,y/2,-0.02]) rotate ([0,45,0]) cube([dh,y+1,dh],center=true);}
        }
    }
    else
    {
        difference ()
        {
            cube([x,y,z]);
            if (x1==true) {translate([x/2,-0.02,-0.02]) rotate ([45,0,0]) cube([x+1,dh,dh],center=true);}
            if (x2==true) {translate([x/2,y+0.02,-0.02]) rotate ([45,0,0]) cube([x+1,dh,dh],center=true);}
            if (y1==true) {translate([-0.02,y/2,-0.02]) rotate ([0,45,0]) cube([dh,y+1,dh],center=true);}
            if (y2==true) {translate([x+0.02,y/2,-0.02]) rotate ([0,45,0]) cube([dh,y+1,dh],center=true);}
        }        
	}
}