# 컴파일러 설정
CC = gcc

# 운영 체제 감지 및 설정
ifeq ($(OS),Windows_NT)
    CFLAGS = -I./PDCurses
    LDFLAGS = -L./PDCurses/wincon -lpdcurses
    TARGET = viva.exe
    DLL = PDCurses/wincon/pdcurses.dll
    RM_DLL = ./pdcurses.dll
    COPY = powershell -Command "Copy-Item"
    RM = powershell -Command "Remove-Item"
else
    CFLAGS = $(shell ncursesw5-config --cflags)
    LDFLAGS = $(shell ncursesw5-config --libs)
    TARGET = viva
    RM = rm -f
endif

# 소스 파일
SRCS = main.c

# 기본 규칙
all: pdcurses $(TARGET)

# 빌드 규칙
$(TARGET): $(SRCS)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)


# pdcurses 복사 규칙 (Windows)
pdcurses:
ifeq ($(OS),Windows_NT)
	mingw32-make -C PDCurses/wincon DLL=Y INFOEX=N
	$(COPY) $(DLL) $(RM_DLL)
endif

# 정리 규칙
clean:
	$(RM) $(TARGET)
ifeq ($(OS),Windows_NT)
	$(RM) $(RM_DLL)
endif

# PHONY 타겟 설정
.PHONY: all clean pdcurses