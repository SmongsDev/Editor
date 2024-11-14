#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#if defined(_WIN32) || defined(_WIN64)
    #include <curses.h>       // Windows에서는 curses
#else
    #include <ncurses.h>       // Linux에서는 ncurses
#endif

#define CTRL_KEY(k) ((k) & 0x1f)

struct text {
    int index;
    int size;
    char *chars;
    struct text *prev;
    struct text *next;
};

struct editorConfig {
    int cx, cy;
    int rowoff;
    int screenRows, screenCols;
    int totalRows;
    struct text *row;
    struct text *currentRow;
    char *filename;
    bool isSave;
    char message[256];
};

struct editorConfig E;

void die(const char *s) {
    endwin();
    perror(s);
    exit(1);
}

void initEditor() {
    E.cx = 0;
    E.cy = 0;
    E.rowoff = 0;
    E.totalRows = 0;
    E.row = NULL;
    E.currentRow = NULL;
    E.filename = NULL;
    E.isSave = false;
    getmaxyx(stdscr, E.screenRows, E.screenCols);
    E.screenRows -= 2;
    scrollok(stdscr, TRUE);
}

void initColors() {
    start_color();
    init_pair(1, COLOR_YELLOW, COLOR_BLACK); // 일반 검색 결과 색상
    init_pair(2, COLOR_RED, COLOR_BLACK);    // 현재 검색 결과 색상

    use_default_colors();
}

void initIndexNumbering() {
    struct text *row = E.row;
    int idx = 0;
    while (row) {
        row->index = idx;
        idx++;
    }
}

void editorScroll() {
    // 커서가 화면 위쪽을 벗어났을 때
    if (E.cy < E.rowoff) {
        E.rowoff = E.cy;
    }
    // 커서가 화면 아래쪽을 벗어났을 때
    if (E.cy >= E.rowoff + E.screenRows) {
        E.rowoff = E.cy - E.screenRows + 1;
    }
}

void editorAppendRow(const char *s, size_t len) {
    struct text *new_row = (struct text *)malloc(sizeof(struct text));
    new_row->size = len;
    new_row->chars = (char *)malloc(len + 1);
    memcpy(new_row->chars, s, len);
    new_row->chars[len] = '\0';
    new_row->index = E.totalRows;
    new_row->prev = NULL;
    new_row->next = NULL;

    if (E.row == NULL) {
        E.row = new_row;
        E.currentRow = new_row;
    } else {
        struct text *last_row = E.row;
        while (last_row->next) last_row = last_row->next;
        last_row->next = new_row;
        new_row->prev = last_row;
    }
    E.totalRows++;
}
ssize_t window_getline(char **lineptr, size_t *n, FILE *stream) {
    if (!lineptr || !n || !stream) return -1;

    char *buf = *lineptr;
    size_t size = *n;
    int c;
    size_t i = 0;

    if (buf == NULL || size == 0) {
        size = 128;
        buf = realloc(buf, size);
        if (!buf) return -1;
        *lineptr = buf;
        *n = size;
    }

    while ((c = fgetc(stream)) != EOF) {
        if (i >= size - 1) {
            size *= 2;
            buf = realloc(buf, size);
            if (!buf) return -1;
            *lineptr = buf;
            *n = size;
        }
        buf[i++] = (char)c;
        if (c == '\n') break;
    }

    if (i == 0) return -1;

    buf[i] = '\0';
    return i;
}

void editorOpen(const char *filename) {
    free(E.filename);
    E.filename = strdup(filename);

   FILE *fp = fopen(filename, "r");
    if (!fp) die("fopen");

    char *line = NULL; // 읽어온 줄을 저장할 문자열 포인터
    size_t linecap = 0; // 버퍼의 크기
    ssize_t linelen; // 문자열의 길이 (문자 수)

    #ifdef _WIN32
        while ((linelen = window_getline(&line, &linecap, fp)) != -1) {
    #else
        while ((linelen = getline(&line, &linecap, fp)) != -1) {
    #endif
        while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
            linelen--;
        editorAppendRow(line, linelen);
    }

    free(line);
    fclose(fp);
    E.isSave = false;
    snprintf(E.message, sizeof(E.message), "Opened file %s", filename);
}

void editorSave() {
    if (E.filename == NULL) {
        char filename[256];
        mvhline(E.screenRows, 0, ' ', E.screenCols);
        mvprintw(E.screenRows, 0, "Save as: ");
        echo();
        getnstr(filename, sizeof(filename) - 1);
        noecho();
        E.filename = strdup(filename);
    }

    FILE *fp = fopen(E.filename, "w");
    if (!fp) die("fopen");

    struct text *row = E.row;
    while (row) {
        fwrite(row->chars, 1, row->size, fp); // 한 줄씩 저장
        fwrite("\n", 1, 1, fp);
        row = row->next;
    }
    fclose(fp);
    E.isSave = false;
    snprintf(E.message, sizeof(E.message), "Saved to %s", E.filename);
}

void editorInsertChar(int c) {
    if (E.currentRow == NULL) {
        editorAppendRow("", 0);
    }
    if (!((c >= 32 && c <= 126) || (c >= 192 && c <= 255)))
        return;

    struct text *row = E.currentRow;

    row->chars = (char *)realloc(row->chars, row->size + 2);
    memmove(&row->chars[E.cx + 1], &row->chars[E.cx], row->size - E.cx + 1);
    row->chars[E.cx] = c;
    row->size++;
    E.cx++;
    E.isSave = true;

    if (E.cx >= E.screenCols) {
        E.cx = 0;
        E.cy++;
        if (E.cy >= E.screenRows) {
            scroll(stdscr);
            E.cy = E.screenRows - 1;
        }
    }
    editorScroll();
}

void editorInsertNewline() {
    if (E.currentRow == NULL) {
        editorAppendRow("", 0);
        return;
    }

    struct text *row = E.currentRow;

    // 현재 커서 위치 이후의 내용을 새 줄로 이동
    int new_line_length = row->size - E.cx;
    char *new_line_content = (char *)malloc(new_line_length + 1);
    if (!new_line_content) return; // 메모리 할당 실패 시 함수 종료

    // 새로운 줄에 현재 커서 이후의 내용 복사
    memcpy(new_line_content, &row->chars[E.cx], new_line_length);
    new_line_content[new_line_length] = '\0';  // 널 문자로 종료

    row->chars[E.cx] = '\0';  // 현재 줄의 남은 부분을 잘라냄
    row->size = E.cx;         // 현재 줄의 길이를 커서 위치로 업데이트

    // 새로운 줄 생성 및 초기화
    struct text *new_row = (struct text *)malloc(sizeof(struct text));
    new_row->size = new_line_length;
    new_row->chars = new_line_content;
    new_row->next = row->next;
    new_row->prev = row;
    new_row->size = E.totalRows + 1;

    if (row->next) {
        row->next->prev = new_row;
    }
    row->next = new_row;

    // 새 줄로 커서 이동
    E.currentRow = new_row;
    E.totalRows++;
    E.cx = 0;
    E.cy++;
    E.isSave = true;

    editorScroll();
}

void editorDelChar() {
    if (E.currentRow == NULL) return;

    struct text *row = E.currentRow;

    if (E.cx == 0) {
        if (row->prev) {
            struct text *prev_row = row->prev;

            E.cy--;
            E.cx = prev_row->size;

            // 이전 줄 끝에 현재 줄 내용을 추가
            prev_row->chars = realloc(prev_row->chars, prev_row->size + row->size + 1);
            memcpy(&prev_row->chars[prev_row->size], row->chars, row->size);
            prev_row->size += row->size;
            prev_row->chars[prev_row->size] = '\0';

            // 현재 줄을 삭제
            if (row->next) {
                row->next->prev = prev_row;
            }
            prev_row->next = row->next;

            free(row->chars);
            free(row);

            E.currentRow = prev_row;
            E.totalRows--;
            E.isSave = true;
        }
    } else {
        // E.cx가 0이 아닐 때, 일반적인 문자 삭제
        memmove(&row->chars[E.cx - 1], &row->chars[E.cx], row->size - E.cx);
        row->size--;
        E.cx--;
        E.isSave = true;
    }
}

void editorMoveCursor(int key) {
    switch (key) {
        case KEY_LEFT:
            if (E.cx > 0) {
                E.cx--;
            } else if (E.currentRow->prev) {
                E.currentRow = E.currentRow->prev;
                E.cy--;
                E.cx = E.currentRow->size;
            }
            break;
        case KEY_RIGHT:
            if (E.cx < E.currentRow->size) {
                E.cx++;
            } else if (E.currentRow->next) {
                E.currentRow = E.currentRow->next;
                E.cy++;
                E.cx = 0;
            }
            break;
        case KEY_UP:
            if (E.currentRow->prev) {
                E.currentRow = E.currentRow->prev;
                E.cy--;
                if (E.cx > E.currentRow->size) {
                    E.cx = E.currentRow->size;
                }
            }
            break;
        case KEY_DOWN:
            if (E.currentRow->next) {
                E.currentRow = E.currentRow->next;
                E.cy++;
                if (E.cx > E.currentRow->size) {
                    E.cx = E.currentRow->size;
                }
            }
            break;
        case KEY_HOME:
            E.cx = 0;
            break;
        case KEY_END:
            E.cx = E.currentRow->size;
            break;
        case KEY_PPAGE:  // Page Up
            for (int i = 0; i < E.screenRows; i++) {
                if (E.currentRow->prev) {
                    E.currentRow = E.currentRow->prev;
                    E.cy--;
                }
            }
            if (E.cy < 0) E.cy = 0;
            if (E.cx > E.currentRow->size) E.cx = E.currentRow->size;
            break;
        case KEY_NPAGE:  // Page Down
            for (int i = 0; i < E.screenRows; i++) {
                if (E.currentRow->next) {
                    E.currentRow = E.currentRow->next;
                    E.cy++;
                }
            }
            if (E.cy >= E.totalRows) E.cy = E.totalRows - 1;
            if (E.cx > E.currentRow->size) E.cx = E.currentRow->size;
            break;
    }

    editorScroll();
}

void editorRows() {
    struct text *current = E.row;

    for (int y = 0; y < E.screenRows; y++) {
        int fileRow = y + E.rowoff;
        if (fileRow >= E.totalRows) {
            // 텍스트가 없을 때만 중앙에 메시지 출력
            if (E.totalRows == 0 && y == E.screenRows / 2) {
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome), "Visual Text editor -- version 0.0.1");
                if (welcomelen > E.screenCols) welcomelen = E.screenCols;

                int padding = (E.screenCols - welcomelen) / 2;
                if (padding) {
                    mvaddch(y, 0, '~');
                }
                mvaddnstr(y, padding, welcome, welcomelen);
            } else {
                mvaddch(y, 0, '~');
            }
        } else {
            current = E.row;
            for (int i = 0; i < fileRow; i++) {
                if (current->next) {
                    current = current->next;
                } else {
                    break;
                }
            }

            int len = current ? current->size : 0;
            if (len > E.screenCols) {
                len = E.screenCols;
            }
            if (current && current->chars) {
                mvaddnstr(y, 0, current->chars, len);
            } else {
                mvaddch(y, 0, '~');
            }
        }
    }
}

void editorStatusBar() {
    attron(A_REVERSE);
    char *ext = strrchr(E.filename ? E.filename : "[No Name]", '.');

    char leftStatus[40];
    int leftLen = snprintf(leftStatus, sizeof(leftStatus), " %s - %d lines", E.filename ? E.filename : "[No Name]", E.totalRows);

    char rightStatus[40];
    int rightLen = snprintf(rightStatus, sizeof(rightStatus), "%s | %d/%d", ext ? ++ext : "no ft", E.cy + 1, E.totalRows);

    mvhline(E.screenRows, 0, ' ', E.screenCols);  // 상태 바 초기화
    mvprintw(E.screenRows, 0, "%s", leftStatus);  // 왼쪽 상태 바 출력
    mvprintw(E.screenRows, E.screenCols - rightLen, "%s", rightStatus);

//    mvhline(y, 0, ' ', E.screenCols);
//    char status[80];
//    char *ext = strrchr(E.filename ? E.filename : "[No Name]", '.');
//    snprintf(status, sizeof(status), " %s - %d lines", E.filename ? E.filename : "[No Name]", E.totalRows);
//    mvaddstr(y, 0, status);
    attroff(A_REVERSE);
}

void editorMessageBar() {
    int y = E.screenRows + 1;
    mvhline(y, 0, ' ', E.screenCols);
    char message[80];
    snprintf(message, sizeof(message), "HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");
    mvaddstr(y, 0, message);
}

int saved_cx, saved_cy, saved_rowoff;
struct text *saved_currentRow;
bool search_mode = false;
char query[256];
int current_match_y = -1;           // 검색 결과의 행 위치
int current_match_x = -1;           // 색 결과의 열 위치

void editorFindCallback(char *query) {
    struct text *row = E.row;
    int found = 0;

    for (int y = 0; row && y < E.totalRows; y++, row = row->next) {
        char *match = strstr(row->chars, query);
        if (match) {
            found = 1;
            E.cy = row->index;
            E.cx = match - row->chars;
            E.currentRow = row;
            E.rowoff = E.cy;
            break;
        }
    }
    if (!found) {
        snprintf(E.message, sizeof(E.message), "No match found for '%s'", query);
    }
}

void editorFind() {
    mvhline(E.screenRows + 1, 0, ' ', E.screenCols);
    mvprintw(E.screenRows + 1, 0, "Search: ");
    echo();
    getnstr(query, sizeof(query) - 1);
    noecho();

    // 검색 전 커서와 화면 위치를 저장
    saved_cx = E.cx;
    saved_cy = E.cy;
    saved_rowoff = E.rowoff;
    saved_currentRow = E.currentRow;
    search_mode = true;

    editorFindCallback(query);
}

void editorHighlightMatch(char *query) {
    struct text *row = E.row;
    for (int y = 0; row && y < E.totalRows; y++, row = row->next) {
        char *match = strstr(row->chars, query);
        while (match) {
            int match_pos = match - row->chars;
            if (y == current_match_y && match_pos == current_match_x) {
                attron(COLOR_PAIR(2));
                mvaddnstr(y - E.rowoff, match_pos, query, strlen(query));
                attroff(COLOR_PAIR(2));
            } else {
                attron(COLOR_PAIR(1));
                mvaddnstr(y - E.rowoff, match_pos, query, strlen(query));
                attroff(COLOR_PAIR(1));
            }
            match = strstr(match + 1, query);
        }
    }
}

void editorSearchNext(char *query, int direction) {
    struct text *row = direction > 0 ? E.currentRow->next : E.currentRow->prev;
    int found = 0;

    while (row) {
        char *match = strstr(row->chars, query);
        if (match) {
            E.cy = row->index;
            E.cx = match - row->chars;
            E.currentRow = row;
            E.rowoff = E.cy;
            current_match_y = E.cy;
            current_match_x = E.cx;
            found = 1;
            break;
        }
        row = direction > 0 ? row->next : row->prev;
    }
    if (!found) {
        snprintf(E.message, sizeof(E.message), "No more matches found.");
    }
}

void updateWindowSize() {
    getmaxyx(stdscr, E.screenRows, E.screenCols);
    E.screenRows -= 2;
}

void editorRefreshScreen() {
    clear();
    editorRows();
    editorStatusBar();
    editorMessageBar();
    move(E.cy - E.rowoff, E.cx < E.screenCols ? E.cx : E.screenCols - 1);
    refresh();
}

void editorSearchMode(char *query) {
    int key;

    while (search_mode) {
        editorRefreshScreen();
        editorHighlightMatch(query);

        key = getch();
        switch (key) {
            case KEY_RIGHT:
                editorSearchNext(query, 1);  // 다음 항목
            break;
            case KEY_LEFT:
                editorSearchNext(query, -1);  // 이전 항목
            break;
            case '\n':
            case 27:  // Esc로 검색 취소 및 위치 복원
                E.cx = saved_cx;
                E.cy = saved_cy;
                E.rowoff = saved_rowoff;
                E.currentRow = saved_currentRow;
                search_mode = false;
                editorRefreshScreen();
                break;
            case KEY_RESIZE:  // 화면 크기 변경
                updateWindowSize();
            editorScroll();
            break;
        }
    }
}

int main(int argc, char *argv[]) {
    initscr();
    raw();
    keypad(stdscr, TRUE);
    scrollok(stdscr, TRUE);
//    noecho();

    initEditor();
    initColors();

    if (argc >= 2) {
        editorOpen(argv[1]);
    }

    while (1) {
        editorRefreshScreen();
        int c = getch();
        switch (c) {
            case CTRL_KEY('q'):
                if (E.isSave) {
                    mvhline(E.screenRows, 0, ' ', E.screenCols);
                    mvprintw(E.screenRows, 0, "Unsaved changes! Press Ctrl-Q again to quit.");
                    refresh();
                    int confirm = getch();
                    if (confirm != CTRL_KEY('q')) break;
                }
                endwin();
                exit(0);
                break;
            case CTRL_KEY('s'):
                editorSave();
                break;
            case CTRL_KEY('f'):
                editorFind();
                break;
            case KEY_UP:
            case KEY_DOWN:
            case KEY_LEFT:
            case KEY_RIGHT:
            case KEY_HOME:
            case KEY_END:
            case KEY_PPAGE:
            case KEY_NPAGE:
                editorMoveCursor(c);
                break;
            case '\b':
            case KEY_BACKSPACE:
                editorDelChar();
                break;
            case '\r':
            case '\n':
                editorInsertNewline();
                break;
            case KEY_RESIZE:
                updateWindowSize();
                editorScroll();
                break;
            default:
                editorInsertChar(c);
                break;
        }
        if (search_mode) {
            editorSearchMode(query);
        }
    }
    return 0;
}