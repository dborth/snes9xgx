.PHONY = all wii gc wiiu wii-clean gc-clean wiiu-clean wii-run gc-run wiiu-run

all: wii gc wiiu

run: wii-run

clean: wii-clean gc-clean wiiu-clean

wii:
	$(MAKE) -f Makefile.wii

wii-clean:
	$(MAKE) -f Makefile.wii clean

wii-run: wii
	$(MAKE) -f Makefile.wii run

gc:
	$(MAKE) -f Makefile.gc

gc-clean:
	$(MAKE) -f Makefile.gc clean

gc-run: gc
	$(MAKE) -f Makefile.gc run

wiiu:
	$(MAKE) -f Makefile.wiiu

wiiu-clean:
	$(MAKE) -f Makefile.wiiu clean

wiiu-run: wii
	$(MAKE) -f Makefile.wiiu run
