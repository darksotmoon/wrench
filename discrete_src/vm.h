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

#ifndef _VM_H
#define _VM_H
/*------------------------------------------------------------------------------*/


//------------------------------------------------------------------------------
struct WRFunction
{
	char arguments;
	char frameSpaceNeeded;
	char frameBaseAdjustment;
	uint32_t hash;
	union
	{
		const unsigned char* offset;
		int offsetI;
	};
};

//------------------------------------------------------------------------------
struct WRCFunctionCallback
{
	WR_C_CALLBACK function;
	void* usr;
};

//------------------------------------------------------------------------------
class WRGCValueArray;
enum WRGCArrayType
{
	SV_CHAR = 0x02,
	SV_INT = 0x04,
	SV_FLOAT = 0x06,

	SV_VALUE = 0x01,
	SV_HASH_TABLE = 0x03,
};

#define IS_SVA_VALUE_TYPE(V) ((V)->m_type & 0x1)

//------------------------------------------------------------------------------
struct WRContext
{
	WRFunction* localFunctions;
	WRHashTable<WRFunction*> localFunctionRegistry;
	WRValue* globalSpace;
	int globals;

	const unsigned char* bottom;
	int32_t stopLocation;

	WRGCArray* svAllocated;

	void mark( WRValue* s );
	void gc( WRValue* stackTop );
	WRGCArray* getSVA( int size, WRGCArrayType type, bool init );
	
	WRState* w;

	WRContext* next;
	
	WRContext( WRState* state );
	~WRContext();
};

//------------------------------------------------------------------------------
struct WRState
{
	WRHashTable<WRCFunctionCallback> c_functionRegistry;
	WRHashTable<WR_LIB_CALLBACK> c_libFunctionRegistry;

	WRError err;

	WRValue* stack;
	int stackSize;

	WRContext* contextList;

	WRState( int EntriesInStack =WRENCH_DEFAULT_STACK_SIZE );
	~WRState();
};

//------------------------------------------------------------------------------
class WRGCArray
{
public:

	char m_type;
	char m_preAllocated;
	uint16_t m_mod;
	uint32_t m_size;

	union
	{
		uint32_t* m_hashTable;
		const unsigned char* m_ROMHashTable;
	};

	WRGCArray* m_next; // for gc

	union
	{
		const void* m_data;
		int* m_Idata;
		unsigned char* m_Cdata;
		WRValue* m_Vdata;
		float* m_Fdata;
	};

	WRGCArray( const unsigned int size,
			   const WRGCArrayType type,
			   const void* preAlloc =0 )
	{
		m_type = (char)type;
		m_next = 0;
		m_size = size;
		if ( preAlloc )
		{
			m_preAllocated = 1;
			m_data = preAlloc;
		}
		else
		{
			m_preAllocated = 0;
			switch( m_type )
			{
				case SV_VALUE: { m_Vdata = new WRValue[size]; break; }
				case SV_CHAR: { m_Cdata = new unsigned char[size]; break; }
				case SV_INT: { m_Idata = new int[size]; break; }
				case SV_FLOAT: { m_Fdata = new float[size]; break; }
				case SV_HASH_TABLE:
				{
					m_mod = c_primeTable[0];
					m_hashTable = new uint32_t[m_mod];
					memset( m_hashTable, 0, m_mod*sizeof(uint32_t) );
					m_size = m_mod;
					m_Vdata = new WRValue[m_size];
					memset( (char*)m_Vdata, 0, m_size*sizeof(WRValue) );
					break;
				}
			}
		}
	}

	void clear()
	{
		if ( m_preAllocated )
		{
			return;
		}
		
		switch( m_type )
		{
			case SV_HASH_TABLE: delete[] m_hashTable; m_hashTable = 0;
			case SV_VALUE: delete[] m_Vdata; break; 
						   
			case SV_CHAR: { delete[] m_Cdata; break; }
			case SV_INT: { delete[] m_Idata; break; }
			case SV_FLOAT: { delete[] m_Fdata; break; }
		}

		m_data = 0;
	}
	
	~WRGCArray() 
	{
		clear(); 
	}
	
	void* operator[]( const unsigned int l ) { return get( l ); }

	WRGCArray& operator= ( WRGCArray& A )
	{
		clear();

		m_preAllocated = 0;

		m_mod = A.m_mod;
		m_size = A.m_size;
		m_ROMHashTable = A.m_ROMHashTable;
		
		if ( !m_next )
		{
			m_next = A.m_next;
			A.m_next = this;
		}

		switch( (m_type = A.m_type) )
		{
			case SV_HASH_TABLE:
			case SV_VALUE:
			{
				m_data = new WRValue[m_size];
				m_hashTable = new uint32_t[m_mod];
				memcpy( m_hashTable, A.m_hashTable, m_mod*sizeof(uint32_t) );
				for( unsigned int i=0; i<m_size; ++i )
				{
					((WRValue*)m_data)[i] = ((WRValue*)A.m_data)[i];
				}
				break;
			}

			case SV_CHAR:
			{
				m_Cdata = new unsigned char[m_size];
				memcpy( (char*)m_data, (char*)A.m_data, m_size );
				break;
			}
			case SV_INT:
			{
				m_Idata = new int[m_size];
				memcpy( (char*)m_data, (char*)A.m_data, m_size*sizeof(int) );
				break;
			}

			case SV_FLOAT:
			{
				m_Fdata = new float[m_size];
				memcpy( (char*)m_data, (char*)A.m_data, m_size*sizeof(float) );
				break;
			}
		}
		
		return *this;
	}

	void* growHash( const uint32_t l );
	void* get( const uint32_t l )
	{
		int s = l < m_size ? l : m_size - 1;

		switch( m_type )
		{
			case SV_VALUE: { return (void*)(m_Vdata + s); }
			case SV_CHAR: { return (void*)(m_Cdata + s); }
			case SV_INT: { return  (void*)(m_Idata + s); }
			case SV_FLOAT: { return (void*)(m_Fdata + s); }

			case SV_HASH_TABLE:
			{
				uint32_t index = l % m_mod;

				if (m_hashTable[index] == 0)
				{
					m_hashTable[index] = l;
					return (void*)(m_Vdata + index);
				}
				else if (m_hashTable[index] == l)
				{
					return (void*)(m_Vdata + index);
				}

				return growHash(l);
			}

			default: return 0;
		}
	}

private:
	WRGCArray(WRGCArray& A);

};

#define INIT_AS_ARRAY    (((uint32_t)WR_EX) | ((uint32_t)WR_EX_ARRAY<<24))
#define INIT_AS_USR      (((uint32_t)WR_EX) | ((uint32_t)WR_EX_USR<<24))
#define INIT_AS_REFARRAY (((uint32_t)WR_EX) | ((uint32_t)WR_EX_REFARRAY<<24))
#define INIT_AS_STRUCT   (((uint32_t)WR_EX) | ((uint32_t)WR_EX_STRUCT<<24))
#define INIT_AS_HASH_TABLE (((uint32_t)WR_EX) | ((uint32_t)WR_EX_HASH_TABLE<<24))

#define INIT_AS_REF      WR_REF
#define INIT_AS_INT      WR_INT
#define INIT_AS_FLOAT    WR_FLOAT

#define IS_EXARRAY_TYPE(P) ((P)&0x80)

#define ARRAY_ELEMENT_FROM_P2(P) (((P)&0x00FFFF00) >> 8)
#define ARRAY_ELEMENT_TO_P2(P,E) { (P)->padL = (E); (P)->padH  = ((E)>>8); (P)->xtype |= (((E)>>16)&0x1F); }

void wr_arrayToValue( const WRValue* array, WRValue* value );
void wr_intValueToArray( const WRValue* array, int32_t I );
void wr_floatValueToArray( const WRValue* array, float F );
void wr_countOfArrayElement( WRValue* array, WRValue* target );


typedef void (*WRVoidFunc)( WRValue* to, WRValue* from );
extern WRVoidFunc wr_assign[16];

extern WRVoidFunc wr_SubtractAssign[16];
extern WRVoidFunc wr_AddAssign[16];
extern WRVoidFunc wr_ModAssign[16];
extern WRVoidFunc wr_MultiplyAssign[16];
extern WRVoidFunc wr_DivideAssign[16];
extern WRVoidFunc wr_ORAssign[16];
extern WRVoidFunc wr_ANDAssign[16];
extern WRVoidFunc wr_XORAssign[16];
extern WRVoidFunc wr_RightShiftAssign[16];
extern WRVoidFunc wr_LeftShiftAssign[16];
extern WRVoidFunc wr_postinc[4];
extern WRVoidFunc wr_postdec[4];

typedef void (*WRTargetFunc)( WRValue* to, WRValue* from, WRValue* target );
extern WRTargetFunc wr_AdditionBinary[16];
extern WRTargetFunc wr_MultiplyBinary[16];
extern WRTargetFunc wr_SubtractBinary[16];
extern WRTargetFunc wr_DivideBinary[16];
extern WRTargetFunc wr_LeftShiftBinary[16];
extern WRTargetFunc wr_RightShiftBinary[16];
extern WRTargetFunc wr_ModBinary[16];
extern WRTargetFunc wr_ANDBinary[16];
extern WRTargetFunc wr_ORBinary[16];
extern WRTargetFunc wr_XORBinary[16];

typedef void (*WRStateFunc)( WRContext* c, WRValue* to, WRValue* from, WRValue* target );
extern WRStateFunc wr_index[16];
extern WRStateFunc wr_assignAsHash[4];
extern WRStateFunc wr_assignToArray[16];

typedef bool (*WRReturnFunc)( WRValue* to, WRValue* from );
extern WRReturnFunc wr_CompareEQ[16];
extern WRReturnFunc wr_CompareGT[16];
extern WRReturnFunc wr_CompareLT[16];
extern WRReturnFunc wr_LogicalAND[16];
extern WRReturnFunc wr_LogicalOR[16];

typedef void (*WRUnaryFunc)( WRValue* value );
extern WRUnaryFunc wr_negate[4];
extern WRUnaryFunc wr_preinc[4];
extern WRUnaryFunc wr_predec[4];
extern WRUnaryFunc wr_toInt[4];
extern WRUnaryFunc wr_toFloat[4];
extern WRUnaryFunc wr_bitwiseNot[4];

typedef bool (*WRReturnSingleFunc)( WRValue* value );

typedef bool (*WRValueCheckFunc)( WRValue* value );
extern WRReturnSingleFunc wr_LogicalNot[4];

typedef void (*WRIndexHashFunc)( WRValue* value, WRValue* target, uint32_t hash );
extern WRIndexHashFunc wr_IndexHash[4];

void wr_assignToHashTable( WRContext* c, WRValue* index, WRValue* value, WRValue* table );

extern WRReturnFunc wr_CompareEQ[16];

#endif
