# 컴파일러 설정
CC = gcc

# 컴파일 옵션
#CFLAGS = -Wall -g `ncursesw5-config --cflags`
#LDFLAGS = `ncursesw5-config --libs`

CFLAGS = -Wall -g $(shell if command -v ncursesw5-config > /dev/null 2>&1; then ncursesw5-config --cflags; else ncurses5-config --cflags; fi)
LDFLAGS = $(shell if command -v ncursesw5-config > /dev/null 2>&1; then ncursesw5-config --libs; else ncurses5-config --libs; fi)


# 실행 파일 이름
TARGET = viva

# 소스 파일
SRCS = main.c

# 오브젝트 파일
OBJS = $(SRCS:.c=.o)

# 기본 규칙: 프로그램 컴파일
$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LDFLAGS)

# 개별 소스 파일 컴파일
%.o: %.c
	$(CC) -c $< $(CFLAGS)

# 정리 규칙: 오브젝트 파일 및 실행 파일 제거
clean:
	rm -f $(OBJS) $(TARGET)

# make 실행 시 기본적으로 $(TARGET)을 실행
.PHONY: clean