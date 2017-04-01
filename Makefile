# Compiler definitions
CROSS_COMPILE ?= avr-
CC = $(CROSS_COMPILE)gcc
OBJCOPY = $(CROSS_COMPILE)objcopy
MCU ?= attiny85
CFLAGS ?= -mmcu=$(MCU) -Os
CPPFLAGS ?= -DF_CPU=8000000 -D_POLY_CFG=\"poly_cfg.h\"
LDFLAGS ?= -mmcu=$(MCU) -Os -Wl,--as-needed

all: synth.hex

%.hex: %.elf
	$(OBJCOPY) -j .text -j .data -O ihex $< $@

synth.elf: main.o poly.o
	$(CC) -o $@ $(LDFLAGS) $^

poly.o: poly.h
main.o: poly.h

%.E: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ -E $^

clean:
	-rm -f *.o *.hex *.elf
