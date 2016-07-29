# -*- makefile -*-
# --------------------------------------------------------------------
#
# Building Ipe --- common definitions
#
# --------------------------------------------------------------------
# Are we compiling for Windows?  For Mac OS X?
ifdef COMSPEC
  WIN32	=  1
  IPEBUNDLE = 1
  IPEUI = WIN32
else ifdef IPECROSS
  WIN32 = 1
  IPEBUNDLE = 1
  IPEUI = WIN32
else
  UNAME = $(shell uname)
  ifeq "$(UNAME)" "Darwin"
    MACOS = 1
    IPEUI = COCOA
    IPECONFIGMAK ?= macos.mak
  else
    IPEUI ?= QT
    IPECONFIGMAK ?= config.mak
  endif
endif
# --------------------------------------------------------------------
# IPESRCDIR is Ipe's top "src" directory
# if 'common.mak' is included on a different level than a subdirectory
# of "src", then IPESRCDIR must be set before including 'common.mak'.

IPESRCDIR ?= ..

# --------------------------------------------------------------------
# Read configuration options (not used on Win32)

ifndef WIN32
  include $(IPESRCDIR)/$(IPECONFIGMAK)
  BUILDDIR = $(IPESRCDIR)/../build
endif

# --------------------------------------------------------------------

# set variables that depend on UI library

# -------------------- QT --------------------
ifeq ($(IPEUI),QT)
CPPFLAGS     += -DIPEUI_QT
# Qt5 requires -fPIC depending on its own compilation settings.
CPPFLAGS     += -fPIC
CXXFLAGS     += -fPIC
DLL_CFLAGS   = -fPIC
IPEUI_QT     := 1
UI_CFLAGS    = $(QT_CFLAGS)
UI_LIBS      = $(QT_LIBS)
moc_sources  = $(addprefix moc_, $(subst .h,.cpp,$(moc_headers)))
all_sources  = $(sources) $(qt_sources)
objects      = $(addprefix $(OBJDIR)/, $(subst .cpp,.o,$(all_sources) \
		$(moc_sources)))
# -------------------- WIN32 --------------------
else ifeq ($(IPEUI), WIN32)
CPPFLAGS     += -DIPEUI_WIN32
IPEUI_WIN32  := 1
UI_CFLAGS    := 
UI_LIBS      := -lcomctl32 -lcomdlg32 -lgdi32 -lgdiplus
all_sources  = $(sources) $(win_sources)
objects      = $(addprefix $(OBJDIR)/, $(subst .cpp,.o,$(all_sources)))
# -------------------- COCOA --------------------
else ifeq ($(IPEUI), COCOA)
CPPFLAGS     += -DIPEUI_COCOA
IPEUI_COCOA  := 1
CXXFLAGS     += -mmacosx-version-min=10.8 -Wdeprecated-declarations
LDFLAGS      += -mmacosx-version-min=10.8
UI_CFLAGS    = $(IPEOBJCPP)
UI_LIBS      = -framework Cocoa -framework AppKit \
	-framework ApplicationServices
all_sources  = $(sources) $(cocoa_sources)
objects      = $(addprefix $(OBJDIR)/, $(subst .cpp,.o,$(all_sources)))
# -------------------- GTK --------------------
else ifeq ($(IPEUI), GTK)
CPPFLAGS     += -DIPEUI_GTK -DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED
IPEUI_GTK    := 1
GTK_CFLAGS   ?= $(shell pkg-config --cflags gtk+-2.0)
GTK_LIBS     ?= $(shell pkg-config --libs gtk+-2.0)
UI_CFLAGS    = $(GTK_CFLAGS)
UI_LIBS      = $(GTK_LIBS)
all_sources  = $(sources) $(gtk_sources)
BUILDDIR     = $(IPESRCDIR)/../gtkbuild
objects      = $(addprefix $(OBJDIR)/, $(subst .cpp,.o,$(all_sources)))
else
  error("Unknown IPEUI selected")
endif

CPPFLAGS += -std=c++11
CXXFLAGS += -Wall -Wno-sign-conversion -Wno-inconsistent-missing-override

ifdef IPESTRICT
  CXXFLAGS += -Werror -DIPESTRICT
endif

ifdef IPECXX
  CXX = $(IPECXX)
endif

DEPEND ?= $(OBJDIR)/depend.mak

.PHONY: clean 
.PHONY: install
.PHONY: all

# Variables depending on platform
ifdef WIN32
  # -------------------- WIN32 --------------------
  # Set just in case user has environment variables set
  IPEDOCDIR	 :=
  IPEICONDIR	 :=

  CPPFLAGS	 += -DWIN32 -DUNICODE -D_UNICODE
  DLL_LDFLAGS	 += -shared 
  PLUGIN_LDFLAGS += -shared
  DL_LIBS	 := 
  JPEG_CFLAGS   := 
  JPEG_LIBS     := -lgdiplus
  PNG_CFLAGS	:= 
  PNG_LIBS	:= -lgdiplus

  buildlib	 = $(BUILDDIR)/bin
  buildbin       = $(BUILDDIR)/bin
  buildipelets   = $(BUILDDIR)/ipelets
  exe_target	 = $(BUILDDIR)/bin/$1.exe
  dll_target     = $(buildlib)/$1.dll
  soname         = 
  dll_symlinks   = 
  install_symlinks = 
  ipelet_target  = $(BUILDDIR)/ipelets/$1.dll

  BUILDDIR       = $(IPESRCDIR)/../mingw
  CXXFLAGS	 += -g -O2 -fno-rtti -fno-exceptions
ifdef IPECROSS
  # --------------- Cross compiling with Mingw-w64 ---------------
  # http://mingw-w64.sourceforge.net/
  CXX           = i686-w64-mingw32-g++
  CC            = i686-w64-mingw32-gcc
  STRIP_TARGET  = i686-w64-mingw32-strip $(TARGET)
  WINDRES	= i686-w64-mingw32-windres
  IPEDEPS	?= /sw/mingwlibs
else
  # --------------- Compiling with Mingw-w64 under Windows ---------------
  WINDRES	= windres.exe
  CXXFLAGS	+= -g -O2
  STRIP_TARGET  = strip $(TARGET)
  IPEDEPS	?= /mingwlibs
endif
  ZLIB_CFLAGS   := -I$(IPEDEPS)/include
  ZLIB_LIBS     := -L$(IPEDEPS)/lib -lz
  FREETYPE_CFLAGS := -I$(IPEDEPS)/include/freetype2 \
       -I$(IPEDEPS)/include
  FREETYPE_LIBS := -L$(IPEDEPS)/lib -lfreetype
  CAIRO_CFLAGS  := -I$(IPEDEPS)/include/cairo
  CAIRO_LIBS    := -L$(IPEDEPS)/lib -lcairo
  LUA_CFLAGS    := -I$(IPEDEPS)/lua53/include
  LUA_LIBS      := $(IPEDEPS)/lua53/lua53.dll

else
ifdef MACOS
  # -------------------- Mac OS X --------------------
  CXXFLAGS	  += -g -Os -stdlib=libc++
  LDFLAGS	  += -stdlib=libc++
  IPEOBJCPP       = -x objective-c++ -fobjc-arc 
  DLL_LDFLAGS	  += -dynamiclib 
  PLUGIN_LDFLAGS  += -bundle
  DL_LIBS         ?= -ldl
  ZLIB_CFLAGS     ?=
  ZLIB_LIBS       ?= -lz
  JPEG_CFLAGS     ?= 
  JPEG_LIBS       ?= -framework ApplicationServices
  ifdef IPEBUNDLE
    IPELIBDIRINFO = @executable_path/../Frameworks
    BUNDLEDIR     ?= $(BUILDDIR)/Ipe.app/Contents
    RESOURCEDIR   = $(BUNDLEDIR)/Resources
    buildlib	  = $(BUNDLEDIR)/Frameworks
    buildbin      = $(BUNDLEDIR)/MacOS
    buildipelets  = $(RESOURCEDIR)/ipelets
    exe_target    = $(BUNDLEDIR)/MacOS/$1
    ipelet_target = $(RESOURCEDIR)/ipelets/$1.so
    IPEAPP	  = 1
  else
    IPELIBDIRINFO ?= $(IPELIBDIR)
    buildlib	  = $(BUILDDIR)/lib
    exe_target    = $(BUILDDIR)/bin/$1
    ipelet_target = $(BUILDDIR)/ipelets/$1.so
    buildbin      = $(BUILDDIR)/bin
    buildipelets  = $(BUILDDIR)/ipelets
  endif
  soname          = -Wl,-dylib_install_name,$(IPELIBDIRINFO)/lib$1.$(IPEVERS).dylib
  dll_target      = $(buildlib)/lib$1.$(IPEVERS).dylib
  dll_symlinks    = ln -sf lib$1.$(IPEVERS).dylib $(buildlib)/lib$1.dylib
  install_symlinks = ln -sf lib$1.$(IPEVERS).dylib \
		$(INSTALL_ROOT)$(IPELIBDIR)/lib$1.dylib
else	
  # -------------------- Unix --------------------
  CXXFLAGS	 += -g -O2
  DLL_LDFLAGS	 += -shared 
  PLUGIN_LDFLAGS += -shared
  soname         = -Wl,-soname,lib$1.so.$(IPEVERS)
  dll_target     = $(buildlib)/lib$1.so.$(IPEVERS)
  dll_symlinks   = ln -sf lib$1.so.$(IPEVERS) $(buildlib)/lib$1.so
  install_symlinks = ln -sf lib$1.so.$(IPEVERS) \
		$(INSTALL_ROOT)$(IPELIBDIR)/lib$1.so
  buildlib	 = $(BUILDDIR)/lib
  buildbin       = $(BUILDDIR)/bin
  buildipelets   = $(BUILDDIR)/ipelets
  exe_target	 = $(BUILDDIR)/bin/$1
  ipelet_target  = $(BUILDDIR)/ipelets/$1.so
endif
endif

# Macros

INSTALL_DIR = install -d
INSTALL_FILES = install -m 0644
INSTALL_SCRIPTS = install -m 0755
INSTALL_PROGRAMS = install -m 0755

MAKE_BINDIR = mkdir -p $(buildbin)

MAKE_LIBDIR = mkdir -p $(buildlib)

MAKE_IPELETDIR = mkdir -p $(buildipelets)

MAKE_DEPEND = \
	mkdir -p $(OBJDIR); \
	echo "" > $@; \
	for f in $(all_sources); do \
	$(CXX) -MM -MT $(OBJDIR)/$${f%%.cpp}.o $(CPPFLAGS) $$f >> $@; done

# The rules

$(OBJDIR)/%.o:  %.cpp
	@echo Compiling $(<F)...
	$(COMPILE.cc) -o $@ $<

ifdef IPEUI_QT
moc_%.cpp:  %.h
	@echo Running moc on $(<F)...
	$(MOC) $(CPPFLAGS )-o $@ $<
endif

# --------------------------------------------------------------------
