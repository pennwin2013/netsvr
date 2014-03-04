include ../mkstand.mak

MV=/bin/mv
TARGET=worker
LDFLAGS+=-L../bin -lnetsvr -lfdxsmsg -L../lib -llog4cplus -lboost_system -lboost_thread -lboost_filesystem -lboost_program_options -lrt -lpthread 

INCLD+=-I../include -I../fdxsmsglib -I../netsvrlib

SOURCES=\
global_func\
XGetopt\
worker\
logger_imp

ALL:$(TARGET)

clean:
	$(RM) -f *.o $(TARGET)


$(TARGET):$(SOURCES:=.o)
	$(CXX) $(CCFLAGS) $(INCLD)-DTRACE -o $@ $(SOURCES:=.o)  $(LIBSUDF) $(LDFLAGS)	
	$(MV) $@ ../bin/
