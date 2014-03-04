#ifndef _PROTOCOL_DEF_H_
#define _PROTOCOL_DEF_H_

#include <iostream>
#include "message_block.hpp"

namespace FDXSLib
{
///////////////////////////////////////////////////////////////////////////////
#define BUFFER_MAX_LENGTH 8196
typedef message_block<BUFFER_MAX_LENGTH> protocol_message;

#ifdef _MSC_VER
#pragma pack(1)
#define PACK
typedef unsigned short u_short;
#elif defined(_GCC)
typedef unsigned short u_short;
#define PACK __attribute__ ((packed))
#else
#define PACK
#endif

#define PTH_LENGTH 16
struct protocol_head
{
  char major_version;
  char minor_version;
  char first_flag;
  char next_flag;
  char crc_flag;
  char encrypt_type;
  std::size_t message_length;
  std::size_t message_id;
  u_short sequence_no;
  char crc[2];
} PACK;

struct protocol
{
  static const std::size_t kMajorVersion = 1;
  static const std::size_t kMinorVersion = 0;
  protocol_head head;
  protocol_message* data;
};
//const std::size_t protocol::kMajorVersion;
//const std::size_t protocol::kMinorVersioin;




#ifdef _MSC_VER
#pragma pack()
#elif defined(_GCC)
#undef PACK
#else
#undef PACK
#endif

///////////////////////////////////////////////////////////////////////////////
#define PTH_FIRSTFLAG_IDX 0
#define PTH_NEXTFLAG_IDX 1
#define PTH_CRCFLAG_IDX 2

#define _SET_BYTE_BIT(_f,_b) do{ _b |= (1<<(7-_f)) & 0xFF; }while(0)
#define _CLEAR_BYTE_BIT(_f,_b) do { _b &= ~((1<<(7-_f)) & 0xFF); } while(0)
#define _IS_SET_BYTE_BIT(_f,_b)  (_b & ((1<<(7-_f)) & 0xFF))

#define SET_BITMAP(idx,_map) do { std::size_t _b = idx >> 3; \
    std::size_t _f = idx - (_b << 3); \
    _SET_BYTE_BIT(_f,_map[_b]); }while(0)

#define CLEAR_BITMAP(idx,_map) do { std::size_t _b = idx >> 3; \
    std::size_t _f = idx - (_b << 3); \
    _CLEAR_BYTE_BIT(_f,_map[_b]); }while(0)


inline bool _is_set_byte_bit( std::size_t idx, char* _map )
{
  std::size_t _b = idx >> 3;
  std::size_t _f = idx - ( _b << 3 );
  return _IS_SET_BYTE_BIT( _f, _map[_b] ) ? true : false;
}

#define IS_BITMAP_SET(idx,_map) _is_set_byte_bit(idx,_map)



///////////////////////////////////////////////////////////////////////////////
namespace details
{
std::istream& operator>> ( std::istream& stream, protocol& pt );
std::ostream& operator<< ( std::ostream& stream, const protocol& pt );

std::size_t new_message_id();
std::ostream& operator<< ( std::ostream& stream, const protocol_head& head );
std::ostream& operator<< ( std::ostream& stream, const protocol_message* msg );

///////////////////////////////////////////////////////////////////////////////
std::istream& operator>> ( std::istream& stream, protocol_head& head );
std::istream& copy_message_buffer( std::istream& stream, std::size_t  maxlength, protocol_message*& msg );
}

}

#endif // _PROTOCOL_DEF_H_

