.PHONY = all wii gc wii-clean gc-clean wii-run gc-run specials specials-clean specials-wii specials-gc specials-wii-clean specials-gc-clean

VERSION := 004

all: wii gc

clean: wii-clean gc-clean specials-clean

wii:
	$(MAKE) -f Makefile.wii

wii-clean:
	$(MAKE) -f Makefile.wii clean

wii-run:
	$(MAKE) -f Makefile.wii run

gc:
	$(MAKE) -f Makefile.gc

gc-clean:
	$(MAKE) -f Makefile.gc clean

gc-run:
	$(MAKE) -f Makefile.gc run

# Custom Quicksave Versions
	
specials: specials-wii specials-gc

specials-clean: specials-wii-clean specials-gc-clean

specials-wii:
	touch source/ngc/snes9xGX.h
	$(MAKE) "CUSTOMFLAGS:=-DQUICK_SAVE_SLOT=0" "LOADTYPE:='"MCSLOTA"'" "VERSION:=$(VERSION)" -f Makefile.wii
	touch source/ngc/snes9xGX.h
	$(MAKE) "CUSTOMFLAGS:=-DQUICK_SAVE_SLOT=1" "LOADTYPE:='"MCSLOTB"'" "VERSION:=$(VERSION)" -f Makefile.wii
	touch source/ngc/snes9xGX.h
	$(MAKE) "CUSTOMFLAGS:=-DQUICK_SAVE_SLOT=3" "LOADTYPE:='"SD"'" "VERSION:=$(VERSION)" -f Makefile.wii

specials-gc:
	touch source/ngc/snes9xGX.h
	$(MAKE) "CUSTOMFLAGS:=-DQUICK_SAVE_SLOT=0" "LOADTYPE:='"MCSLOTA"'" "VERSION:=$(VERSION)" -f Makefile.gc
	touch source/ngc/snes9xGX.h
	$(MAKE) "CUSTOMFLAGS:=-DQUICK_SAVE_SLOT=1" "LOADTYPE:='"MCSLOTB"'" "VERSION:=$(VERSION)" -f Makefile.gc
	touch source/ngc/snes9xGX.h
	$(MAKE) "CUSTOMFLAGS:=-DQUICK_SAVE_SLOT=3" "LOADTYPE:='"SD"'" "VERSION:=$(VERSION)" -f Makefile.gc
	touch source/ngc/snes9xGX.h
	$(MAKE) "CUSTOMFLAGS:=-DQUICK_SAVE_SLOT=4" "LOADTYPE:='"SMB"'" "VERSION:=$(VERSION)" -f Makefile.gc
	
specials-wii-clean:
	$(MAKE) "CUSTOMFLAGS:=-DQUICK_SAVE_SLOT=0" "LOADTYPE:='"MCSLOTA"'" "VERSION:=$(VERSION)" -f Makefile.wii clean
	$(MAKE) "CUSTOMFLAGS:=-DQUICK_SAVE_SLOT=1" "LOADTYPE:='"MCSLOTB"'" "VERSION:=$(VERSION)" -f Makefile.wii clean
	$(MAKE) "CUSTOMFLAGS:=-DQUICK_SAVE_SLOT=3" "LOADTYPE:='"SD"'" "VERSION:=$(VERSION)" -f Makefile.wii clean	

specials-gc-clean:
	$(MAKE) "CUSTOMFLAGS:=-DQUICK_SAVE_SLOT=0" "LOADTYPE:='"MCSLOTA"'" "VERSION:=$(VERSION)" -f Makefile.gc clean
	$(MAKE) "CUSTOMFLAGS:=-DQUICK_SAVE_SLOT=1" "LOADTYPE:='"MCSLOTB"'" "VERSION:=$(VERSION)" -f Makefile.gc clean
	$(MAKE) "CUSTOMFLAGS:=-DQUICK_SAVE_SLOT=3" "LOADTYPE:='"SD"'" "VERSION:=$(VERSION)" -f Makefile.gc clean
	$(MAKE) "CUSTOMFLAGS:=-DQUICK_SAVE_SLOT=4" "LOADTYPE:='"SMB"'" "VERSION:=$(VERSION)" -f Makefile.gc clean