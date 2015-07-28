# CPack
CPack is a simple fast library written in C for archiving files.

It can be used for game engines or archivator with SFX support.
The package can contain only the 64(look in CPack.h #define stringsize for change key lenght) byte key lenght, so it not support directories.

Support checking versions of package.

Cross-platform realization of mmap-ing. For disable memory mapping comment define in CPack.h.
For changing comression and hash algo look macros in CPack.c 

FUTURE CHANGES
Memory reading from memory.
