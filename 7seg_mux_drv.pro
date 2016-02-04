TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

INCLUDEPATH += \
	lib \
	/usr/avr/include

DEFINES += __AVR_ATmega328P__ F_CPU=16000000UL

DISTFILES += \
    style.astylerc \
    Makefile

HEADERS += \
    lib/adc.h \
    lib/blockdev.h \
    lib/calc.h \
    lib/color.h \
    lib/debounce.h \
    lib/dht11.h \
    lib/fat16.h \
    lib/fat16_internal.h \
    lib/iopins.h \
    lib/lcd.h \
    lib/nsdelay.h \
    lib/onewire.h \
    lib/sd.h \
    lib/sd_blockdev.h \
    lib/sd_fat.h \
    lib/sipo_pwm.h \
    lib/sonar.h \
    lib/spi.h \
    lib/stream.h \
    lib/uart.h \
    lib/wsrgb.h

SOURCES += \
    lib/adc.c \
    lib/color.c \
    lib/debounce.c \
    lib/dht11.c \
    lib/fat16.c \
    lib/iopins.c \
    lib/lcd.c \
    lib/onewire.c \
    lib/sd.c \
    lib/sd_blockdev.c \
    lib/sd_fat.c \
    lib/sipo_pwm.c \
    lib/sonar.c \
    lib/spi.c \
    lib/stream.c \
    lib/uart.c \
    lib/wsrgb.c \
    lib/main.c \
    main.c
