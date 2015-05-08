SDK_DIR=$(HOME)/sources/bebop/ARSDKBuildUtils
IDIR=./
CC=gcc
CFLAGS=-I$(IDIR) -I$(SDK_DIR)/Targets/Unix/Install/include

LDIR = $(SDK_DIR)/Targets/Unix/Install/lib

LIBS=-L$(SDK_DIR)/Targets/Unix/Install/lib -larsal -larcommands -larnetwork -larnetworkal -lardiscovery -larstream -lcurl -ljson
LIBS_DBG=-L$(SDK_DIR)/Targets/Unix/Install/lib -larsal_dbg -larcommands_dbg -larnetwork_dbg -larnetworkal_dbg -lardiscovery_dbg -larstream_dbg

OBJ=BebopDroneReceiveStream.o matrix.o polly.o

all: polly/polly

%.o: polly/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

polly/polly: polly/$(OBJ)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f polly/*.o
	rm -f polly

