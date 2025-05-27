all:
	$(MAKE) -C shell
	$(MAKE) -C init
	sh build_lvgl.sh
	$(MAKE) -C aeui
	$(MAKE) -C 2048
	$(MAKE) -C test
	$(MAKE) -C test1
	$(MAKE) -C test2

clean:
	$(MAKE) -C shell/pl_readline clean
	$(MAKE) -C shell clean
	$(MAKE) -C aeui clean
	$(MAKE) -C test clean
	$(MAKE) -C test1 clean
	$(MAKE) -C test2 clean
