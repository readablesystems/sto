CPPSRCS		= main.cpp cpp_framework.cpp ITest.cpp
TARGET		= test_intel64

CPP		= g++

CPPFLAGS	= -O3 -m64 -DNDEBUG -DINTEL64 -D_REENTRANT -lrt -pthread  
LFLAGS		= -O3 -m64 -DNDEBUG -DINTEL64 -D_REENTRANT -lrt -pthread

OBJS		= $(CPPSRCS:.cpp=.o)

all: $(TARGET)

main.o:
	$(CPP) $(CPPFLAGS) -c ./test/main.cpp

cpp_framework.o:
	$(CPP) $(CPPFLAGS) -c ./framework/cpp_framework.cpp

ITest.o:
	$(CPP) $(CPPFLAGS) -c ./data_structures/ITest.cpp

$(TARGET): $(OBJS)
	$(CPP) $(LFLAGS) $(OBJS) -o $(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

depend:
	mkdep $(SRCS)
