CFLAGS=`/usr/local/bin/pkg-config --cflags glfw3`
LDFLAGS=`/usr/local/bin/pkg-config --libs glfw3`
EXCEPTIONS=-Wno-documentation -Wno-reserved-id-macro -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-missing-variable-declarations -Wno-missing-prototypes -Wno-global-constructors -Wno-padded -Wno-exit-time-destructors -Wno-weak-vtables
WARNINGS=-Weverything $(EXCEPTIONS)
COMPILER=clang++ --std=c++11 -Werror $(WARNINGS)

BINS=proto client.dylib

all: $(BINS)

proto: proto.cpp client.h
	$(COMPILER) -g $(CFLAGS) $(LDFLAGS) -lobjc -o $@ $<

.cpp.o:
	$(COMPILER) -g -c -o $@ $<

OBJS=client.o imgui_w.o imgui_draw_w.o imgui_demo_w.o

client.o: client.h client.cpp vandalism.cpp cascade.cpp tests.cpp swrender.h swcolor.h

client.dylib: $(OBJS)
	$(COMPILER) -g -Xlinker -dylib -o $@ $(OBJS)

clean:
	rm -f $(OBJS)
	rm -f $(BINS)
	rm -rf proto.DSYM client.dylib.DSYM
	rm -f *~
