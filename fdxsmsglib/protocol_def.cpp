#include <algorithm>
#include <boost/utility.hpp>
#ifdef WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#include <arpa/inet.h>
#endif
#include "protocol_def.h"

namespace FDXSLib
{
namespace details
{
///////////////////////////////////////////////////////////////////////////////
inline static void size_value_to_buffer( std::streambuf* buf, std::size_t val )
{
  std::size_t temp = htonl( val );
  const char* p = ( const char* ) &temp;
  buf->sputn( p, sizeof( std::size_t ) );
}
inline static void short_value_to_buffer( std::streambuf* buf, unsigned short val )
{
  unsigned short temp = htons( val );
  const char* p = ( const char* ) &temp;
  buf->sputn( p, sizeof( unsigned short ) );
}
inline static void buffer_to_size_value( std::istream& stream, std::size_t& val )
{
  char p[4];
  stream.read( p, sizeof( std::size_t ) );
  std::size_t temp;
  memcpy( ( void* ) &temp, p, sizeof( std::size_t ) );
  val = ntohl( temp );
}
inline static void buffer_to_short_value( std::istream& stream, unsigned short& val )
{
  char p[4];
  stream.read( p, sizeof( unsigned short ) );
  unsigned short temp = 0;
  memcpy( ( void* ) &temp, p, sizeof( unsigned short ) );
  val = ntohs( temp );
}

std::ostream& operator<< ( std::ostream& stream, const protocol_head& head )
{
  std::streambuf* buf = stream.rdbuf();
  /*
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
  */
  //size_value_to_buffer( buf, head.major_version );
  //size_value_to_buffer( buf, head.minor_version );
  buf->sputc( head.major_version );
  buf->sputc( head.minor_version );
  char flag[2];
  memset( flag, 0, sizeof flag );
  if( head.first_flag )
    SET_BITMAP( PTH_FIRSTFLAG_IDX, flag );
  if( head.next_flag )
    SET_BITMAP( PTH_NEXTFLAG_IDX, flag );
  if( head.crc_flag )
    SET_BITMAP( PTH_CRCFLAG_IDX, flag );
  char t;
  switch( head.encrypt_type )
  {
  case BOOST_BINARY( 1 ) :
    t = BOOST_BINARY( 1 );
    flag[0] |= t;
    break;
  case BOOST_BINARY( 10 ) :
    t = BOOST_BINARY( 10 );
    flag[0] |= t;
    break;
  case BOOST_BINARY( 11 ) :
    t = BOOST_BINARY( 11 );
    flag[0] |= t;
    break;
  default:
    break;
  }
  buf->sputn( flag, 2 );
  size_value_to_buffer( buf, head.message_length );
  size_value_to_buffer( buf, head.message_id );
  short_value_to_buffer( buf, ( u_short ) head.sequence_no );
  buf->sputn( head.crc, sizeof( head.crc ) );
  return stream;
}

std::ostream& operator<< ( std::ostream& stream, const protocol_message* msg )
{
  std::streambuf* buf = stream.rdbuf();
  std::size_t t = 0;
  for( const protocol_message* m = msg; m != NULL; m = m->next() )
  {
    if( m->length() == 0 )
      continue;
    buf->sputn( m->rd(), m->length() );
    //t += m->length();
  }
  //std::cout<<"operator<< ( std::ostream& stream, const protocol_message* msg ) : "<<t<<std::endl;
  return stream;
}


std::istream& operator>> ( std::istream& stream, protocol_head& head )
{
  /*
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
  */
  //buffer_to_size_value( stream, head.major_version );
  //buffer_to_size_value( stream, head.minor_version );
  stream.read( &head.major_version, 1 );
  stream.read( &head.minor_version, 1 );
  char flag[2];
  memset( flag, 0, sizeof flag );
  stream.read( flag, sizeof( flag ) );
  if( IS_BITMAP_SET( PTH_FIRSTFLAG_IDX, flag ) )
    head.first_flag = 1;
  if( IS_BITMAP_SET( PTH_NEXTFLAG_IDX, flag ) )
    head.next_flag = 1;
  if( IS_BITMAP_SET( PTH_CRCFLAG_IDX, flag ) )
    head.crc_flag = 1;

  // TODO: not support encrypt yet
  head.encrypt_type = 0;
  buffer_to_size_value( stream, head.message_length );
  buffer_to_size_value( stream, head.message_id );
  unsigned short v = 0;
  buffer_to_short_value( stream, v );
  head.sequence_no = v;

  stream.read( head.crc, sizeof( head.crc ) );
  return stream;
}
std::istream& copy_message_buffer( std::istream& stream, std::size_t  maxlength, protocol_message*& msg )
{
  protocol_message* header = msg;
  std::size_t copy_length = 0;
  if( header == NULL )
  {
    header = new protocol_message;
  }

  protocol_message* wmsg = header;
  for( std::size_t l = 0; l < maxlength; l += copy_length )
  {
    copy_length = std::min( static_cast<std::size_t>( maxlength - l ), wmsg->wr_left() );

    if( copy_length == 0 && l < maxlength )
    {
      protocol_message* next_msg = wmsg->next();
      if( next_msg == NULL )
      {
        wmsg->next( new protocol_message() );
      }
      wmsg = wmsg->next();
      continue;
    }
    stream.read( wmsg->wr(), copy_length );
    wmsg->wr( copy_length );
  }
  msg = header;
  return stream;
}
std::ostream& operator<< ( std::ostream& stream, const protocol& pt )
{
  stream << pt.head;
  if( pt.head.message_length > 0 )
  {
    stream << pt.data;
  }
  return stream;
}
std::istream& operator>> ( std::istream& stream, protocol& pt )
{
  stream >> pt.head;
  if( pt.head.message_length > 0 )
  {
    copy_message_buffer( stream, pt.head.message_length, pt.data );
  }
  return stream;
}
std::size_t new_message_id()
{
  return static_cast<std::size_t>( rand() );
}
///////////////////////////////////////////////////////////////////////////////
}


}

