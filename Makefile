LIBDIR += libezrtsp
LIBSRC += $(foreach d, $(LIBDIR), $(wildcard $d/*.c))
LIBOBJ += $(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(LIBSRC)))


CFLAGS += -Wall -ffunction-sections -fdata-sections 
CFLAGS += -O3


all: lib target

lib: $(LIBOBJ)
	ar -rcs libezrtsp.a $^ 

target:
	gcc -o rtspserv main.c ./libezrtsp.a -I./libezrtsp/ -lpthread
	strip rtspserv

clean:
	rm -rf rtspserv
	rm -rf *.o
	rm -rf *.a
	rm -rf libezrtsp/*.o

