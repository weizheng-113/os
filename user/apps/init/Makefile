all:
	$(CC) $(GLOBAL_CFLAGS) -c init.c -o init.o	
	ld init.o $(shell find $(CWD)/obj/ -name "*.o" ! -name "crt0.S.o" ! -name "start.c.o") -o init.exec	

clean:
	rm -rf *.o *.exec
