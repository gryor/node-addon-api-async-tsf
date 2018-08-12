comma := ,
empty:=
space:= $(empty) $(empty)
destdir ?=
prefix ?= /usr
installdir := ${destdir}${prefix}
.DEFAULT_GOAL := all

node_version= $(shell node --version)

build/node-addon-api-async-tsf/obj/all/src/main.c.o: build/node-addon-api-async-tsf/obj/all/%.o: %
	mkdir -p ${dir $@}
	gcc -fPIC -g -ggdb -rdynamic -pthread -O0 -Wall -Werror -Wfatal-errors -std=gnu11 -Wstrict-prototypes -Wno-misleading-indentation -DPIC -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -DEV_MULTIPLICITY=0 -I/usr/include/node -Inode_modules -c $< -o $@

all: build/node-addon-api-async-tsf/obj/all/src/main.c.o
	mkdir -p build/node-addon-api-async-tsf/lib
	gcc -shared -fPIC -Wl,-soname,libasync_tsf.so.0 -DPIC -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -DEV_MULTIPLICITY=0 -l:libm.so -l:libpthread.so -l:libdl.so -l:libv8.so build/node-addon-api-async-tsf/obj/all/src/main.c.o -o build/node-addon-api-async-tsf/lib/async_tsf.node

clean-all: 
	rm -rf build/node-addon-api-async-tsf/obj/all
	rmdir --ignore-fail-on-non-empty build/node-addon-api-async-tsf/obj

compile-commands: 
	cd .; bear --append -o build/compile_commands.json make

clean: clean-all
