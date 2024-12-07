# Executables
CC=m68k-palmos-gcc
OBJ_RES=m68k-palmos-obj-res
PRC=build-prc

TARGET=hello
DB_NAME="Hello, World"
CRID=WRLD

SRC=hello.c

all: build/${TARGET}.prc

build:
	mkdir -p build

build/${TARGET}.bin: ${SRC} | build
	${CC} ${SRC} -o build/${TARGET}.bin

build/*.${TARGET}.bin.grc: build/${TARGET}.bin
	cd build
	${OBJ_RES} ${TARGET}.bin

build/${TARGET}.prc: build/*.grc
	${PRC} build/${TARGET}.prc ${DB_NAME} ${CRID} build/*.grc

clean:
	rm -rf build

.PHONY: all clean