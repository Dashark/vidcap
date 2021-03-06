AXIS_USABLE_LIBS = UCLIBC GLIBC
include $(AXIS_TOP_DIR)/tools/build/rules/common.mak

PROGS     = vidcap

CFLAGS += -Wall -g -O2 -std=c++11
ifeq ($(AXIS_BUILDTYPE),host)
PKGS    = rapp
LDFLAGS += -lcapturehost -ljpeg -lrapp
else
PKGS    = glib-2.0 axhttp axevent axparameter axstorage
LDFLAGS += -lcapture -lrapp
endif # AXIS_BUILDTYPE == host


CFLAGS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_LIBDIR) pkg-config --cflags $(PKGS))
LDLIBS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_LIBDIR) pkg-config --libs $(PKGS))

OBJS = vidcap.o

all:	$(PROGS)

$(PROGS): $(OBJS)
	$(CC) $(LIBS) $(LDLIBS) $^ $(LDFLAGS) -o $@

clean:
	rm -f $(PROGS) *.o core
	rm -f *.tar

