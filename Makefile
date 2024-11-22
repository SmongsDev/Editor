# 컴파일러 설정
CC = gcc

# 운영 체제 감지 및 설정
ifeq ($(OS),Windows_NT)
    CFLAGS = -I./lib
    LDFLAGS += -L./lib -lpdcurses
    TARGET = viva.exe
    DLL = lib\pdcurses.dll
    COPY = copy
    RM = del /f /q
else
    CFLAGS = $(shell ncursesw5-config --cflags)
    LDFLAGS = $(shell ncursesw5-config --libs)
    TARGET = viva
    RM = rm -f
endif

# 소스 파일
SRCS = main.c

# 기본 규칙
all: $(TARGET) copy_dll

# 빌드 규칙
$(TARGET): $(SRCS)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)


# DLL 복사 규칙 (Windows)
copy_dll:
ifeq ($(OS),Windows_NT)
	$(COPY) $(DLL) .
endif

# 정리 규칙
clean:
	$(RM) $(TARGET) pdcurses.dll

# PHONY 타겟 설정
.PHONY: all clean