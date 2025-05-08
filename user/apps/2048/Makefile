all:
	$(CC) $(GLOBAL_CFLAGS) -I$(CWD)/apps/lvgl -c 2048.c -o 2048.o
	ld 2048.o $(CWD)/apps/lvgl/lib/*.a $(shell find $(CWD)/obj/ -name "*.o") -o 2048.exec

clean:
	rm -rf *.o *.exec
