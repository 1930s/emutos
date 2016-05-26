EmuTOS - 512 KB version

This ROM is suitable for the following hardware:
- TT
- Falcon
- emulators of the above

Note: Extra hardware is autodetected.

This ROM is the optimal one for Hatari, with any hardware combination.

Note: There is a bug in special Hatari 1.9.0 variants which prevents IDE
working in some conditions. If you experience trouble, select 68030 CPU and
disable the "Cycle exact" option.

Some legacy programs don't work with 512k TOS images, for those one may
need to use a smaller EmuTOS image.

etos512k.img - Multilanguage
etos512k.sym - Symbol address information for Hatari debugger and profiler

Additional information on debugging EmuTOS and its software compatibility
is included with Hatari:
http://hg.tuxfamily.org/mercurialroot/hatari/hatari/raw-file/tip/doc/emutos.txt

The default language is English.
Other supported languages are:
- Czech
- German
- Spanish
- Finnish
- French
- Greek
- Italian
- Norwegian
- Russian
- Swiss German
They can be used by setting the NVRAM appropriately.

Note that selecting Norwegian currently sets the language to English, but the
keyboard layout to Norwegian.

This ROM image has been built using:
make 512

