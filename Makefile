
CC=gcc
CFLAGS=-c -Wall
LDFLAGS=-lpthread -lrt

SOURCES=main.c 
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=thread_test

.PHONY=tags

all: $(SOURCES) $(EXECUTABLE)

tags: $(SOURCES)
	cscope -b $(SOURCES)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@ 

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)
