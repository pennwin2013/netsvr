include ../mkstand.mak
LDFLAGS=-L../bin -lfdxsmsg -lnetsvr -L../lib -lIce -lIceUtil -lboost_filesystem -lboost_program_options -lboost_system \
-lboost_thread -llog4cplus -lboost_program_options -lpthread -lrt

INCLD+=-I./ -I../include -I../fdxsmsglib -I../netsvrlib
all:netsrv
SOURCE=\
callserver\
netmain\
queue\
queue_stack\
event_queue\
subprocess\
logger_imp

#yktserver:$(SRC:=.o)
	#g++ -I. -I/usr/include  -o YktServer.o ykt.o cvtcode $(LDFLAGS)

netsrv:$(SOURCE:=.o)
	$(CXX) $(CCFLAGS) $(INCLD)-DTRACE -o $@ $(SOURCE:=.o)  $(LIBSUDF) $(LDFLAGS) 
	mv $@ ../bin
clean:
	@rm -f *.o tp*
