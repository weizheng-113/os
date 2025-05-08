export CC = clang
export CWD = $(shell pwd)
export GLOBAL_CFLAGS = -static -nostdinc -nostdlib -mno-red-zone -fno-builtin -fno-stack-protector -g -O0 -I$(CWD)/alibc/include

all:
	$(MAKE) -C alibc

	$(MAKE) -C apps

clean:
	$(MAKE) -C apps clean
	@rm -rf $(CWD)/obj
