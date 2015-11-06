CFLAGS=`/usr/local/bin/pkg-config --cflags glfw3`
LDFLAGS=`/usr/local/bin/pkg-config --libs glfw3`
COMPILER=clang++ --std=c++11

BINS=proto client.dylib

all: $(BINS)

proto: proto.cpp client.h
	$(COMPILER) -g $(CFLAGS) $(LDFLAGS) -lobjc -o $@ $<

.cpp.o:
	$(COMPILER) -g -c -o $@ $<

OBJS=client.o imgui.o imgui_draw.o imgui_demo.o

client.o: client.h client.cpp vandalism.cpp cascade.cpp tests.cpp swrender.h swcolor.h

client.dylib: $(OBJS)
	$(COMPILER) -g -Xlinker -dylib -o $@ $(OBJS)

clean:
	rm -f $(OBJS)
	rm -f $(BINS)
	rm -rf proto.DSYM client.dylib.DSYM
	rm -f *~
