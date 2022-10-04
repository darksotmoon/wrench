/*~
94
90
hello world
0.1
-0.1
-0.2
0.2
15
-5
4000000
20
0
1.2
18
7
1.7
8
10
-10
26
27
28
8
2
33
120
19
31
32
33
16.3818
90.1
8
1
4
15
15
27
27
12
3
3
13
3
13
10
3
3
3
3
10
1.3
1
~*/

c = --a + a-- + ++a + a--;

a = 90;
a++;
++a;
b = a++;
c = ++a;
log(a);
a--;
--a;
b = a--;
c = --a;
log(a);

log( "hello world" );

log( .1 );
log( -.1 );
log( -0.2 );
log( 0.2 );
log( 0xF );
log( -5 );

b = 4000000;
log( b );
b = 20;
log( b );

b = 0;
log( b );

b = 1.2;
log( b );


b = 10;
b += 5;
b /= 3;
b -= 2;
b *= 6;
log( b ); // 18

a = 10;
a -= 3;
log(a); // 7
b = 10.2;
b -= .5 + (a += 1);
log(b); // 7
log(a); // 1.7

n = -(-(-(-(-(-10))))); // 10 
log(n);
n = -(-(-(-(-10)))); // -10 
log(n);

a = 25;
log( 1 + ++a - 1 );  // 26
log( ++a + 1 - 1 ); // 27
log( 1 - 1 + ++a ); // 28

log( 10 - 2 );
log( 10 / 5 );

a = 28;
log( 1 + a++ + 10 - 2 * 3 ); // 33

log( (40 + 50) * 4 / (((2 + 1))) ); // 120
log( 3 * 5 - 10 + 10 + 10 - 2 * 3 ); // 19
log( 1 + 1 + a++ ); // 31
log( a++ + 1 + 1 ); // 32 
log( 1 + a++ + 1 ); // 33

d = 90.1;
log( d /= 5.5 );
log( d *= 5.5 );

e = 0x04;
f = e << 1;
g = e >> 2;
log( f );
log( g );
log( e );

a = 10;
b = 12;
log( a += 5 ); // 15
log( a ); // 15
log( a += b ); // 27
log( a ); // 27
log( b ); // 12

a = 13;
b = 10;
log( 13 % 10 ); // 3
log( a % 10 ); // 3
log( a ); // 13
log( a % b ); // 3
log( a ); // 13
log( b ); // 10

log( a %= 10 ); // 3
log( a ); // 3
a = 13; 
log( a %= b ); // 3
log( a ); // 3
log( b ); // 10

a = 1.3;
log( a );
(int)a;
log( a );

// excercise keyhole optimizer loads
local();
ag = 8;
bg = 3;
function local()
{
	a = 8;
	b = 3;
	c = a % b;if ( c != 2 ) log("F1");
	c = a << b;if ( c != 64 ) log("F2");
	c = a >> b;if ( c != 1 ) log("F3");
	
	a = 9;
	c = a & b;if ( c != 1 ) log("F4");
	c = a ^ b;if ( c != 10 ) log("F5");
	a = 8;
	c = a | b;if ( c != 11 ) log("F6");

	::ag = 8;
	b = 3;
	c = ::ag % b;if ( c != 2 ) log("F1");
	c = ::ag << b;if ( c != 64 ) log("F2");
	c = ::ag >> b;if ( c != 1 ) log("F3");

	::ag = 9;
	c = ::ag & b;if ( c != 1 ) log("F4");
	c = ::ag ^ b;if ( c != 10 ) log("F5");
	::ag = 8;
	c = ::ag | b;if ( c != 11 ) log("F6");


	a = 8;
	::ab = 3;
	c = a % ::ab;if ( c != 2 ) log("F1");
	c = a << ::ab;if ( c != 64 ) log("F2");
	c = a >> ::ab;if ( c != 1 ) log("F3");

	a = 9;
	c = a & ::ab;if ( c != 1 ) log("F4");
	c = a ^ ::ab;if ( c != 10 ) log("F5");
	a = 8;
	c = a | ::ab;if ( c != 11 ) log("F6");

	::ag = 8;
	::bg = 3;
	c = ::ag % ::bg;if ( c != 2 ) log("F1");
	c = ::ag << ::bg;if ( c != 64 ) log("F2");
	c = ::ag >> ::bg;if ( c != 1 ) log("F3");

	::ag = 9;
	c = ::ag & ::bg;if ( c != 1 ) log("F4");
	c = ::ag ^ ::bg;if ( c != 10 ) log("F5");
	::ag = 8;
	c = ::ag | ::bg;if ( c != 11 ) log("F6");
	
}


