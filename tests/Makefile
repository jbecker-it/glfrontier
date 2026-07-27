# Host-side tests for the parts of the Android port that can be checked
# without a device.
#
# These build against the stub SDL in stub/ rather than real SDL2, so they
# run anywhere and need no device, window or GPU. They cover the logic
# that is easy to get quietly wrong and hard to debug on a phone: the
# touch gesture state machine, the foldable layout geometry, and the ini
# parser that feeds the on-screen control set.
#
#     make -C tests && make -C tests run

CC ?= gcc
CFLAGS := -Wall -Wextra -Wno-unused-parameter -g -I stub -I ../src -I ..
LDLIBS := -lm

TESTS := test_touch test_ini

all: $(TESTS)

test_touch: test_touch.c ../src/touch.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_ini: test_ini.c ../src/ini.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

run: all
	./test_touch
	./test_ini ../joystick.ini > /dev/null && echo "ini parser: parsed ../joystick.ini"

clean:
	rm -f $(TESTS)

.PHONY: all run clean
