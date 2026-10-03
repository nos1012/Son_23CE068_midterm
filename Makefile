CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
LDFLAGS ?=

TARGET = ls
OBJECTS = main.o options.o list.o format.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS)

main.o: main.c ls.h options.h list.h
options.o: options.c options.h ls.h
list.o: list.c list.h format.h ls.h
format.o: format.c format.h ls.h

clean:
	$(RM) $(TARGET) $(OBJECTS)
