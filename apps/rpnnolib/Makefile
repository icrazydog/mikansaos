TARGET = rpnnolib

CPPFLAGS = -I.
CXXFLAGS = -O2 -Wall -g --target=x86_64-elf -ffreestanding \
            -fno-exceptions -fno-rtti -std=c++17
LDFLAGS = --entry main -z norelro --image-base 0 --static

.PHONY: all
all: $(TARGET)

rpnnolib: rpnnolib.o Makefile
	ld.lld $(LDFLAGS) -o rpnnolib rpnnolib.o

%.o: %.cpp Makefile
	clang++ $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
