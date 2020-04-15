
CC= g++
CPPFLAGS= -O2 -std=c++11 -fPIC -pedantic -Wall -Werror -L.libs -L$(OFFLINE_MAIN)/lib \
	-I. -I$(ROOTSYS)/include -I$(OFFLINE_MAIN)/include -I$(OFFLINE_MAIN)/include/eigen3

LDFLAGS= -shared -llog4cpp

SOURCES = $(shell echo *.cc)
HEADERS = $(shell echo *.h)
OBJECTS=$(SOURCES:.cc=.o)

TARGET=libPHTpcTracker.so

.PHONY : all
all: $(TARGET)

.PHONY : clean
clean:
	$(RM) $(OBJECTS) $(TARGET)
	$(RM) $(wildcard *.d)

$(TARGET) : $(OBJECTS)
	$(CC) $(CPPFLAGS) $(OBJECTS) -o $@ $(LDFLAGS)