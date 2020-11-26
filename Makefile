ifdef GTK4
DBUS_CPPFLAGS = `pkg-config --cflags giomm-2.66` -DGTK4 -g -O0 -std=c++17
DBUS_LDFLAGS = `pkg-config --libs giomm-2.66`
export PKG_CONFIG_PATH := /usr/local/lib/pkgconfig
else
DBUS_CPPFLAGS = `pkg-config --cflags giomm-2.4` -g -O0 -std=c++17
DBUS_LDFLAGS = `pkg-config --libs giomm-2.4`
endif

default: dbustest dbustest2

dbustest.o: dbustest3.cpp
	$(CXX) $(CPPFLAGS) $(DBUS_CPPFLAGS) -c $< -o $@

dbustest: dbustest.o
	$(CXX) $(LDFLAGS) $(DBUS_LDFLAGS) $^ -o $@

dbustest2: dbustest4.o
	$(CC) $(LDFLAGS) `pkg-config --libs gio-2.0` $^ -o $@

dbustest4.o: dbustest4.c
	$(CC) $(CFLAGS) `pkg-config --cflags gio-2.0` -c $< -o $@

clean:
	-rm *.o
	-rm dbustest dbustest2
