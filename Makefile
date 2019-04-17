AXIS_USABLE_LIBS = UCLIBC GLIBC
include $(AXIS_TOP_DIR)/tools/build/rules/common.mak

PROGS     = vidcap

CFLAGS += -Wall -g -std=c++11
#CXXFLAGS += -Wall -g -std=c++11
ifeq ($(AXIS_BUILDTYPE),host)
PKGS    = rapp
CFLAGS += -DYUV_SHOW
LDFLAGS += -lcapturehost -ljpeg -lrapp -lSDL
else
PKGS    = glib-2.0 axhttp axevent axparameter axstorage
LDFLAGS += -lcapture -lrapp
endif # AXIS_BUILDTYPE == host


CFLAGS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_LIBDIR) pkg-config --cflags $(PKGS))
LDLIBS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_LIBDIR) pkg-config --libs $(PKGS))
CFLAGS += -I./include
CXXFLAGS += $(CFLAGS)
SLDLIBS += ./lib/*.a
OBJS = vidcap.o rapp.o yamlServices.o otsu.o

all:	$(PROGS)

$(PROGS): $(OBJS)
	$(CXX) $(CFLAGS) $(LIBS) $(LDLIBS) $^ $(LDFLAGS) $(SLDLIBS) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CFLAGS) -c $<

clean:
	rm -f $(PROGS) *.o core
	rm -f *.tar

