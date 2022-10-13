/*******************************************************************************
Copyright (c) 2022 Curt Hartung -- curt.hartung@gmail.com

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
#ifndef _WRENCH_H
#define _WRENCH_H
/*------------------------------------------------------------------------------*/

/************************************************************************
The interpreter operates in two fundamentally different modes:
switch() and jumptable (where each instruction jumps to the next one
directly instead of going to the top and switch()-ing again)

jumptable: only works on GCC/Clang type compilers, since it uses the &&
label operator

This Flag:

-- Has NO EFFECT on the compiler or the compiled object code

-- Is MUCH faster (on the order of 40%!) on embedded "uncomplicated"
   systems ie- simple caching and flat memory speeds.

-- Is a bit faster (more like 20%) on PC
  
-- increases interpreter size slightly (a K or two)

*/
#include <cstddef>
#define WRENCH_JUMPTABLE_INTERPRETER
/***********************************************************************/

/************************************************************************
Build wrench without it's compiler. this will minimize
the code size at the cost of being able to only load/run bytecode
*/
//#define WRENCH_WITHOUT_COMPILER
/***********************************************************************/

/************************************************************************
For efficiency The interpreter expects a pointer to a single contiguous
block of read-only bytecode, and it will never call the loader again.

This will compile in the bounds-checking and call the loader whenever
it detects execution space outside of the current block. The loader
must return a pointer to the requested offset in the bytecode and the
size of the mapped block. (see WR_LOAD_BLOCK_FUNC)

******** RETURN AT LEAST 20 BYTES or "bad things" happen ******

*/
//#define WRENCH_PARTIAL_BYTECODE_LOADS
/***********************************************************************/

/************************************************************************
how many stack locations the wrench state will pre-allocate. The stack
cannot be grown so this needs to be enough for the life of the process.
this does NOT include global/array space which is allocated separately, just
function calls. Unless you are using piles of local data AND recursing
like crazy a modest size should be more than enough.

On most systems this will consume 8 bytes per stack entry
*/
#define WRENCH_DEFAULT_STACK_SIZE 100
/***********************************************************************/

/************************************************************************
.asString() uses sprintf from stdio. If you don't want to
include or use that library then undefine this and that method will no
longer function
*/
#define WRENCH_SPRINTF_OPERATIONS
/***********************************************************************/

#define WRENCH_VERSION 131

#include <stdint.h>

struct WRState;
struct WRValue;
struct WRContext;
struct WRFunction;

/***************************************************************/
/**************************************************************/
//                       State Management

// create/destroy a WRState object that can run multiple contexts/threads
WRState* wr_newState( int stackSize =WRENCH_DEFAULT_STACK_SIZE );
void wr_destroyState( WRState* w );



/***************************************************************/
/**************************************************************/
//                        Running Code

// the interpreter calls this to get a block of bytecode
//
// if WRENCH_PARTIAL_BYTECODE_LOADS is NOT defined (default) this will be
// called exactly one time and expect the entire code block to be
// returned; it will not be checked again.
//
// if WRENCH_PARTIAL_BYTECODE_LOADS is defined, it expects at least 20 bytes
// of code returned per call and will re-call any time code is
// needed from outside the returned region.
//
//
// NOTE: The returned memory IS USED IN PLACE. it is NOT copied to an
// internal buffer so it must remain valid for the life of the execution
//
// offset: where in the bytecode stream wrench wants to load (always zero
//         for non - WRENCH_PARTIAL_BYTECODE_LOADS)
// block:  pointer to pointer where the block is mapped, this can be
//         read-only and must remain valid for the life of the script!
// usr:    opaque user-supplied pointer passed to each load call (optional)
typedef unsigned int (*WR_LOAD_BLOCK_FUNC)( int offset, const unsigned char** block, void* usr );

// compile source code and return a new'ed block of bytecode ready
// for wr_run(), memory must be delete[]'ed
// return value is a WRError
// optionally an "errMsg" buffer can be passed which will output a
//   human-readable string of what went wrong and where.
int wr_compile( const char* source, const int size, unsigned char** out, int* outLen, char* errMsg =0 );

// run a block of code, either as a single block or with a loader that
// will be called back. PLEASE SEE NOTES ABOVE REGARDING: WRENCH_PARTIAL_BYTECODE_LOADS

// w:          state (see wr_newState)
// block/size: location of bytecode
// loader/usr: callback for loader and a void* pointer that will be
//             passed to the loader opaquely (optional)
// RETURNS:    a WRContext pointer to be passed to wr_callFunction
//             'global' values are NOT SHARED between contexts
WRContext* wr_run( WRState* w, const unsigned char* block );
WRContext* wr_run( WRState* w, WR_LOAD_BLOCK_FUNC loader, void* usr =0 );


// after wr_run() this allows any function contained to be
// called with the given arguments, returning a single value
//
// contextId:    the context in which the function was loaded
// functionName: plaintext name of the function to be called
// argv:         array of WRValues to call the function with (optional)
// argn:         how many arguments argv contains (optional, but if
//               zero then argv will be ignored)
// RETURNS:      zero for no error or WRError code
int wr_callFunction( WRState* w, WRContext* context, const char* functionName, const WRValue* argv =0, const int argn =0 );

// exactly the same as the above but the hash is supplied directly to
// save compute time, us wr_hashStr(...) to obtain the hash of the
// functionName
int wr_callFunction( WRState* w, WRContext* context, const int32_t hash, const WRValue* argv =0, const int argn =0 );

// The raw function pointer can be pre-loaded with wr_getFunction() and
// passed directly
// This method is exposed so functions can be called with an absolute
// minmum of overhead
int wr_callFunction( WRState* w, WRContext* context, WRFunction* function, const WRValue* argv, const int argn );

// After wr_run(...) or wr_callFunction(...) has run, there is always a
// return value (default 0) this command fetches it
WRValue* wr_returnValueFromLastCall( WRState* w );

// once wr_run() is called the returned context object can be used to
// pre-fetch a function pointer. This reduces the overhead of calling
// that function to almost nothing.
WRFunction* wr_getFunction( WRContext* context, const char* functionName );

// Destroy a context you no longer need and free up all the memory it
// was using, all contexts are freed when wr_destroyState() is called,
// so calling this is not necessary, but reccomended so the memory can
// be re-used
void wr_destroyContext( WRState* w, WRContext* context );


/***************************************************************/
/***************************************************************/
//                 Callbacks from wrench                         

// register a function inside a state that can be called (by ALL
// contexts)
// callback will contain:
// w:             state it was called back from
// argv:          pointer to arguments function was called
//                with (may be null!)
// argn:          how many arguments it was called with
// retVal:        this value will be passed back, default: integer zero
// usr:           opaque pointer function was registered with
typedef void (*WR_C_CALLBACK)(WRState* w, const WRValue* argv, const int argn, WRValue& retVal, void* usr );

// IMPORTANT: The values passed may be references (keepin' it real) so
// always use the getters inside the WRValue class:

// int: WRValue.asInt();
// float: WRValue.asFloat();
// string: WRValue.asString();

// w:        state to register with (will be available to all contexts)
// name:     name of the function
// function: callback (see typdef above)
// usr:      opaque pointer that will be passed to the callback (optional)
int wr_registerFunction( WRState* w, const char* name, WR_C_CALLBACK function, void* usr =0 );

/***************************************************************/
/***************************************************************/
//                   wrench Library calls

// Wrench allows libraries to be loaded in the form of <name>::<func>
// these calls are installed in such a way that they impose ZERO
// overhead if not used, zero memory footprint, text segment, etc.

// library calls do assume a deep knowledge of the internal workings of
// wrench so the callback is an absolute minimum of info:
// stackTop: current top of the stack, arguments have been pushed here,
//           and any return value must be placed here, not pushed!
//           since the code path is highly optimized it might be ignore
//           by the calling code, see one of the many library examples
// argn:     how many arguments this function was called with. no
//           guarantees are made about matching a function signature,
//           library calls validate if they care.
typedef void (*WR_LIB_CALLBACK)( WRValue* stackTop, const int argn );

// w:         state to register with (will be available to all contexts)
// signature: library signature must be in the form of <lib>::<name>
//            ex: "math::cos"
// function:  callback (see typdef above)
void wr_registerLibraryFunction( WRState* w, const char* signature, WR_LIB_CALLBACK function );

// you can write your own libraries (look at std.cpp for all the
// standard ones) but several have been provided:

void wr_loadAllLibs( WRState* w ); // load all libraries in one call

void wr_loadMathLib( WRState* w ); // provides most of the calls in math.h
void wr_loadStdLib( WRState* w ); // standard functions like sprintf/rand/
//void wr_loadStringLib( WRState* w );	// srsly? how much time
                                        // you think I have to waste on
                                        // features no one will ever use

/***************************************************************/
/***************************************************************/
//              wrench types and how to make them!

// The WRValue is the basic data unit inside wrench, it can be a
// float/int/array whatever... think of it as a thneed. It's 8 bytes on
// a 32 bit system and you can create one on the stack no problemo, no
// heap will be harmed

// load a value up and make it ready for calling a function
void wr_makeInt( WRValue* val, int i );
void wr_makeFloat( WRValue* val, float f );

// So you want to access memory directly? This is how you do that. with
// a custom User type. creating a UserData is like creating a struct,
// it's a container that can be accessed by wrench with dot notation:
//
// someUserData.value1      <- simple value
// someUserData.value2[20]  <- array
// someUserData.value1.value2 <- yes this works too

// STEP 1:    create the user data. This will be the 'top level' object
// the other members are loaded into. It can be passed to any function as
// a parameter. "SizeHint" allows a right-sized hash table to be made
// instead of growing, which can help reduce memory fragmentation, the
// hint should be the number of values you plan to pack into it.
void wr_makeUserData( WRValue* value, int sizeHint =0 );

// STEP 2:    load it up!
//
// add the given value to the userData, which will be accessible by
// 'name' as in:  theObject.name 

// userData:  User data object created with wr_makeUserData() above
// name:      what the array will be called inside wrench
// data/len:  pointer to a user-managed memory space which will be
//            accessible via .name inside wrench
// value:     pointer to a plain user-mananged WRValue
void wr_addUserValue( WRValue* userData, const char* name, WRValue* value );
void wr_addUserCharArray( WRValue* userData, const char* name, const unsigned char* data, const int len );
void wr_addUserIntArray( WRValue* userData, const char* name, const int* data, const int len );
void wr_addUserFloatArray( WRValue* userData, const char* name, const float* data, const int len );

// STEP 3:    use it!
// the userData object can now be used in any call into wrench and be
// accessible as shown above. All structures will be freed when WRValue
// falls out of scope, alternately they can be manually freed by
// calling the free() method from WRValue


// NOTE: for efficiency there is no constructor on WRValue despite it
// having pointers! This is because 99.99% of the time one is
// created it's immediately loaded with values, so a constructor would
// be wasted cycles in all cases. But this also means it's your
// responsibility to call "wr_makeXXX or at LEAST .init() after
// creating one!


/***************************************************************/
/***************************************************************/
//                          Errors

// in order to minimize text segment size, only a minimum of strings
// are embedded in the code. error messages are very verbose enums
enum WRError
{
	WR_ERR_None = 0,

	WR_ERR_compiler_not_loaded,
	WR_ERR_function_hash_signature_not_found,
	WR_ERR_unknown_opcode,
	WR_ERR_unexpected_EOF,
	WR_ERR_unexpected_token,
	WR_ERR_bad_expression,
	WR_ERR_bad_label,
	WR_ERR_statement_expected,
	WR_ERR_unterminated_string_literal,
	WR_ERR_newline_in_string_literal,
	WR_ERR_bad_string_escape_sequence,
	WR_ERR_tried_to_load_non_resolvable,
	WR_ERR_break_keyword_not_in_looping_structure,
	WR_ERR_continue_keyword_not_in_looping_structure,
	WR_ERR_expected_while,
	WR_ERR_compiler_panic,

	WR_ERR_run_must_be_called_by_itself_first,
	WR_ERR_hash_table_size_exceeded,
	WR_ERR_wrench_function_not_found,
	WR_ERR_array_must_be_indexed,
	WR_ERR_context_not_found,

	WR_ERR_usr_data_template_already_exists,
	WR_ERR_usr_data_already_exists,
	WR_ERR_usr_data_template_not_found,
	WR_ERR_usr_data_refernce_not_found,

	WR_warning_enums_follow,

	WR_WARN_c_function_not_found,
	WR_WARN_lib_function_not_found,
};

// after wrench executes it may have set an error code, this is how to
// retreive it. This sytems is coarse at the moment. Re-entering the
// interpreter clears the last error
WRError wr_getLastError( WRState* w );


/******************************************************************/
//                    "standard" functions

// hashing function used inside wrench, it's a stripped down murmer,
// not academically fantastic but very good and very fast
uint32_t wr_hash( const void* dat, const int len );
uint32_t wr_hashStr( const char* dat );

extern int32_t wr_Seed;

// inside-baseball time. This has to be here so stuff that includes
// "wrench.h" can create a WRValue. nothing to see here.. smile and
// wave...

//------------------------------------------------------------------------------
enum WRValueType
{
	WR_INT = 0x00,
	WR_FLOAT = 0x01,
	WR_REF = 0x02,
	WR_EX = 0x3,
};

enum WRExType
{
	WR_EX_NONE = 0x00,
	WR_EX_USR = 0x01,
	
	WR_EX_REFARRAY = 0x04,
	WR_EX_ARRAY = 0x05,
	WR_EX_STRUCT = 0x06,
	WR_EX_HASH = 0x0C,
};


//------------------------------------------------------------------------------
class WRContainerData;
class WRGCArray;
struct WRValue
{
	// never reference the data members directly, they are unions and
	// bad things will happen. Always access them with one of these
	// methods
	int asInt() const
	{
		// this should be faster than a switch by checking the most common cases first
		if ( type == WR_INT )
		{
			return i;
		}
		if ( type == WR_REF )
		{
			return r->asInt();
		}
		if ( type == WR_FLOAT )
		{
			return (int)f;
		}
		if ( type == WR_EX )
		{
			return xtype == WR_EX_REFARRAY ? arrayValueAsInt() : 0;
		}
		return i;

		// but the switch looks like this:
/* 
		switch( type )
		{
			case WR_REF: return r->asInt();
			case WR_EX: return xtype == WR_EX_REFARRAY ? arrayValueAsInt() : 0;
			case WR_FLOAT: return (int)f;
			default: return i;
		}
*/
	}

	float asFloat() const
	{
		if ( type == WR_FLOAT )
		{
			return f;
		}
		if ( type == WR_REF )
		{
			return r->asFloat();
		}
		if ( type == WR_INT )
		{
			return (float)i;
		}
		if ( type == WR_EX )
		{
			return xtype == WR_EX_REFARRAY ? arrayValueAsFloat() : 0;
		}
		return f;
	}
	
        // string: must point to a buffer long enough to contain at least len bytes.
        // the pointer will be passed back
	char* asString( char* string, size_t len ) const;

	//WRValueType getType() { return enumType == WR_REF ? r->getType() : enumType; }

	WRValue* asValueArray( int* len =0 );
	unsigned char* asCharArray( int* len =0 );
	int* asIntArray( int* len =0 );
	float* asFloatArray( int* len =0 );

	inline void init() { p = 0; p2 = 0; } // call upon first create or when you're sure no memory is hanging from one

//private: // is what this SHOULD be.. but that's impractical since the
		   // VM is not an object that can be friended. don't mess
		   // around here unless you really feel comfortable with the
		   // internal workings of wrench.. in fact.. same goes for
		   // above here :)
		   
	void free(); // treat this object as if it owns any memory allocated on u or va pointer
	int arrayValueAsInt() const;
	float arrayValueAsFloat() const;

	inline ~WRValue()
	{
		if (xtype)
		{
			free(); 
		}
	} // this will stay inline MOST of the time

	union // first 4 bytes (or 8 on a 64-bit system but.. what are you doing here?)
	{
		int32_t i;
		float f;
		const void* p;
		WRValue* r;
		WRGCArray* va;
		WRContainerData* u;
	};

	union
	{
		struct
		{
			uint8_t type; // carries the type
			uint8_t padL;
			uint8_t padH;
			uint8_t xtype; // carries the extra type (if it exists)
		};

		uint32_t p2;
		WRValue* frame;
	};

	inline WRValue& operator= (const WRValue& V) { p = V.p; frame = V.frame; return *this; }
};

#ifdef WRENCH_JUMPTABLE_INTERPRETER
 #ifndef __GNUC__
  #undef WRENCH_JUMPTABLE_INTERPRETER
 #else
  #ifdef WRENCH_PARTIAL_BYTECODE_LOADS
   #error WRENCH_JUMPTABLE_INTERPRETER and WRENCH_PARTIAL_BYTECODE_LOADS are incompatible!
  #endif
 #endif
#endif


#if __arm__ || WIN32 || __linux__ || __MINGW32__ || __APPLE__ || __MINGW64__
#include <memory.h>
#else
#include <Arduino.h>
#endif


#ifndef WRENCH_COMBINED
#include "utils.h"
#include "container_data.h"
#include "vm.h"
#include "opcode.h"
#include "str.h"
#include "opcode_stream.h"
#include "cc.h"
#endif

#endif

