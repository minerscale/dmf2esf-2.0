TARGET = out/dmf2esf
CC = gcc
AR = ar
MAKE = make
CFLAGS = -Wall -Iinc -flto -O3 -L./lib -ldmf
CHMOD = chmod

PREBUILD_DIRS = lib/ out/ out/src

.PHONY: default all clean

default: $(TARGET)
all: default

HEADERS = $(wildcard *.h)
HEADERS += $(wildcard inc/*.h)

SRC_C =  $(wildcard *.c)
SRC_C += $(wildcard src/*.c)

OBJECTS = $(addprefix out/, $(SRC_C:.c=.o))

$(PREBUILD_DIRS):
	mkdir -p $@

lib/libdmf.a:
	$(MAKE) TARGET="out/libdmf.a" CC=$(CC) AR=$(AR) -C libdmf/ all docs
	cp libdmf/out/libdmf.a lib/

$(OBJECTS): $(SRC_C) lib/libdmf.a | $(PREBUILD_DIRS)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $^ $(CFLAGS) -o $@
	$(CHMOD) +x $@

clean:
	-rm -f out/*.o
	-rm -f out/src/*.o
	-rm -f $(TARGET)
	-rm -f $(TARGET).exe
	-rm -f lib/*.a
	make -C libdmf/ clean
