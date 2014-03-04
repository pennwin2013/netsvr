include ../mkstand.mak
LDFLAGS=-lIce -lIceUtil -L../lib -lfdxs -lboost_filesystem -lboost_program_options -lboost_system -lboost_thread \
-lboost_program_options -lpthread -lrt

INCLD+=-I../include -I../fdxsmsglib

SOURCE=\
server_link\
server_msg

#yktserver:$(SRC:=.o)
	#g++ -I. -I/usr/include  -o YktServer.o ykt.o cvtcode $(LDFLAGS)


all:libnetsvr.a

libnetsvr.a:$(SOURCE:=.o)
	$(AR) cr libnetsvr.a $(SOURCE:=.o)
	mv $@ ../bin
clean:
	@rm -f $(SOURCE:=.o)

