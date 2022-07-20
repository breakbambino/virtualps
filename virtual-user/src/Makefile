CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++
AR=$(CROSS_COMPILE)ar
LD=$(CROSS_COMPILE)ld
NM=$(CROSS_COMPILE)nm
STRIP=$(CROSS_COMPILE)strip
RM=rm
RMDIR=rmdir

ifeq ("$(PROJECT_HOME)", "")
PROJECT_HOME=$(shell dirname $(shell pwd))
BINDIR=${PROJECT_HOME}/bin
OUTDIR=out
endif

CFLAGS   += -g -DBOOST_LOG_DYN_LINK
CXXFLAGS = -g -DBOOST_LOG_DYN_LINK -std=c++14
LFLAGS   = -g

INC      =
LIB     += -lyaml-cpp -lboost_system -lboost_thread -lboost_log -lpthread -lndn-cxx

#PKGS    :=
#CFLAGS += `pkg-config --cflags $(PKGS)`
#LIB+= `pkg-config --libs $(PKGS)`

#INC        = $(shell pkg-config --cflags json-c)
#LIB        = -Wl,-Bstatic $(shell pkg-config --libs json-c) -Wl,-Bdynamic -Wl,-lcurl

INCS:= $(wildcard *.h)

COMMON_OBJS = logging.o \
		statistics.o \
		stlutils.o \
		yamlutils.o

FUNC_OBJS = main.o
#PERF_OBJS = sub-perf.o

ALL_OBJS = $(COMMON_OBJS) $(FUNC_OBJS) #$(PERF_OBJS)

FUNC_TARGET=virtual-user
#PERF_TARGET=sub-perf

ALL_TARGET = $(FUNC_TARGET)

all: $(FUNC_TARGET) #$(PERF_TARGET)

$(FUNC_TARGET): $(OUTDIR) $(BINDIR) $(FUNC_OBJS:%=$(OUTDIR)/%) $(COMMON_OBJS:%=$(OUTDIR)/%)
	$(CXX) -o $(@:%=${BINDIR}/%) $(LFLAGS) $(FUNC_OBJS:%=$(OUTDIR)/%) $(COMMON_OBJS:%=$(OUTDIR)/%) $(LIB)

#$(PERF_TARGET): $(OUTDIR) $(BINDIR) $(PERF_OBJS:%=$(OUTDIR)/%) $(COMMON_OBJS:%=$(OUTDIR)/%)
#	$(CXX) -o $(@:%=${BINDIR}/%) $(LFLAGS) $(PERF_OBJS:%=$(OUTDIR)/%) $(COMMON_OBJS:%=$(OUTDIR)/%) $(LIB)

.SUFFIXES : .cpp .c .o
#.c.o:
$(OUTDIR)/%.o:%.c $(INCS)
	$(CC) -o $@ $(CFLAGS) $(INC) -c $<
#.cpp.o:
$(OUTDIR)/%.o:%.cpp $(INCS)
	$(CXX) -o $@ $(CXXFLAGS) $(INC) -c $<

clean:
	$(RM) -f $(ALL_OBJS:%=$(OUTDIR)/%) $(ALL_TARGET:%=${BINDIR}/%)
	$(RMDIR) $(OUTDIR)

$(OUTDIR):
	mkdir -p $(OUTDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

