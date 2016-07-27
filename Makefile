ifeq ($(shell uname -s),Linux)
CFLAGS=`pkg-config --cflags glfw3`
LDFLAGS=`pkg-config --libs glfw3`
else
CFLAGS=`/usr/local/bin/pkg-config --cflags glfw3`
LDFLAGS=`/usr/local/bin/pkg-config --libs glfw3`
endif

ifeq ($(shell uname -s),Linux)
COMPILERNAME=g++
WARNINGS=
else
COMPILERNAME=clang++
EXCEPTIONS=-Wno-documentation -Wno-reserved-id-macro -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-missing-variable-declarations -Wno-missing-prototypes -Wno-global-constructors -Wno-padded -Wno-exit-time-destructors -Wno-weak-vtables -Wno-double-promotion
WARNINGS=-Weverything $(EXCEPTIONS)
endif

COMPILER=$(COMPILERNAME) --std=c++11 $(WARNINGS)

TARGET=ism

OBJS=client.o imgui_w.o imgui_draw_w.o imgui_demo_w.o

ism: $(OBJS) proto.cpp
	$(COMPILER) -g $(CFLAGS) -o $@ proto.cpp $(OBJS) $(LDFLAGS)

.cpp.o:
	$(COMPILER) -g -c -o $@ $<

client.o: client.h client.cpp vandalism.cpp cascade.cpp tests.cpp fitbezier.cpp

clean:
	rm -f $(OBJS)
	rm -f $(TARGET)
	rm -rf *.dSYM
	rm -f *~
