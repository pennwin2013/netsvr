#ifndef MSG
#define MSG

module transfer
{
  exception CallException
  {
    string reason;
  };
  sequence <byte> ByteSeq;
  
  struct RequestProtocol
  {
    int requestCode;
    short serviceCode;
    short internalID;
    int status;
	string sessionID;
    ByteSeq data;
    ByteSeq bindata;
  };
  
  struct ResponseProtocol
  {
    int requestCode;
    short serviceCode;
    short internalID;
    int status;
    int returnCode;
    string returnMessage;
	string sessionID;
    ByteSeq data;
    ByteSeq bindata;
  };
  

  interface CallServer
  {
    ["amd"] ResponseProtocol SendMsg (RequestProtocol req) throws CallException;
    ["amd"] idempotent ResponseProtocol QueryMsg (RequestProtocol req) throws CallException;
	ByteSeq ServerStatistic() throws CallException;
  };
};
#endif
