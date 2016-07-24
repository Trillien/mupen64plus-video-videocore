CC=gcc
CXX=g++

ifdef DEBUG
CFLAGS?=-O0 -g -DVCDEBUG
CXXFLAGS?=-O0 -g -DVCDEBUG
else
CFLAGS?=-O2
CXXFLAGS?=-O2
endif

CFLAGS+=-Wall -Wno-parentheses
CXXFLAGS+=-Wall -Wno-parentheses -std=c++11
LD=g++
LDFLAGS=
LIBS=

ifeq ($(shell uname -m),armv7l)
LIBS+=-L/opt/vc/lib -lGLESv2 -lEGL
CFLAGS+=-DHAVE_OPENGLES2 -pthread
CXXFLAGS+=-DHAVE_OPENGLES2 -pthread
LDFLAGS+=-Wl,-Bsymbolic
SO=so
else
ifeq ($(shell uname -o),Msys)
LIBS+=-lglew32 -lopengl32
LDFLAGS+=-Wl,-Bsymbolic -pthread -mthreads
SO=dll
else
ifeq ($(shell uname),Darwin)
LIBS+=-framework OpenGL
SO=dylib
endif
endif
endif

SOURCES_CXX = \
	2xSAI.cpp \
	Combiner.cpp \
	CRC.cpp \
	DepthBuffer.cpp \
	F3D.cpp \
	F3DDKR.cpp \
	F3DEX.cpp \
	F3DEX2.cpp \
	F3DPD.cpp \
	F3DWRUS.cpp \
	FrameBuffer.cpp \
	GBI.cpp \
	gDP.cpp \
	glN64.cpp \
	gSP.cpp \
	L3D.cpp \
	L3DEX.cpp \
	L3DEX2.cpp \
	N64.cpp \
	RDP.cpp \
	RSP.cpp \
	S2DEX.cpp \
	S2DEX2.cpp \
	Textures.cpp \
	VCAtlas.cpp \
	VCCombiner.cpp \
	VCConfig.cpp \
	VCDebugger.cpp \
	VCGeometry.cpp \
	VCRenderer.cpp \
	VCShaderCompiler.cpp \
	VCUtils.cpp \
	VI.cpp \

SOURCES_C =	xxhash.c

OBJECTS = $(SOURCES_CXX:%.cpp=%.o) $(SOURCES_C:%.c=%.o)

SHADERS = blit.fs.glsl blit.vs.glsl debug.fs.glsl debug.vs.glsl n64.fs.glsl n64.vs.glsl

all:	mupen64plus-video-videocore.$(SO)

mupen64plus-video-videocore.$(SO): $(OBJECTS)
	$(LD) -shared $(LDFLAGS) -o $@ $^ `sdl2-config --libs` $(LIBS)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) -o $@ $<

.PHONY: clean install

clean:
	rm -rf $(OBJECTS) $(ALL)

install:	mupen64plus-video-videocore.$(SO) videocore.conf $(SHADERS)
	install -d /usr/local/lib/mupen64plus
	install -D $< /usr/local/lib/mupen64plus/
	install -d /etc/xdg/mupen64plus
	install -D videocore.conf /etc/xdg/mupen64plus/
	install -d /usr/local/share/mupen64plus/videocore
	install -D $(SHADERS) /usr/local/share/mupen64plus/videocore/

rebuild: clean $(ALL)

