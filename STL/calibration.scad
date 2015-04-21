// OpenSCAD file for producing the testpiece, gauge and thumbwheel for orthogonal axis compensation for RepRapPro Ormerod, Mendel and Huxley printers //

long = 90; // Set length for testpiece here

// comment out the following lines to get the parts you want. Comment out the translations if you want the part at the origin.

Gauge();

translate ([15,15,0])
TestPiece();

translate ([40,40,0])
ThumbWheel();

module Gauge()
{
	difference()
	{
		 union()
		 {
			cube([long,8,8]);
			translate([5, 10, 4])
			rotate([0,0,45])
			cube([5,5,8], center=true);
			rotate([0,0,5])
			cube([8,long,8]);
		 }
		 translate([-5, long-5, 4])
		 {
			 rotate([0,90,0])
				cylinder(r=1.6,h=30,center=true,$fn=30);
			 translate([16,0,0])
			 rotate([0,90,0])
				rotate([0,0,30])
				cylinder(r=3.2,h=30,center=true,$fn=6);
			 translate([-10,-15,-10])
			 cube([8,25,20]);
		 }
	}
}

module ThumbWheel()
{
	translate ([0,0,2]) difference()
	{
		cylinder(r=10,h=4,center=true,$fn=50);
		cylinder(r=1.6,h=30,center=true,$fn=30);
	
		translate([0,0,15])
			cylinder(r=3.2,h=30,center=true,$fn=6);
		for(a = [0 : 9] )
		{
		rotate([0,0,36*a])
		translate([4,0,1.3])
			rotate([45,0,0])
				cube([20,4,4]);
		}
	}
}


module TestPiece()
{
 union()
 {
	cube([long,8,8]);
	cube([8,long,8]);
	cube([10,10,long]);
 }
}

