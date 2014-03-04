
STRIP=/usr/bin/strip
UNAME=$(shell uname)
ifeq ($(UNAME),SunOS)
	CC=gcc
	CXX=g++
	LIBS=-lstdc++ -lpthread -lsocket -lnsl -lm
	CCFLAGS= -D_MT -DSUNOS -Wall -DICE_IGNORE_VERSION $(USEROPTS)
	CXXFLAGS=  -D_MT -Wall  -DSUNOS $(USEROPTS)
	LIB_SUFFIX=32
	AR=/usr/ccs/bin/ar
endif
ifeq ($(UNAME),HP-UX)
	CC=aCC
	CXX=aCC
	PREPROCESSOR_C=aCC -E -DESQL=""
	PREPROCESSOR_CXX=aCC -E -DESQL=""
	LIBS=-lpthread 
	CCFLAGS= -mt  -DUNIX -DHP_UX -Agcc -AC99 -Wall $(USEROPTS)
	CXXFLAGS= -mt -DUNIX -DHP_UX -Ag++ -Wall $(USEROPTS)
	LIB_SUFFIX=32
endif
ifeq ($(UNAME),Linux)
	CC=gcc
	CXX=g++
	LIBS=-lstdc++ -lpthread -lnsl
	CCFLAGS= -DTRACE -D_MT -DLINUX $(USEROPTS) -DBOOST_ALL_NO_LIB -DICE_IGNORE_VERSION
	CXXFLAGS= -DTRACE -D_MT -DLINUX $(USEROPTS)
	LIB_SUFFIX=
endif
ifeq ($(UNAME),AIX)
	CC=gcc
	CXX=g++
	LIBS=-lstdc++ -lpthread -lnsl
	CCFLAGS= -D_MT -Wall  -DAIX -Wall $(USEROPTS)
	CXXFLAGS=  -D_MT -Wall  -DAIX -Wall $(USEROPTS)
	LIB_SUFFIX=32
endif

ARFLAGS=-r


.SUFFIXES: .o .c .cpp .pc .sqc .sqC .act .ec .eC

.c.o:
	$(CC) -c $< $(CCFLAGS) $(INCLD) $(INCLUDE) -o $@

.cpp.o:
	$(CXX) -c $< $(CCFLAGS) $(INCLD) -o $@

.cc.o:
	$(CXX) -c $< $(CCFLAGS) $(INCLD) -o $@


	
