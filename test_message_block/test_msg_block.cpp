#define BOOST_TEST_MODULE const_string test_msgblock
#include <string>
#include <fstream>
#include <iostream>
#include <boost/test/unit_test.hpp>
#include "message_block.hpp"
using namespace FDXSLib;
//BOOST_TEST_LOG_LEVEL(boost::unit_test::);

typedef message_block<4096> message_4k;

struct test_message_block
{
  test_message_block(): log_( "test_msgblock.txt" )
  {
    boost::unit_test::unit_test_log.set_stream( log_ );
    BOOST_TEST_MESSAGE( "setup message block" );
  }
  ~test_message_block()
  {
    BOOST_TEST_MESSAGE( "shutdown message block" );
    boost::unit_test::unit_test_log.set_stream( std::cout );
  }
private:
  std::ofstream log_;
};

BOOST_GLOBAL_FIXTURE( test_message_block );

BOOST_AUTO_TEST_CASE( test_init )
{
  message_4k* msg = new message_4k;
  BOOST_CHECK( msg->size() == 4096 );
  msg->release();
}

BOOST_AUTO_TEST_CASE( test_one_msg )
{
  const char value[] = "hello";
  message_4k* msg = new message_4k;
  strcpy( msg->wr(), value );
  msg->wr( strlen( value ) );
  BOOST_CHECK( msg->length() == strlen( value ) );
  msg->release();
}

BOOST_AUTO_TEST_CASE( test_two_msg )
{
  message_4k* header = new message_4k;
  message_4k* msg = header;
  memset( msg->wr(), 0x13, msg->size() );
  msg->wr( msg->size() );
  BOOST_CHECK( msg->length() == msg->size() );

  header->next( new message_4k );
  msg = header->next();

  memset( msg->wr(), 0x13, msg->size() );
  msg->wr( msg->size() );
  BOOST_CHECK( msg->length() == msg->size() );

  BOOST_CHECK( header->total_length() == header->size() * 2 );

  header->release();
}

//BOOST_AUTO_TEST_SUITE_END();

