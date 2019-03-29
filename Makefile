.PHONY: clean

CFLAGS := $(shell pkg-config --libs --cflags \
	gtk+-2.0 gdk-2.0 x11 xcomposite xfixes)

colorpicker: main.c
	cc -o colorpicker main.c $(CFLAGS)

install:
	@cp -v colorpicker /usr/bin/

uninstall:
	@rm -vf /usr/bin/colorpicker

clean:
	@rm -f colorpicker
