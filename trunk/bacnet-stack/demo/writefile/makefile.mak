#
# Simple makefile to build an executable for Win32
#
# This makefile assumes Borland bcc32 development environment
# on Windows NT/9x/2000/XP
#

!ifndef BORLAND_DIR
BORLAND_DIR_Not_Defined:
   @echo .
   @echo You must define environment variable BORLAND_DIR to compile.
!endif

PRODUCT = writefile
PRODUCT_EXE = $(PRODUCT).exe

# Choose the Data Link Layer to Enable
DEFINES = -DBACDL_BIP=1

SRCS = writefile.c \
       ..\..\ports\win32\bip-init.c \
       ..\..\bip.c  \
       ..\..\demo\handler\txbuf.c  \
       ..\..\demo\handler\noserv.c  \
       ..\..\demo\handler\h_whois.c  \
       ..\..\demo\handler\h_rp.c  \
       ..\..\bacdcode.c \
       ..\..\bacapp.c \
       ..\..\bacstr.c \
       ..\..\bactext.c \
       ..\..\indtext.c \
       ..\..\bigend.c \
       ..\..\whois.c \
       ..\..\iam.c \
       ..\..\rp.c \
       ..\..\wp.c \
       ..\..\arf.c \
       ..\..\awf.c \
       ..\..\demo\object\bacfile.c \
       ..\..\demo\object\device.c \
       ..\..\demo\object\ai.c \
       ..\..\demo\object\ao.c \
       ..\..\datalink.c \
       ..\..\tsm.c \
       ..\..\address.c \
       ..\..\abort.c \
       ..\..\reject.c \
       ..\..\bacerror.c \
       ..\..\apdu.c \
       ..\..\npdu.c

OBJS = $(SRCS:.c=.obj)

# Compiler definitions
#
BCC_CFG = bcc32.cfg
CC = $(BORLAND_DIR)\bin\bcc32 +$(BCC_CFG)
#LINK = $(BORLAND_DIR)\bin\tlink32
LINK = $(BORLAND_DIR)\bin\ilink32
TLIB = $(BORLAND_DIR)\bin\tlib

#
# Include directories
#
CC_DIR     = $(BORLAND_DIR)\BIN
INCL_DIRS = -I$(BORLAND_DIR)\include;..\..\;..\..\demo\object\;..\..\demo\handler\;..\..\ports\win32\;.

CFLAGS = $(INCL_DIRS) $(CS_FLAGS) $(DEFINES)

# Libraries
#
C_LIB_DIR = $(BORLAND_DIR)\lib

LIBS = $(C_LIB_DIR)\IMPORT32.lib \
$(C_LIB_DIR)\CW32MT.lib

#
# Main target
#
# This should be the first one in the makefile

all : $(BCC_CFG) $(PRODUCT_EXE)

# Linker specific: the link below is for BCC linker/compiler. If you link
# with a different linker - please change accordingly.
#

# need a temp response file (@&&) because command line is too long
$(PRODUCT_EXE) : $(OBJS)
	@echo Running Linker for $(PRODUCT_EXE)
	$(LINK)	-L$(C_LIB_DIR) -m -c -s -v @&&| # temp response file, starts with |
	  $(BORLAND_DIR)\lib\c0x32.obj $**  # $** lists each dependency
	$<
	$*.map
	$(LIBS)
| # end of temp response file

#
# Utilities

clean :
	@echo Deleting obj files, $(PRODUCT_EXE) and map files.
#	del $(OBJS) # command too long, bummer!
	del *.obj
	del ..\..\*.obj
	del ..\..\demo\handler\*.obj
	del ..\..\demo\object\*.obj
	del ..\..\ports\win32\*.obj
	del $(PRODUCT_EXE)
	del *.map
	del $(BCC_CFG)

#
# Generic rules
#
.SUFFIXES: .cpp .c .sbr .obj

#
# cc generic rule
#
.c.obj:
	$(CC) -o$@ $<

# Compiler configuration file
$(BCC_CFG) :
   Copy &&|
$(CFLAGS) 
-c 
-y     #include line numbers in OBJ's
-v     #include debug info
-w+    #turn on all warnings
-Od    #disable all optimizations
#-a4    #32 bit data alignment
#-M     # generate link map
#-ls    # linker options
#-WM-   #not multithread
-WM    #multithread
-w-aus # ignore warning assigned a value that is never used
-w-sig # ignore warning conversion may lose sig digits
| $@

# EOF: makefile
