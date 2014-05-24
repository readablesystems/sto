CXX := $(shell which g++) -std=c++11
CXXFLAGS := -O3

ifeq ($(PROFILE),1)
CXXFLAGS += -emit-llvm -g
endif

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

concurrent: concurrent.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm *.o concurrent