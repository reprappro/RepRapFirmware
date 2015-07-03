/*

Christmas Snowman

Christine Bowyer

Licence: GPL

*/

thickness=2.5;
arms=true;
union()
{
	difference()
	{
		union()
		{
			cylinder(h =thickness, r = 15,center=true);
			if(arms)
			{
				translate([21,1,0])
					cube([2,32,thickness],center=true);
				rotate([0,0,25])
				union()
				{
				translate([16,0,0])
					cube([14,6,thickness],center=true);
				translate([23,0,0])
					cylinder(h =thickness, r = 4,center=true);
				}
				rotate([0,0,140])
				union()
				{
				translate([16,0,0])
					cube([14,6,thickness],center=true);
				translate([23,0,0])
					cylinder(h =thickness, r = 4,center=true);
				}
			}
		}
		translate([0,15,0])
			cube([20,4,2*thickness],center=true);
		for ( i = [-1 : 1] )
			translate([0,3+5*i,0])
				cylinder(h=20, r=1, center=true,$fn=10);
	}
	translate([0,20,0])
	difference()
	{
		union()
		{		
			cylinder(h =thickness, r = 10,center=true);
			rotate([0,0,25])
			difference()
			{
			translate([0,10,0])
			union()
			{
				cube([12,8,thickness],center=true);
				translate([0,-3,0])
					cube([25,3,thickness],center=true);
			}
			translate([3.5,12,0])
			cylinder(h=20, r=1, center=true,$fn=10);
			}
		}
		
		translate([3.5,2,0])
			cylinder(h=20, r=1.5, center=true,$fn=10);
		translate([-3.5,2,0])
			cylinder(h=20, r=1.5, center=true,$fn=10);
	
		difference()
		{
			cylinder(h =20, r = 6,center=true, $fn=30);
			translate([0,4,0])
					cylinder(h=20, r=8, center=true,$fn=30);
		}
	}
}