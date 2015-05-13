CFLAGS		?= -std=gnu99 -Os -Wall
CXXFLAGS	?= -std=c++11 -Os -Wall

ifeq ($(OS),Windows_NT)
	TARGET_OS := WINDOWS
	DIST_SUFFIX := windows
	ARCHIVE_CMD := 7z a
	ARCHIVE_EXTENSION := zip
	TARGET := make_spiffs.exe
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Linux)
		TARGET_OS := LINUX
		UNAME_P := $(shell uname -p)
		ifeq ($(UNAME_P),x86_64)
			DIST_SUFFIX := linux64
		endif
		ifneq ($(filter %86,$(UNAME_P)),)
			DIST_SUFFIX := linux32
		endif
	endif
	ifeq ($(UNAME_S),Darwin)
		TARGET_OS := OSX
		DIST_SUFFIX := osx
	endif
	ARCHIVE_CMD := tar czf
	ARCHIVE_EXTENSION := tar.gz
	TARGET := make_spiffs
endif

VERSION ?= $(shell git describe --always)

OBJ		:= main.o \
		   spiffs/spiffs_cache.o \
		   spiffs/spiffs_check.o \
		   spiffs/spiffs_gc.o \
		   spiffs/spiffs_hydrogen.o \
		   spiffs/spiffs_nucleus.o \

INCLUDES := -Itclap -Ispiffs -I.

CFLAGS   += $(TARGET_CFLAGS)
CXXFLAGS += $(TARGET_CXXFLAGS)

CPPFLAGS += $(INCLUDES) -D$(TARGET_OS) -DVERSION=\"$(VERSION)\" 

DIST_NAME := make_spiffs-$(VERSION)-$(DIST_SUFFIX)
DIST_DIR := $(DIST_NAME)
DIST_ARCHIVE := $(DIST_NAME).$(ARCHIVE_EXTENSION)

.PHONY: all clean dist

all: $(TARGET)

dist: checkdirs $(TARGET) $(DIST_DIR)
	cp $(TARGET) $(DIST_DIR)/
	$(ARCHIVE_CMD) $(DIST_ARCHIVE) $(DIST_DIR)

$(TARGET): $(OBJ) 
	g++ $^ -o $@
	strip $(TARGET)


$(DIST_DIR):
	@mkdir -p $@

clean:
	@rm *.o
	@rm spiffs/*.o
	@rm -f $(TARGET)
