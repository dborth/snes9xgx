@REM gcc -c -DEXECUTE_SUPERFX_PER_LINE -DMICROSOFT_C -DSPC700_C -DCPU_SHUTDOWN -DSPC700_SHUTDOWN -Ii386 i386/cpuexec.S -o i386/cpuexec.obj
@REM gcc -c -DEXECUTE_SUPERFX_PER_LINE -DMICROSOFT_C -DSPC700_C -DCPU_SHUTDOWN -DSPC700_SHUTDOWN -Ii386 i386/cpuops.S -o i386/cpuops.obj
@REM gcc -c -DEXECUTE_SUPERFX_PER_LINE -DMICROSOFT_C -DSPC700_C -DCPU_SHUTDOWN -DSPC700_SHUTDOWN -Ii386 i386/sa1ops.S -o i386/sa1ops.obj
nasmw -d__DJGPP__=1 -dZSNES_FX -f win32 -i . -i i386 -o i386/zsnes.obj i386/zsnes.asm
nasmw -d__DJGPP__=1 -dZSNES_FX -f win32 -i . -i i386 -o i386/fxemu2b.obj i386/fxemu2b.asm
nasmw -d__DJGPP__=1 -dZSNES_FX -f win32 -i . -i i386 -o i386/fxtable.obj i386/fxtable.asm
nasmw -d__DJGPP__=1 -dZSNES_FX -f win32 -i . -i i386 -o i386/fxemu2.obj i386/fxemu2.asm
nasmw -d__DJGPP__=1 -dZSNES_FX -f win32 -i . -i i386 -o i386/fxemu2c.obj i386/fxemu2c.asm
nasmw -d__DJGPP__=1 -dZSNES_FX -f win32 -i . -i i386 -o i386/sfxproc.obj i386/sfxproc.asm
nasmw -d__DJGPP__=1 -dZSNES_FX -f win32 -i . -i i386 -o i386/spc.obj i386/spc.asm
nasmw -d__DJGPP__=1 -dZSNES_FX -f win32 -i . -i i386 -o i386/zsnesc4.obj i386/zsnesc4.asm
@REM nasmw -d__DJGPP__=1 -dZSNES_FX -f win32 -i . -i i386 -o i386/c4.obj i386/c4.asm
nasmw -d__DJGPP__=1 -dZSNES_FX -f win32 -i . -i i386 -o i386/2xsaimmx.obj i386/2xsaimmx.asm
nasmw -d__DJGPP__=1 -dZSNES_FX -f win32 -i . -i i386 -o i386/bilinear.obj i386/bilinear.asm
