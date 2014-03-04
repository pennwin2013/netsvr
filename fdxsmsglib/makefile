include ../mkstand.mak
LDFLAGS=-lIce -lIceUtil -L../lib -lfdxs -lboost_filesystem -lboost_program_options -lboost_system -lboost_thread \
-lboost_program_options -lpthread -lrt

INCLD+=-I../include

SOURCE=\
protocol_def\
server_session\
details/server_sess_impl\
details/client_sess_impl

#yktserver:$(SRC:=.o)
	#g++ -I. -I/usr/include  -o YktServer.o ykt.o cvtcode $(LDFLAGS)


all:libfdxsmsg.a

libfdxsmsg.a:$(SOURCE:=.o)
	$(AR) cr libfdxsmsg.a $(SOURCE:=.o)
	mv $@ ../bin
clean:
	@rm -f $(SOURCE:=.o)

