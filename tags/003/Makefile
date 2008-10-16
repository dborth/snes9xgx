.PHONY = all wii gc wii-clean gc-clean wii-run gc-run mca mcb sd smb mca-clean mcb-clean sd-clean smb-clean specials specials-clean

VERSION := 003

all: wii gc

clean: wii-clean gc-clean

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
	
specials: sd mcb mca smb

specials-clean: mca-clean mcb-clean smb-clean sd-clean

mca:
	touch source/ngc/snes9xGX.h
	$(MAKE) "CUSTOMFLAGS:=-DQUICK_SAVE_SLOT=0" "LOADTYPE:='"MCSLOTA"'" "VERSION:=$(VERSION)" -f Makefile.wii
	touch source/ngc/snes9xGX.h
	$(MAKE) "CUSTOMFLAGS:=-DQUICK_SAVE_SLOT=0" "LOADTYPE:='"MCSLOTA"'" "VERSION:=$(VERSION)" -f Makefile.gc
mca-clean:
	$(MAKE) "CUSTOMFLAGS:=-DQUICK_SAVE_SLOT=0" "LOADTYPE:='"MCSLOTA"'" "VERSION:=$(VERSION)" -f Makefile.wii clean
	$(MAKE) "CUSTOMFLAGS:=-DQUICK_SAVE_SLOT=0" "LOADTYPE:='"MCSLOTA"'" "VERSION:=$(VERSION)" -f Makefile.gc clean
	
mcb:
	touch source/ngc/snes9xGX.h
	$(MAKE) "CUSTOMFLAGS:=-DQUICK_SAVE_SLOT=1" "LOADTYPE:='"MCSLOTB"'" "VERSION:=$(VERSION)" -f Makefile.wii
	touch source/ngc/snes9xGX.h
	$(MAKE) "CUSTOMFLAGS:=-DQUICK_SAVE_SLOT=1" "LOADTYPE:='"MCSLOTB"'" "VERSION:=$(VERSION)" -f Makefile.gc
mcb-clean:
	$(MAKE) "CUSTOMFLAGS:=-DQUICK_SAVE_SLOT=1" "LOADTYPE:='"MCSLOTB"'" "VERSION:=$(VERSION)" -f Makefile.wii clean
	$(MAKE) "CUSTOMFLAGS:=-DQUICK_SAVE_SLOT=1" "LOADTYPE:='"MCSLOTB"'" "VERSION:=$(VERSION)" -f Makefile.gc clean
	
sd:
	touch source/ngc/snes9xGX.h
	$(MAKE) "CUSTOMFLAGS:=-DQUICK_SAVE_SLOT=3" "LOADTYPE:='"SD"'" "VERSION:=$(VERSION)" -f Makefile.wii
	touch source/ngc/snes9xGX.h
	$(MAKE) "CUSTOMFLAGS:=-DQUICK_SAVE_SLOT=3" "LOADTYPE:='"SD"'" "VERSION:=$(VERSION)" -f Makefile.gc
sd-clean:
	$(MAKE) "CUSTOMFLAGS:=-DQUICK_SAVE_SLOT=3" "LOADTYPE:='"SD"'" "VERSION:=$(VERSION)" -f Makefile.wii clean
	$(MAKE) "CUSTOMFLAGS:=-DQUICK_SAVE_SLOT=3" "LOADTYPE:='"SD"'" "VERSION:=$(VERSION)" -f Makefile.gc clean

smb:
	touch source/ngc/snes9xGX.h
	$(MAKE) "CUSTOMFLAGS:=-DQUICK_SAVE_SLOT=4" "LOADTYPE:='"SMB"'" "VERSION:=$(VERSION)" -f Makefile.gc
smb-clean:
	$(MAKE) "CUSTOMFLAGS:=-DQUICK_SAVE_SLOT=4" "LOADTYPE:='"SMB"'" "VERSION:=$(VERSION)" -f Makefile.gc clean