CXX := $(shell which g++) -std=c++11 -W -Wall
CXXFLAGS := 
LDFLAGS := -pthread

ifeq ($(PROFILE),1)
CXXFLAGS += -g -pg -fno-inline
endif

ifneq ($(OPT),0)
CXXFLAGS += -O3
endif

# debugging on by default
ifneq ($(NDEBUG),1)
CXXFLAGS += -g
endif

# Dependency directory and flags                                                
DEPSDIR := $(shell mkdir -p .deps; echo .deps)
# MD: Dependency as side-effect of compilation                                  
# MF: File for output                                                           
# MP: Include phony targets                                                     
DEPSFILE = $(DEPSDIR)/$(notdir $*.d)
DEPSFLAGS = -MD -MF $(DEPSFILE) #-MP

%.S: %.cc
	$(CXX) $(CXXFLAGS) $(DEPSFLAGS) -S -o $@ $<

%.o: %.cc
	$(CXX) $(CXXFLAGS) $(DEPSFLAGS) -c -o $@ $<

%.o: %.c
	$(CXX) $(CXXFLAGS) $(DEPSFLAGS) -c -o $@ $<

concurrent: concurrent.o clp.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm *.o concurrent
	rm -r $(DEPSDIR)

# Include the dependency files                                                  
-include $(wildcard $(DEPSDIR)/*.d)
