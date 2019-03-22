CC   = gcc
LL   = gcc

OBJS = FileExpander.o etextview.o
EXE  = FileExpander
COPTS = -c -Wall -O2

all: $(OBJS)
	$(LL) $(OBJS) -lsyllable -lstdc++ -o $(EXE)
	rescopy $(EXE) -r ./icons/*.png
	strip --strip-all $(EXE)

install: all
	cp -f $(EXE) /bin/$(EXE)
	cp -f ./FileExpander.rules /etc/FileExpander.rules
	cp -f ./scripts/unpack-fe /usr/bin/unpack-fe

clean:
	rm -f $(OBJS)
	rm -f $(EXE)

%.o: %.cpp
	$(CC) $(COPTS) $< -o $@

FileExpander.o: FileExpander.cpp
etextview.o: etextview.cpp
