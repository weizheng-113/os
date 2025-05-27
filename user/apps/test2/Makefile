all:
	$(CC) $(GLOBAL_CFLAGS) -I$(CWD)/apps/aeui -I$(CWD)/apps/lvgl -c test.c -o test.o
	ld test.o $(CWD)/apps/lvgl/lib/*.a $(shell find $(CWD)/obj/ -name "*.o") -o test2.exec	

clean:
	rm -rf *.o *.exec