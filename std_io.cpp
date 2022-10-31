#include "wrench.h"

//------------------------------------------------------------------------------
void wr_read_file( WRValue* stackTop, const int argn, WRContext* c )
{
	stackTop->init();

#ifdef WRENCH_STD_FILE
	if ( argn == 1 )
	{
		WRValue* arg = stackTop - 1;
		const char* fileName = arg->c_str();

#ifdef _WIN32
		struct _stat sbuf;
		int ret = _stat( fileName, &sbuf );
#else
		struct stat sbuf;
		int ret = stat( fileName, &sbuf );
#endif

		if ( ret == 0 )
		{
			FILE *infil = fopen( fileName, "rb" );
			if ( infil )
			{
				stackTop->p2 = INIT_AS_ARRAY;
				stackTop->va = c->getSVA( (int)sbuf.st_size, SV_CHAR, false );
				if ( fread( stackTop->va->m_Cdata, sbuf.st_size, 1, infil ) != 1 )
				{
					stackTop->init();
					return;
				}
			}

			fclose( infil );
		}
	}
#endif
}

//------------------------------------------------------------------------------
void wr_write_file( WRValue* stackTop, const int argn, WRContext* c )
{
	stackTop->init();
#ifdef WRENCH_STD_FILE
	if ( argn == 2 )
	{
		WRValue* arg1 = stackTop - 2;
		unsigned int len;
		char type;
		const char* data = (char*)((stackTop - 1)->array(&len, &type));
		if ( !data || type != SV_CHAR )
		{
			return;
		}
		
		const char* fileName = arg1->c_str();
		if ( !fileName )
		{
			return;
		}

		FILE *outfil = fopen( fileName, "wb" );
		if ( !outfil )
		{
			return;
		}

		stackTop->i = (int)fwrite( data, len, 1, outfil );
		fclose( outfil );
	}
#endif
}

//------------------------------------------------------------------------------
void wr_getline( WRValue* stackTop, const int argn, WRContext* c )
{
	stackTop->init();
#ifdef WRENCH_STD_FILE
	char buf[256];
	int pos = 0;
	for (;;)
	{
		int in = fgetc( stdin );

		if ( in == EOF || in == '\n' || in == '\r' || pos >= 256 )   //@review: this isn't right - need to check for cr and lf
		{ 
			stackTop->p2 = INIT_AS_ARRAY;
			stackTop->va = c->getSVA( pos, SV_CHAR, false );
			memcpy( stackTop->va->m_Cdata, buf, pos );
			break;
		}

		buf[pos++] = in;
	}
#endif
}

//------------------------------------------------------------------------------
void wr_loadFileLib( WRState* w )
{
	wr_registerLibraryFunction( w, "file::read", wr_read_file );
	wr_registerLibraryFunction( w, "file::write", wr_write_file );

	wr_registerLibraryFunction( w, "io::getline", wr_getline );
}
