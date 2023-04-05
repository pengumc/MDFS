LIB_C_FILES = lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c lfunc.c lgc.c llex.c lmem.c lobject.c lopcodes.c lparser.c lstate.c lstring.c ltable.c ltm.c lundump.c lvm.c lzio.c lauxlib.c lbaselib.c lcorolib.c ldblib.c lmathlib.c loadlib.c loslib.c lstrlib.c ltablib.c lutf8lib.c linit.c 
LIB_O_FILES = $(foreach f, $(LIB_C_FILES), lua/$(basename $(f)).o)


all: $(LIB_O_FILES)
	gcc -Wall -I. -Isoftware $(LIB_O_FILES) lua/lua.c software/mdfs/MDFS.c software/mdfs/MDFS_lua.c -o lua.exe

%.o : %.c
	gcc -Wall -Isoftware -c $< -o $@