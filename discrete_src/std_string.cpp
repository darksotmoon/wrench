/*******************************************************************************
Copyright (c) 2022 Curt Hartung -- curt.hartung@gmail.com

MIT Licence

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*******************************************************************************/

#include "wrench.h"

#include <string.h>
#include <ctype.h>

//------------------------------------------------------------------------------
int wr_strlenEx( WRValue* val )
{
	if ( val->type == WR_REF )
	{
		return wr_strlenEx( val->r );
	}

	return (val->xtype == WR_EX_ARRAY && val->va->m_type == SV_CHAR) ? val->va->m_size :  0;
}

//------------------------------------------------------------------------------
void wr_strlen( WRValue* stackTop, const int argn, WRContext* c )
{
	stackTop->i = argn == 1 ? wr_strlenEx( stackTop - 1 ) : 0;
}

//------------------------------------------------------------------------------
int wr_sprintfEx( char* outbuf, const char* fmt, WRValue* args, const int argn )
{
	if ( !fmt )
	{
		return 0;
	}
	
	enum
	{
		zeroPad         = 1<<0,
		negativeJustify = 1<<1,
		secondPass      = 1<<2,
		negativeSign    = 1<<3,
		parsingSigned   = 1<<4,
		altRep			= 1<<5,
	};

	char* out = outbuf;
	int listPtr = 0;

resetState:

	char padChar = ' ';
	unsigned char columns = 0;
	char flags = 0;

	for(;;)
	{
		char c = *fmt;
		fmt++;

		if ( !c )
		{
			*out = 0;
			break;
		}

		if ( !(secondPass & flags) )
		{
			if ( c != '%' ) // literal
			{
				*out++ = c;
			}
			else // possibly % format specifier
			{
				flags |= secondPass;
			}
		}
		else if ( c >= '0' && c <= '9' ) // width
		{
			columns *= 10;
			columns += c - '0';
			if ( !columns ) // leading zero
			{
				flags |= zeroPad;
				padChar = '0';
			}
		}
		else if ( c == '#' )
		{
			flags |= altRep;
		}
		else if ( c == '-' ) // left-justify
		{
			flags |= negativeJustify;
		}
#ifdef WRENCH_FLOAT_SPRINTF
		else if (c == '.') // ignore, we might be reading a floating point decimal position
		{ }
#endif
		else if ( c == 'c' ) // character
		{
			if ( listPtr < argn )
			{
				*out++ = (char)(args[listPtr++].asInt());
			}
			goto resetState;
		}
		else if ( c == '%' ) // literal %
		{
			*out++ = c;
			goto resetState;
		}
		else // string or integer
		{
			char buf[20]; // buffer for integer

			const char *ptr; // pointer to first char of integer

			if ( c == 's' ) // string
			{
				buf[0] = 0;
				if ( listPtr < argn )
				{
					ptr = (char *)args[listPtr].array();
					if ( !ptr )
					{
						return 0;
					}
					++listPtr;
				}
				else
				{
					ptr = buf;
				}

				padChar = ' '; // in case some joker uses a 0 in their column spec

copyToString:

				// get the string length so it can be formatted, don't
				// copy it, just count it
				unsigned char len = 0;
				for ( ; *ptr; ptr++ )
				{
					len++;
				}
				
				ptr -= len;
				
				// Right-justify
				if ( !(flags & negativeJustify) )
				{
					for ( ; columns > len; columns-- )
					{
						*out++ = padChar;
					}
				}
				
				if ( flags & negativeSign )
				{
					*out++ = '-';
				}

				if ( flags & altRep || c == 'p' )
				{
					*out++ = '0';
					*out++ = 'x';
				}

				for (unsigned char l = 0; l < len; ++l )
				{
					*out++ = *ptr++;
				}

				// Left-justify
				for ( ; columns > len; columns-- )
				{
					*out++ = ' ';
				}

				goto resetState;
			}
			else
			{
				unsigned char base;
				unsigned char width;
				unsigned int val;

				if ( c == 'd' || c == 'i' )
				{
					flags |= parsingSigned;
					goto parseDecimal;
				}
#ifdef WRENCH_FLOAT_SPRINTF
				else if ( c == 'f' || c == 'g' )
				{
					char floatBuf[32];
					int i = 30;
					floatBuf[31] = 0;
					const char *f = fmt;
					for( ; *f != '%'; --f, --i )
					{
						floatBuf[i] = *f;
					}
					floatBuf[i] = '%';

					// suck in whatever lib we need for this
					const int chars = snprintf( buf, 31, floatBuf + i, args[listPtr++].asFloat());

					// your system not have snprintf? the unsafe version is:
//					const int chars = sprintf( buf, floatBuf + i, args[listPtr++].asFloat());
					
					for( int j=0; j<chars; ++j )
					{
						*out++ = buf[j];
					}
					goto resetState;
				}
#endif
				else if ( c == 'u' ) // decimal
				{
parseDecimal:
					base  = 10;
					width = 10;
					goto convertBase;
				}
				else if ( c == 'b' ) // binary
				{
					base  = 2;
					width = 16;
					goto convertBase;
				}
				else if ( c == 'o' ) // octal
				{
					base  = 8;
					width = 5;
					goto convertBase;
				}
				else if ( c == 'x' || c == 'X' || c == 'p' ) // hexadecimal or pointer (pointer is treated as 'X')
				{
					base = 16;
					width = 8;
convertBase:
					if ( listPtr < argn )
					{
						val = args[listPtr++].asInt();
					}
					else
					{
						val = 0;
					}

					if ( (flags & parsingSigned) && (val & 0x80000000) )
					{
						flags |= negativeSign;
						val = -(int)val;
					}

					// Convert to given base, filling buffer backwards from least to most significant
					char* p = buf + width;
					*p = 0;
					ptr = p; // keep track of one past left-most non-zero digit
					do
					{
						char d = val % base;
						val /= base;

						if ( d )
						{
							ptr = p;
						}

						d += '0';
						if ( d > '9' ) // handle bases higher than 10
						{
							d += 'A' - ('9' + 1);
							if ( c == 'x' ) // lowercase
							{
								d += 'a' - 'A';
							}
						}

						*--p = d;

					} while ( p != buf );

					ptr--; // was one past char we want

					goto copyToString;
				}
				else // invalid format specifier
				{
					goto resetState;
				}
			}
		}
	}

	return out - outbuf;
}

//------------------------------------------------------------------------------
void wr_format( WRValue* stackTop, const int argn, WRContext* c )
{
	stackTop->init();

	if( argn >= 1 )
	{
		WRValue* args = stackTop - argn;
		char outbuf[512];
		char inbuf[512];
		args[0].asString( inbuf, 512 );
		
		int size = wr_sprintfEx( outbuf, inbuf, args + 1, argn - 1 );

		stackTop->p2 = INIT_AS_ARRAY;
		stackTop->va = c->getSVA( size, SV_CHAR, false );
		memcpy( stackTop->va->m_Cdata, outbuf, size );
	}
}

//------------------------------------------------------------------------------
void wr_printf( WRValue* stackTop, const int argn, WRContext* c )
{
	stackTop->init();

#ifdef WRENCH_STD_FILE
	if( argn >= 1 )
	{
		WRValue* args = stackTop - argn;

		char outbuf[512];
		stackTop->i = wr_sprintfEx( outbuf, args[0].c_str(), args + 1, argn - 1 );
		
		printf( "%s", outbuf );
	}
#endif
}

//------------------------------------------------------------------------------
void wr_sprintf( WRValue* stackTop, const int argn, WRContext* c )
{
	stackTop->init();
	WRValue* args = stackTop - argn;
	
	if ( argn < 2
		 || args[0].type != WR_REF
		 || args[1].xtype != WR_EX_ARRAY
		 || args[1].va->m_type != SV_CHAR )
	{
		return;
	}

	char outbuf[512];
	char inbuf[512];
	args[1].asString(inbuf);
	stackTop->i = wr_sprintfEx( outbuf, inbuf, args + 2, argn - 2 );

	args[0].r->p2 = INIT_AS_ARRAY;
	args[0].r->va = c->getSVA( stackTop->i, SV_CHAR, false );
	memcpy( args[0].r->va->m_Cdata, outbuf, stackTop->i );
}

//------------------------------------------------------------------------------
void wr_isspace( WRValue* stackTop, const int argn, WRContext* c )
{
	if ( argn == 1 )
	{
		stackTop->i = (int)isspace( (char)(stackTop - 1)->asInt() );
	}
}

//------------------------------------------------------------------------------
void wr_isalpha( WRValue* stackTop, const int argn, WRContext* c )
{
	if ( argn == 1 )
	{
		stackTop->i = (int)isalpha( (char)(stackTop - 1)->asInt() );
	}
}

//------------------------------------------------------------------------------
void wr_isdigit( WRValue* stackTop, const int argn, WRContext* c )
{
	if ( argn == 1 )
	{
		stackTop->i = (int)isdigit( (char)(stackTop - 1)->asInt() );
	}
}

//------------------------------------------------------------------------------
void wr_isalnum( WRValue* stackTop, const int argn, WRContext* c )
{
	if ( argn == 1 )
	{
		stackTop->i = (int)isalnum( (char)(stackTop - 1)->asInt() );
	}
}

//------------------------------------------------------------------------------
void wr_mid( WRValue* stackTop, const int argn, WRContext* c )
{
	stackTop->init();
	
	if ( argn < 2 )
	{
		return;
	}

	WRValue* args = stackTop - argn;
	unsigned int len;
	const char* data = (char *)(args[0].array(&len));
	
	if( !data || len <= 0 )
	{
		return;
	}
	
	unsigned int start = args[1].asInt();
	stackTop->p2 = INIT_AS_ARRAY;

	unsigned int chars = 0;
	if ( start < len )
	{
		chars = (argn > 2) ? args[2].asInt() : len;
		if ( chars > (len - start) )
		{
			chars = len - start;
		}
	}
	
	stackTop->va = c->getSVA( chars, SV_CHAR, false );
	memcpy( stackTop->va->m_Cdata, data + start, chars );
}

//------------------------------------------------------------------------------
void wr_strchr( WRValue* stackTop, const int argn, WRContext* c )
{
	stackTop->i = -1;

	if ( argn < 2 )
	{
		return;
	}

	WRValue* args = stackTop - argn;

	const char* str = (const char*)(args[0].array());
	if ( !str )
	{
		return;
	}
	
	char ch = (char)args[1].asInt();
	const char* found = strchr( str, ch );
	if ( found )
	{
		stackTop->i = found - str;
	}
}

//------------------------------------------------------------------------------
void wr_tolower( WRValue* stackTop, const int argn, WRContext* c )
{
	if ( argn == 1 )
	{
		WRValue& value = (stackTop - 1)->deref();
		if ( value.xtype == WR_EX_ARRAY && value.va->m_type == SV_CHAR )
		{
			stackTop->p2 = INIT_AS_ARRAY;
			stackTop->va = c->getSVA( value.va->m_size, SV_CHAR, false );

			for( uint32_t i=0; i<value.va->m_size; ++i )
			{
				stackTop->va->m_SCdata[i] = tolower(value.va->m_SCdata[i]);
			}
		}
		else if ( value.type == WR_INT )
		{
			stackTop->i = (int)tolower( (stackTop - 1)->asInt() );
		}
	}
}

//------------------------------------------------------------------------------
void wr_toupper( WRValue* stackTop, const int argn, WRContext* c )
{
	if ( argn == 1 )
	{
		WRValue& value = (stackTop - 1)->deref();
		if ( value.xtype == WR_EX_ARRAY && value.va->m_type == SV_CHAR )
		{
			stackTop->p2 = INIT_AS_ARRAY;
			stackTop->va = c->getSVA( value.va->m_size, SV_CHAR, false );

			for( uint32_t i=0; i<value.va->m_size; ++i )
			{
				stackTop->va->m_SCdata[i] = toupper(value.va->m_SCdata[i]);
			}
		}
		else if ( value.type == WR_INT )
		{
			stackTop->i = (int)toupper( (stackTop - 1)->asInt() );
		}

	}
}

//------------------------------------------------------------------------------
void wr_tol( WRValue* stackTop, const int argn, WRContext* c )
{
	if ( argn == 2 )
	{
		const char* str = (const char*)stackTop[-2].array();
		if ( str )
		{
			stackTop->i = (int)strtol( str, 0, stackTop[-1].asInt() );
		}
	}
	else if ( argn == 1 )
	{
		const char* str = (const char*)stackTop[-1].array();
		if ( str )
		{
			stackTop->i = (int)strtol( str, 0, 10 );
		}
	}
}

//------------------------------------------------------------------------------
void wr_concat( WRValue* stackTop, const int argn, WRContext* c )
{
	stackTop->init();

	if ( argn < 2 )
	{
		return;
	}

	WRValue* args = stackTop - argn;
	unsigned int len1 = 0;
	unsigned int len2 = 0;
	const char* data1 = (const char*)args[0].array( &len1 );
	const char* data2 = (const char*)args[1].array( &len2 );

	if( !data1 || !data2 )
	{
		return;
	}

	stackTop->p2 = INIT_AS_ARRAY;
	stackTop->va = c->getSVA( len1 + len2, SV_CHAR, false );
	memcpy( stackTop->va->m_Cdata, data1, len1 );
	memcpy( stackTop->va->m_Cdata + len1, data2, len2 );
}

//------------------------------------------------------------------------------
void wr_left( WRValue* stackTop, const int argn, WRContext* c )
{
	stackTop->init();

	if ( argn < 2 )
	{
		return;
	}

	WRValue* args = stackTop - argn;
	unsigned int len = 0;
	const char* data = (const char*)args[0].array(&len );
	if ( !data )
	{
		return;
	}
	unsigned int chars = args[1].asInt();

	stackTop->p2 = INIT_AS_ARRAY;
	
	if ( chars > len )
	{
		chars = len;
	}
	
	stackTop->va = c->getSVA( chars, SV_CHAR, false );
	memcpy( stackTop->va->m_Cdata, data, chars );
}

//------------------------------------------------------------------------------
void wr_right( WRValue* stackTop, const int argn, WRContext* c )
{
	stackTop->init();
	
	if ( argn < 2 )
	{
		return;
	}

	WRValue* args = stackTop - argn;
	unsigned int len = 0;
	const char* data = (const char*)args[0].array(&len );
	if ( !data )
	{
		return;
	}
	unsigned int chars = args[1].asInt();

	if ( chars > len )
	{
		chars = len;
	}

	stackTop->p2 = INIT_AS_ARRAY;
	stackTop->va = c->getSVA( chars, SV_CHAR, false );
	memcpy( stackTop->va->m_Cdata, data + (len - chars), chars );
}

//------------------------------------------------------------------------------
void wr_trimright( WRValue* stackTop, const int argn, WRContext* c )
{
	stackTop->init();

	WRValue* args = stackTop - argn;
	const char* data;
	int len = 0;
	
	if ( argn < 1 || ((data = (const char*)args->array((unsigned int *)&len)) == 0) )
	{
		return;
	}

	while( --len >= 0 && isspace(data[len]) );

	++len;

	stackTop->p2 = INIT_AS_ARRAY;
	stackTop->va = c->getSVA( len, SV_CHAR, false );
	memcpy( stackTop->va->m_Cdata, data, len );
}

//------------------------------------------------------------------------------
void wr_trimleft( WRValue* stackTop, const int argn, WRContext* c )
{
	stackTop->init();

	WRValue* args = stackTop - argn;
	const char* data;
	unsigned int len = 0;
	
	if ( argn < 1 || ((data = (const char*)args->array(&len)) == 0) )
	{
		return;
	}

	unsigned int marker = 0;
	while( marker < len && isspace(data[marker]) ) { ++marker; }

	stackTop->p2 = INIT_AS_ARRAY;
	stackTop->va = c->getSVA( len - marker, SV_CHAR, false );
	memcpy( stackTop->va->m_Cdata, data + marker, len - marker );
}

//------------------------------------------------------------------------------
void wr_trim( WRValue* stackTop, const int argn, WRContext* c )
{
	stackTop->init();

	WRValue* args = stackTop - argn;
	const char* data;
	int len = 0;
	if ( argn < 1 || ((data = (const char*)args->array((unsigned int*)&len)) == 0) )
	{
		return;
	}

	int marker = 0;
	while( marker < len && isspace(data[marker]) ) { ++marker; }

	while( --len >= marker && isspace(data[len]) );

	++len;

	stackTop->p2 = INIT_AS_ARRAY;
	stackTop->va = c->getSVA( len - marker, SV_CHAR, false );
	memcpy( stackTop->va->m_Cdata, data + marker, len - marker );
}

//------------------------------------------------------------------------------
void wr_insert( WRValue* stackTop, const int argn, WRContext* c )
{
	stackTop->init();

	if ( argn < 3 )
	{
		return;
	}

	WRValue* args = stackTop - argn;
	unsigned int len1 = 0;
	unsigned int len2 = 0;
	const char* data1 = (const char*)args[0].array( &len1 );
	const char* data2 = (const char*)args[1].array( &len2 );

	if( !data1 || !data2 )
	{
		return;
	}
	
	unsigned int pos = args[2].asInt();
	if ( pos >= len1 )
	{
		pos = len1;
	}

	stackTop->p2 = INIT_AS_ARRAY;
	unsigned int newlen = len1 + len2;
	stackTop->va = c->getSVA( newlen, SV_CHAR, false );

	memcpy( stackTop->va->m_Cdata, data1, pos );
	memcpy( stackTop->va->m_Cdata + pos, data2, len2 );
	memcpy( stackTop->va->m_Cdata + pos + len2, data1 + pos, len1  - pos );
}


//------------------------------------------------------------------------------
void wr_loadStringLib( WRState* w )
{
	wr_registerLibraryFunction( w, "str::strlen", wr_strlen );
	wr_registerLibraryFunction( w, "str::sprintf", wr_sprintf );
	wr_registerLibraryFunction( w, "str::printf", wr_printf );
	wr_registerLibraryFunction( w, "str::format", wr_format );
	wr_registerLibraryFunction( w, "str::isspace", wr_isspace );
	wr_registerLibraryFunction( w, "str::isdigit", wr_isdigit );
	wr_registerLibraryFunction( w, "str::isalpha", wr_isalpha );
	wr_registerLibraryFunction( w, "str::mid", wr_mid );
	wr_registerLibraryFunction( w, "str::chr", wr_strchr );
	wr_registerLibraryFunction( w, "str::tolower", wr_tolower );
	wr_registerLibraryFunction( w, "str::toupper", wr_toupper );
	wr_registerLibraryFunction( w, "str::tol", wr_tol );
	wr_registerLibraryFunction( w, "str::concat", wr_concat );
	wr_registerLibraryFunction( w, "str::left", wr_left );
	wr_registerLibraryFunction( w, "str::trunc", wr_left );
	wr_registerLibraryFunction( w, "str::right", wr_right );
	wr_registerLibraryFunction( w, "str::substr", wr_mid );
	wr_registerLibraryFunction( w, "str::trimright", wr_trimright );
	wr_registerLibraryFunction( w, "str::trimleft", wr_trimleft );
	wr_registerLibraryFunction( w, "str::trim", wr_trim );
	wr_registerLibraryFunction( w, "str::insert", wr_insert );
}
