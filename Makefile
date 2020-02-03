
CXX = i686-w64-mingw32-g++
PKG_CONFIG = i686-w64-mingw32-pkg-config
CXXFLAGS = -O2 -g -std=c++14 -Wall
LDFLAGS =

all: bin/sforzando-helper.exe

clean:
	rm -rf bin
	rm -rf build

.PHONY: all clean

build/%.cpp.o: %.cpp
	@install -d $(dir $@)
	$(CXX) -MD -MP $(CXXFLAGS) -c -o $@ $<
build/%.cc.o: %.cc
	@install -d $(dir $@)
	$(CXX) -MD -MP $(CXXFLAGS) -c -o $@ $<

#-------------------------------------------------------------------------------
render_OBJS = \
	build/sources/main.cpp.o \
	build/sources/host_sforzando.cpp.o \
	build/thirdparty/pugixml/pugixml.cpp.o \
	build/thirdparty/fmidi/sources/fmidi/fmidi_util.cc.o \
	build/thirdparty/fmidi/sources/fmidi/file/read_xmi.cc.o \
	build/thirdparty/fmidi/sources/fmidi/file/read_smf.cc.o \
	build/thirdparty/fmidi/sources/fmidi/file/identify.cc.o \
	build/thirdparty/fmidi/sources/fmidi/file/write_smf.cc.o \
	build/thirdparty/fmidi/sources/fmidi/fmidi_internal.cc.o \
	build/thirdparty/fmidi/sources/fmidi/fmidi_player.cc.o \
	build/thirdparty/fmidi/sources/fmidi/fmidi_seq.cc.o \
	build/thirdparty/fmidi/sources/fmidi/u_stdio.cc.o \
	build/thirdparty/fmidi/sources/fmidi/u_memstream.cc.o

bin/sforzando-helper.exe: $(render_OBJS)
	@install -d $(dir $@)
	$(CXX) -o $@ $^ $(LDFLAGS)

bin/sforzando-helper.exe: LDFLAGS += -static
bin/sforzando-helper.exe: CXXFLAGS += -Isources -Iresources
bin/sforzando-helper.exe: CXXFLAGS += -Ithirdparty/vestige
bin/sforzando-helper.exe: CXXFLAGS += -Ithirdparty/pugixml
bin/sforzando-helper.exe: CXXFLAGS += -Ithirdparty/zstr
bin/sforzando-helper.exe: CXXFLAGS += -Ithirdparty/fmidi/sources -DFMIDI_STATIC -DFMIDI_DISABLE_DESCRIBE_API
bin/sforzando-helper.exe: CXXFLAGS += "-Wl,-(" `$(PKG_CONFIG) --cflags --static sndfile zlib` "-Wl,-)"
bin/sforzando-helper.exe: LDFLAGS += "-Wl,-(" `$(PKG_CONFIG) --libs --static sndfile zlib` "-Wl,-)"

-include $(render_OBJS:%.o=%.d)
