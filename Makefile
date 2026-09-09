# $VER: makefile 1.0 (25.05.2026)
 
VERSION     =   1

CC = ppc-morphos-g++
CFLAGS = -noixemul -nostartfiles -O2 -fomit-frame-pointer -mno-prototype -mcpu=604e -mmultiple
CFLAGS += -W -Wall -Wpointer-arith -Wno-parentheses -fno-exceptions
LD = ppc-morphos-g++
LDFLAGS = -nostartfiles -noixemul
LIBS = -labox -lm -lmath
STRIP = ppc-morphos-strip --remove-section .comment
VER := $(shell grep "define VERS " class_version.h | cut -d " " -f 5 | tr -d "\042" )
OUTPUT = sid.demuxer
OBJS = class.o

#===============================================================================

.PHONY: all  clean install

all: $(OUTPUT)


clean:
	-rm -rf $(OBJS) *.bak *.s *.db $(OUTPUT) recognize

install: all
	mkdir -p /SYS/MorphOS/Classes/Multimedia
	cp $(OUTPUT) /SYS/MorphOS/Classes/Multimedia/
	-flushlib $(OUTPUT)

#===============================================================================

$(OUTPUT).db: $(OBJS) recognize
	$(LD) $(LDFLAGS) $(OBJS) $(LIBS) -o $(OUTPUT).db
	ppc-morphos-objcopy --add-section .dtcode=recognize $(OUTPUT).db
	ppc-morphos-objcopy --set-section-flags .dtcode=readonly $(OUTPUT).db

$(OUTPUT): $(OUTPUT).db
	$(STRIP) $(OUTPUT).db -o $(OUTPUT)

recognize.db: recognize.c
	ppc-morphos-gcc -s -O2 -nostdlib -o recognize.db recognize.c -labox

recognize: recognize.db
	ppc-morphos-objcopy -R .comment recognize.db recognize

#===============================================================================

class.o: class.cpp class_version.h
	$(CC) $(CFLAGS) -c $<

