BREW_PREFIX := $(shell brew --prefix)
PKG_CFLAGS := $(shell pkg-config --cflags libgit2 libcjson)
PKG_LFLAGS := $(shell pkg-config --libs libgit2 libcjson)
CC = clang
PREFIX = /usr/local/bin
TARGET = pr-release
CFLAGS = -I${BREW_PREFIX}/include -L${BREW_PREFIX}/lib ${PKG_CFLAGS} ${PKG_LFLAGS} -Ideps -Wall

# all the source files
SRC = $(wildcard src/*.c)
SRC += $(wildcard deps/*/*.c)

OBJS = $(SRC:.c=.o)

.PHONY:
all: $(TARGET)

.PHONY:
fmt: 
	clang-format -i $(wildcard src/*.c)
	

.PHONY:
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(OBJS)

.PHONY:
%.o: %.c
	$(CC) $(DEP_FLAG) $(CFLAGS) $(LDFLAGS) -o $@ -c $<

.PHONY:
clean:
	rm -f $(OBJS)

.PHONY:
install: $(TARGET)
	cp -f $(TARGET) $(PREFIX)

.PHONY:
uninstall: $(PREFIX)/$(TARGET)
	rm -f $(PREFIX)/$(TARGET)