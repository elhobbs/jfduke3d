#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/ds_rules

# Engine source code path
ENGINEROOT ?= jfbuild
MACTROOT ?= jfmact
AUDIOLIBROOT ?= jfaudiolib
SRC = $(realpath ../)


#---------------------------------------------------------------------------------
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing extra header files
# DATA is a list of directories containing binary files
# all directories are relative to this makefile
#---------------------------------------------------------------------------------
BUILD		:=	build
SOURCES		:=	source build
INCLUDES	:=	include \
				../$(ENGINEROOT)/src \
				../$(ENGINEROOT)/include \
				../$(MACTROOT) \
				../$(AUDIOLIBROOT) \
				../$(AUDIOLIBROOT)/include \
				../$(AUDIOLIBROOT)/src \
				../src
DATA		:=

ENGINE_SRC = \
	$(ENGINEROOT)/src/a-c.c \
	$(ENGINEROOT)/src/asmprot.c \
	$(ENGINEROOT)/src/baselayer.c \
	$(ENGINEROOT)/src/cache1d.c \
	$(ENGINEROOT)/src/compat.c \
	$(ENGINEROOT)/src/crc32.c \
	$(ENGINEROOT)/src/defs.c \
	$(ENGINEROOT)/src/engine.c \
	$(ENGINEROOT)/src/kplib.c \
	$(ENGINEROOT)/src/mmulti_null.c \
	$(ENGINEROOT)/src/osd.c \
	$(ENGINEROOT)/src/pragmas.c \
	$(ENGINEROOT)/src/scriptfile.c \
	$(ENGINEROOT)/src/textfont.c \
	$(ENGINEROOT)/src/smalltextfont.c

AUDIOLIB_SRC = \
	$(AUDIOLIBROOT)/src/drivers.c \
	$(AUDIOLIBROOT)/src/fx_man.c \
	$(AUDIOLIBROOT)/src/cd.c \
	$(AUDIOLIBROOT)/src/multivoc.c \
	$(AUDIOLIBROOT)/src/mix.c \
	$(AUDIOLIBROOT)/src/mixst.c \
	$(AUDIOLIBROOT)/src/pitch.c \
	$(AUDIOLIBROOT)/src/vorbis.c \
	$(AUDIOLIBROOT)/src/music.c \
	$(AUDIOLIBROOT)/src/midi.c \
	$(AUDIOLIBROOT)/src/driver_nosound.c \
	$(AUDIOLIBROOT)/src/asssys.c


JMACT_SRC= \
	$(MACTROOT)/util_lib.c \
	$(MACTROOT)/file_lib.c \
	$(MACTROOT)/control.c \
	$(MACTROOT)/keyboard.c \
	$(MACTROOT)/mouse.c \
	$(MACTROOT)/mathutil.c \
	$(MACTROOT)/scriplib.c \
	$(MACTROOT)/animlib.c

GAME_SRC = \
	src/game.c \
	src/actors.c \
	src/gamedef.c \
	src/global.c \
	src/menues.c \
	src/player.c \
	src/premap.c \
	src/sector.c \
	src/rts.c \
	src/config.c \
	src/osdfuncs.c \
	src/osdcmds.c \
	src/grpscan.c \
	src/sounds.c \
	$(JMACTOBJ)

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-mthumb -mthumb-interwork

CFLAGS	:=	-g -Wall -O3 \
 			-march=armv5te -mtune=arm946e-s -fomit-frame-pointer \
			-ffast-math \
			$(ARCH)

CFLAGS0	:=	-g -Wall -O0 \
 			-march=armv5te -mtune=arm946e-s \
			-ffast-math \
			$(ARCH)

CFLAGS	+=	$(INCLUDE) -DARM9 -D__NDS__ -DPLATFORM_UNIX=1 -DUSE_POLYMOST=0 -DUSE_OPENGL=0 -DUSE_ASM=0
CXXFLAGS	:=	$(CFLAGS) -fno-rtti -fno-exceptions

ASFLAGS	:=	-g $(ARCH) -march=armv5te -mtune=arm946e-s

LDFLAGS	=	-specs=ds_arm9.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

#---------------------------------------------------------------------------------
# any extra libraries we wish to link with the project
#---------------------------------------------------------------------------------
LIBS	:=	-lfat -lnds9 -lm
 
#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:=	$(LIBNDS)
 
#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------
 
export ARM9ELF	:=	$(CURDIR)/$(TARGET).elf
export DEPSDIR := $(CURDIR)/$(BUILD)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir)) \
					$(SRC) \
					$(BUILD)
 



CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c))) \
				$(ENGINE_SRC) \
				$(GAME_SRC) \
				$(JMACT_SRC) \
				$(AUDIOLIB_SRC)
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))
 
#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#---------------------------------------------------------------------------------
	export LD	:=	$(CC)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

export OFILES	:=	$(addsuffix .o,$(BINFILES)) \
					$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
 
export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)
 
export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)
 
.PHONY: $(BUILD) clean
 
#---------------------------------------------------------------------------------
$(BUILD):
	@echo build $@
	@[ -d $@ ] || mkdir -p $@
	@[ -d $@/src ] || mkdir -p $@/src
	@[ -d $@/$(MACTROOT) ] || mkdir -p $@/$(MACTROOT)
	@[ -d $@/$(AUDIOLIBROOT)/src ] || mkdir -p $@/$(AUDIOLIBROOT)/src
	@[ -d $@/$(ENGINEROOT)/src ] || mkdir -p $@/$(ENGINEROOT)/src
#@echo includes $(INCLUDES)
#@echo CFILES $(CFILES)
#echo "build $(BUILD)"
	echo "const char *game_version = \""$(shell git describe --always || echo git error)"\";" > $@/version-auto.c
	echo "const char *game_date = __DATE__;" >> $@/version-auto.c
	echo "const char *game_time = __TIME__;" >> $@/version-auto.c
	echo "const char *build_version = \""$(shell git describe --always || echo git error)"\";" >> $@/version-auto.c
	echo "const char *build_date = __DATE__;" >> $@/version-auto.c
	echo "const char *build_time = __TIME__;" >> $@/version-auto.c
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
 
#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) *.elf *.nds* *.bin 

#---------------------------------------------------------------------------------
else
 
#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(ARM9ELF)	:	$(OFILES) 
	@echo linking $(notdir $@)
	@$(LD)  $(LDFLAGS) $(OFILES) $(LIBPATHS) $(LIBS) -o $@

#---------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data 
#---------------------------------------------------------------------------------
%.bin.o	:	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPSDIR)/*.d
 
#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
