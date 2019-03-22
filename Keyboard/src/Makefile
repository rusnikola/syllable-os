CC   = gcc
LL   = gcc

OBJS = Keyboard.o
SRCS = Keyboard.cpp
EXE  = Keyboard
COPTS = -c -Wall -O2

all: $(OBJS)
	$(LL) $(OBJS) -lsyllable -lstdc++ -o $(EXE)
	rescopy $(EXE) -r *.png
	strip --strip-all $(EXE)

install: all
	cp -f $(EXE) /Applications/Preferences/$(EXE)

clean:
	rm -f $(OBJS)
	rm -f $(EXE)

%.o: %.cpp
	$(CC) $(COPTS) $< -o $@

Keyboard.o: Keyboard.cpp
