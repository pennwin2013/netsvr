#ifndef _MESSAGE_BLOCK_HPP_
#define _MESSAGE_BLOCK_HPP_

#include <cassert>
#include <algorithm>
#include <stdexcept>

namespace FDXSLib
{

class default_pool
{
public:
  explicit default_pool( std::size_t chunk_size = 8196 ) : chunk_size_( chunk_size )
  {
  }
  ~default_pool()
  {
  }
  void* malloc()
  {
    char* p = new char[chunk_size_];
    return p;
  }
  void free( void* chunk )
  {
    delete []( ( char* ) chunk );
  }
private:
  default_pool( const default_pool& );
  default_pool& operator= ( const default_pool& );
  std::size_t chunk_size_;
};

struct msgblk_empty_t
{
};

static const msgblk_empty_t msgblk_empty = {};

template<std::size_t N, class Pool = default_pool>
class message_block
{
public:
  typedef Pool allocator_type;
  typedef message_block<N, allocator_type> message_block_type;
  typedef message_block_type* pointer_type;
  typedef const pointer_type const_pointer_type;
  ////////////////////////////////////////////////////////
  static const std::size_t kDefaultBlockSize = N;
  ///////////////////////////////////////////////////////////
  explicit message_block() : pnext_( 0 ), ppre_( 0 ), read_ptr_( 0 ), write_ptr_( 0 ),
    block_size_( kDefaultBlockSize )
  {
    alloc_memory();
    //std::cout << "create message block " << std::endl;
  }
  explicit message_block( msgblk_empty_t t ): pnext_( 0 ), ppre_( 0 ),
    read_ptr_( 0 ), block_size_( kDefaultBlockSize ), write_ptr_( 0 ), block_( 0 )
  {
  }
  pointer_type next( pointer_type next_block )
  {
    if( NULL == next_block )
    {
      this->pnext_ = NULL;
      return this;
    }
    pointer_type t = this->pnext_;
    this->pnext_ = next_block;
    next_block->ppre_ = this;
    next_block->add_tail( t );
    return this;
  }
  ~message_block()
  {
    //std::cout << "release message block " << std::endl;
    do_release();
  }
  void swap( message_block_type& rhs )
  {
    std::swap( this->block_, rhs.block_ );
    std::swap( this->write_ptr_, rhs.write_ptr_ );
    std::swap( this->read_ptr_, rhs.read_ptr_ );
    std::swap( this->ppre_, rhs.ppre_ );
    std::swap( this->pnext_, rhs.pnext_ );
    std::swap( this->block_size_, rhs.block_size_ );
  }
  const_pointer_type next() const
  {
    return pnext_;
  }
  pointer_type next()
  {
    return pnext_;
  }
  pointer_type last_block()
  {
    pointer_type ret = this;
    for( pointer_type m = this; m != NULL; m = m->pnext_ )
    {
      ret = m;
    }
    return ret;
  }
  inline void add_tail( pointer_type p )
  {
    last_block()->next( p );
  }
  inline std::size_t size() const
  {
    return block_size_;
  }
  inline std::size_t length() const
  {
    std::size_t s = static_cast<std::size_t>( write_ptr_ - block_ );
    return s;
  }
  inline std::size_t rd_left() const
  {
    std::size_t s = static_cast<std::size_t>( write_ptr_ - read_ptr_ );
    return s;
  }
  inline std::size_t total_rd_left() const
  {
    std::size_t s = rd_left();
    if( pnext_ != NULL )
      s += pnext_->total_rd_left();
    return s;
  }
  inline std::size_t wr_left() const
  {
    std::size_t s = static_cast<std::size_t>( block_ + block_size_ - write_ptr_ );
    return s;
  }
  inline std::size_t total_wr_left() const
  {
    std::size_t s = wr_left();
    if( pnext_ != NULL )
      s += pnext_->total_wr_left();
    return s;
  }
  inline std::size_t total_length() const
  {
    std::size_t s = length();
    if( pnext_ != NULL )
      s += pnext_->total_length();
    return s;
  }
  void release()
  {
    do_release();
    delete this;
  }
  const char* rd() const
  {
    return read_ptr_;
  }
  void rd( std::size_t len )
  {
    assert( read_ptr_ + len <= block_ + block_size_ );
    read_ptr_ += len;
  }
  void reset_rd()
  {
    read_ptr_ = block_;
  }
  void reset_wr()
  {
    write_ptr_ = block_;
  }
  char* wr()
  {
    return write_ptr_;
  }
  void wr( std::size_t len )
  {
    if( write_ptr_ + len <= block_ + block_size_ )
    {
      write_ptr_ += len;
    }
    else
    {
      throw std::overflow_error( "memory size overflow!" );
    }
  }
  void clear()
  {
    write_ptr_ = block_;
    read_ptr_ = block_;
    if( pnext_ != NULL )
      pnext_->clear();
  }
  char* data()
  {
    return block_;
  }
  void copy_from_buffer( const char* buffer, std::size_t maxlen )
  {
    pointer_type msg = this;
    std::size_t copy_length = 0;
    for( std::size_t len = 0; len < maxlen; len += copy_length )
    {
		copy_length = std::min( maxlen - len, msg->wr_left() );
      if( copy_length == 0 )
      {
        msg->next( new message_block_type );
        msg = msg->next();
        continue;
      }
      memcpy( msg->wr(), buffer + len, copy_length );
      msg->wr( copy_length );
    }
  }
  void copy_to_buffer( char *& buffer, std::size_t* buffer_length, std::size_t maxlen = 0 )
  {
    std::size_t new_size = this->total_rd_left();
    if( maxlen > 0 )
      new_size = std::min( maxlen, new_size );
    buffer = new char[new_size];
    *buffer_length = new_size;
    pointer_type m = this;
    for( std::size_t offset = 0;
         m != NULL && offset < new_size ; m = m->next() )
    {
      if( m->rd_left() == 0 )
        continue;
      std::size_t copy_length = std::min( new_size - offset, m->rd_left() );
      memcpy( buffer + offset, m->rd(), copy_length );
      m->rd( copy_length );
      offset += copy_length;
    }
  }
private:
  message_block( const message_block<N, allocator_type>& rhs );
  message_block<N, allocator_type>& operator= (
    const message_block<N, allocator_type>& rhs );
  void alloc_memory()
  {
    block_ = ( char* ) s_alloc_.malloc();
    assert( block_ != NULL );
    write_ptr_ = block_;
    read_ptr_ = block_;
  }
  void free_memory()
  {
    write_ptr_ = 0;
    read_ptr_ = 0;
    s_alloc_.free( block_ );
    block_ = 0;
  }
  void do_release()
  {
    if( block_ == 0 )
      return;
    free_memory();
    if( pnext_ != NULL )
    {
      pnext_->release();
    }
    ppre_ = 0;
    pnext_ = 0;
  }
  ///////////////////////////////////////////////////////////////////////////////
  static allocator_type s_alloc_;
  message_block_type* pnext_;
  message_block_type* ppre_;
  std::size_t block_size_;
  char* block_;
  char* write_ptr_;
  char* read_ptr_;
};

template<std::size_t N, class P>
typename message_block<N, P>::allocator_type message_block<N, P>::s_alloc_( N );

template<std::size_t N, class P>
const std::size_t message_block<N, P>::kDefaultBlockSize;

}

#endif
