
CXX = i686-w64-mingw32-g++
PKG_CONFIG = i686-w64-mingw32-pkg-config
CXXFLAGS = -O2 -g -std=c++14 -Wall
LDFLAGS =

all: bin/sforzando.exe

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

bin/sforzando.exe: $(render_OBJS)
	@install -d $(dir $@)
	$(CXX) -o $@ $^ $(LDFLAGS)

bin/sforzando.exe: LDFLAGS += -static
bin/sforzando.exe: CXXFLAGS += -Isources -Iresources
bin/sforzando.exe: CXXFLAGS += -Ithirdparty/vestige
bin/sforzando.exe: CXXFLAGS += -Ithirdparty/pugixml
bin/sforzando.exe: CXXFLAGS += -Ithirdparty/zstr
bin/sforzando.exe: CXXFLAGS += -Ithirdparty/fmidi/sources -DFMIDI_STATIC -DFMIDI_DISABLE_DESCRIBE_API
bin/sforzando.exe: CXXFLAGS += "-Wl,-(" `$(PKG_CONFIG) --cflags --static sndfile zlib` "-Wl,-)"
bin/sforzando.exe: LDFLAGS += "-Wl,-(" `$(PKG_CONFIG) --libs --static sndfile zlib` "-Wl,-)"

-include $(render_OBJS:%.o=%.d)
