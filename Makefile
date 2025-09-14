LIBDIR += libezrtsp
LIBSRC += $(foreach d, $(LIBDIR), $(wildcard $d/*.c))
LIBOBJ += $(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(LIBSRC)))

INC_DIR += -I./libezrtsp
INC_DIR += -I./ffmpeg_lib/include/


DEP_LIB += ./ffmpeg_lib/lib/libavdevice.a
DEP_LIB += ./ffmpeg_lib/lib/libavfilter.a
DEP_LIB += ./ffmpeg_lib/lib/libavformat.a
DEP_LIB += ./ffmpeg_lib/lib/libavcodec.a
DEP_LIB += ./ffmpeg_lib/lib/libavutil.a
DEP_LIB += ./ffmpeg_lib/lib/libswscale.a
DEP_LIB += ./ffmpeg_lib/lib/libswresample.a


CFLAGS += -Wall -ffunction-sections -fdata-sections 
CFLAGS += -O3

all: lib target

lib: $(LIBOBJ)
	ar -rcs libezrtsp.a $^ 

target:
	gcc -o rtspserv main.c ./libezrtsp.a $(DEP_LIB) $(INC_DIR) -lpthread -lm
	strip rtspserv

clean:
	rm -rf rtspserv
	rm -rf *.o
	rm -rf *.a
	rm -rf libezrtsp/*.o

