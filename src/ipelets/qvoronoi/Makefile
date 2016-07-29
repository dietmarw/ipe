# --------------------------------------------------------------------
# Makefile for qvoronoi ipelet
# Needs qhull library (www.qhull.org)
# On Ubuntu/Debian:  apt-get install libqhull-dev
# --------------------------------------------------------------------

IPESRCDIR = ../..
OBJDIR = $(BUILDDIR)/obj/ipelets
DEPEND = $(OBJDIR)/qvoronoi.depend
include ../../common.mak

ifdef WIN32
QHULL_CFLAGS = -I$(IPEDEPS)/include/libqhull
QHULL_LIBS = -L$(IPEDEPS)/lib -lqhull
else ifdef MACOS
QHULL_CFLAGS = -I$(IPEDEPS)/include/libqhull
QHULL_LIBS = -L$(IPEDEPS)/lib -lqhull
else
QHULL_CFLAGS = -I/usr/include/qhull
QHULL_LIBS = -lqhull
endif

TARGET = $(call ipelet_target,qvoronoi)
LUASCRIPT = qvoronoi.lua

CPPFLAGS += -I../../include $(QHULL_CFLAGS)
LIBS += -L$(buildlib) $(QHULL_LIBS) -lipe 
CXXFLAGS += $(DLL_CFLAGS)

all: $(TARGET)

sources	= qvoronoi.cpp

$(TARGET): $(objects)
	$(MAKE_IPELETDIR)
	$(CXX) $(LDFLAGS) $(PLUGIN_LDFLAGS) -o $@ $^ $(LIBS)

clean:
	@-rm -f $(objects) $(TARGET) $(DEPEND)

$(DEPEND): Makefile
	$(MAKE_DEPEND)

-include $(DEPEND)

ifndef IPEBUNDLE

install: $(TARGET)
	$(INSTALL_DIR) $(INSTALL_ROOT)$(IPELETDIR)
	$(INSTALL_PROGRAMS) $(TARGET) $(INSTALL_ROOT)$(IPELETDIR)
	$(INSTALL_FILES) $(LUASCRIPT) $(INSTALL_ROOT)$(IPELETDIR)

endif

ifdef IPEAPP

app:
	$(INSTALL_DIR) $(RESOURCEDIR)/ipelets
	$(INSTALL_FILES) qvoronoi.lua $(RESOURCEDIR)/ipelets

endif

# --------------------------------------------------------------------
