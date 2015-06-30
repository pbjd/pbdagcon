include pbi.mk
include boost.mk

COMMON_OBJECTS := Alignment.o AlnGraphBoost.o
PBDAGCON_OBJECTS := BlasrM5AlnProvider.o main.o SimpleAligner.o
DAZCON_OBJECTS := QV.o DB.o align.o DazAlnProvider.o dazcon.o

CXXFLAGS += -O3 -std=c++11 -Wall -Wuninitialized -pedantic -I third-party \
		   -I $(BOOST_HEADERS) -I ../../daligner

CFLAGS = -O3 -Wall -Wextra -fno-strict-aliasing

all: pbdagcon

dazcon: LDLIBS = -lpthread
dazcon: $(COMMON_OBJECTS) $(DAZCON_OBJECTS)
	$(CXX) -Wl,--no-as-needed -o $@ $^ $(LDLIBS)

# We are moving to BAM, which requires extra lib support when compiling against
# libblasr.  This conditional allows backward compatable compilations with 
# PacificBiosciences/blasr_libcpp.
ifeq ($(wildcard $(PBBAM)/*),)
pbdagcon: LDLIBS = -lpbdata -lblasr -lpthread
else
pbdagcon: LDLIBS = -lpbdata -lblasr -lpbbam -lhts -lz -lpthread
endif
pbdagcon: CXXFLAGS += $(INCDIRS)
pbdagcon: $(COMMON_OBJECTS) $(PBDAGCON_OBJECTS)
	$(CXX) -Wl,--no-as-needed $(LIBDIRS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(COMMON_OBJECTS): $(BOOST_HEADERS)

$(BOOST_HEADERS):
	cd third-party && $(GET_BOOST)

clean:
	$(RM) *.d
	$(RM) *.o
	$(RM) pbdagcon
	$(RM) dazcon

.PHONY: all clean
