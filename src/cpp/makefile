all:

THISDIR:=$(dir $(lastword ${MAKEFILE_LIST}))
-include ${CURDIR}/../../defines.mk

COMMON_OBJECTS := Alignment.o AlnGraphBoost.o
PBDAGCON_OBJECTS := BlasrM5AlnProvider.o main.o SimpleAligner.o
DAZCON_OBJECTS := DB.o QV.o align.o DazAlnProvider.o dazcon.o SimpleAligner.o

CPPFLAGS += -MMD -MP
CXXFLAGS ?= -O3 -std=c++11 -Wall -Wuninitialized -pedantic
CFLAGS = -O3 -Wall -Wextra -fno-strict-aliasing

INCDIRS := \
	${DAZZ_DB_INCLUDE} \
	${DALIGNER_INCLUDE} \
	${LIBBLASR_INCLUDE} \
	${LIBPBDATA_INCLUDE} \
	${LIBPBIHDF_INCLUDE} \
	${PBBAM_INCLUDE} \
	${HDF5_INCLUDE} \
	${HTSLIB_INCLUDE}
SYS_INCDIRS := \
	${BOOST_INCLUDE} \
	${THISDIR}/third-party
LIBDIRS := \
	${LIBBLASR_LIB} \
	${LIBPBDATA_LIB} \
	${LIBPBIHDF_LIB} \
	${PBBAM_LIB} \
	${HDF5_LIB} \
	${HTSLIB_LIB} \
	${GCC_LIB} \
	${ZLIB_LIB}
LDLIBS+= \
	${LIBBLASR_LIBFLAGS} \
	${LIBPBDATA_LIBFLAGS} \
	${LIBPBIHDF_LIBFLAGS} \
	${PBBAM_LIBFLAGS} \
	${HDF5_LIBFLAGS} \
	${HTSLIB_LIBFLAGS} \
	${ZLIB_LIBFLAGS} \
	${PTHREAD_LIBFLAGS} \
	${DL_LIBFLAGS}
CPPFLAGS+=$(patsubst %,-I%,${INCDIRS}) $(patsubst %,-isystem%,${SYS_INCDIRS})
LDFLAGS+=$(patsubst %,-L %,${LIBDIRS})
LDFLAGS += ${EXTRA_LDFLAGS}

vpath align.c ${DALIGNER_SRC}
vpath DB.c ${DAZZ_DB_SRC}
vpath QV.c ${DAZZ_DB_SRC}

all: pbdagcon dazcon

# Technically does not need pbdata or blasr, but so what?
dazcon: $(COMMON_OBJECTS) $(DAZCON_OBJECTS)
	$(CXX) -o $@ $^ $(LDFLAGS) $(LDLIBS)

pbdagcon: $(COMMON_OBJECTS) $(PBDAGCON_OBJECTS)
	$(CXX) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(COMMON_OBJECTS) $(PBDAGCON_OBJECTS):

clean:
	$(RM) *.d
	$(RM) *.o
	$(RM) pbdagcon
	$(RM) dazcon

.PHONY: all clean

SRCS:=    $(notdir $(wildcard ${THISDIR}/*.c))
CPP_SRCS:=$(notdir $(wildcard ${THISDIR}/*.cpp))
DEPS:=$(patsubst %.c,%.d,${SRCS})
CPP_DEPS:=$(patsubst %.cpp,%.d,${CPP_SRCS})
-include ${DEPS} ${CPP_DEPS}
