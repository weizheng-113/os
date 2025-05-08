all:
	$(CC) $(GLOBAL_CFLAGS) -I$(CWD)/apps/aeui -I$(CWD)/apps/lvgl -c aeui.c -o aeui.o
	$(CC) $(GLOBAL_CFLAGS) -I$(CWD)/apps/aeui -I$(CWD)/apps/lvgl -c window.c -o window.o
	ld aeui.o window.o $(CWD)/apps/lvgl/lib/*.a $(shell find $(CWD)/obj/ -name "*.o" ! -name "crt0.S.o" ! -name "start.c.o") -o aeui.exec	

clean:
	rm -rf *.o *.exec
