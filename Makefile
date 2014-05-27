CXX := $(shell which g++) -std=c++11
CXXFLAGS := -O3
LDFLAGS := -pthread

ifeq ($(PROFILE),1)
CXXFLAGS += -g -pg -fno-inline
endif

ifeq ($(DEBUG),1)
CXXFLAGS += -g
endif

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

concurrent: concurrent.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm *.o concurrent
