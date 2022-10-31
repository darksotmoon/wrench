#include <Arduino.h>
#include "wrench.h"

void print( WRState* w, const WRValue* argv, const int argn, WRValue& retVal, void* usr )
{
	char buf[512];
	for( int i=0; i<argn; ++i )
	{
		Serial.print( argv[i].asString(buf, 512) );
	}
}


const char* wrenchCode = 

"print( \"Hello World!\\n\" ); "
"for( i=0; i<10; i++ )       "
"{                           "
"    print( i );               "
"}                           ";
 

void setup()
{
	Serial.begin( 115200 );

	delay( 2000 ); // wait for link to come up for sample

	WRState* w = wr_newState(); // create the state

	wr_registerFunction( w, "print", print ); // bind a function

	unsigned char* outBytes; // compiled code is alloc'ed
	int outLen;
	
	int err = wr_compile( wrenchCode, strlen(wrenchCode), &outBytes, &outLen ); // compile it
	if ( err == 0 )
	{
		wr_run( w, outBytes ); // load and run the code!
		delete[] outBytes; // clean up 
	}

	wr_destroyState( w );
}

void loop()
{}
