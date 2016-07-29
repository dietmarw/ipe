# --------------------------------------------------------------------
# Makefile for icons (only for OSX)
# --------------------------------------------------------------------

IPESRCDIR = ../..
include ../../common.mak

all: 

ifdef IPEAPP

ICONDIR = $(RESOURCEDIR)/icons

icons = copy cut delete fit_objects fit_page fit_width		\
	grid_visible ipe keyboard mode_arc1 mode_arc2 mode_arc3	\
	mode_circle1 mode_circle2 mode_circle3 mode_ink		\
	mode_label mode_lines mode_marks mode_math mode_pan	\
	mode_paragraph mode_parallelogram mode_polygons         \
	mode_rectangles1 mode_rectangles2 mode_rectangles3      \
	mode_rotate mode_shear                                  \
	mode_select mode_shredder mode_splinegons mode_splines	\
	mode_stretch mode_translate paste pen redo shift_key	\
	snapangle snapauto snapbd snapctl snapgrid snapint	\
	snapvtx stop undo zoom_in zoom_out


app:
	$(INSTALL_DIR) $(ICONDIR)
	for f in $(icons) ; do \
	  f1=$$f.png; f2=$$f@2x.png; \
	  f3=$${f}_32x32.png; f4=$${f}_32x32@2x.png; \
	  if [ ! -e $$f2 ]; then f2=""; fi; \
	  if [ ! -e $$f3 ]; then f3=""; fi; \
	  if [ ! -e $$f4 ]; then f4=""; fi; \
	  ft=$$f.tiff; \
	  tiffutil -catnosizecheck $$f1 $$f2 $$f3 $$f4 -out $(ICONDIR)/$$ft; \
	done

endif

# --------------------------------------------------------------------
