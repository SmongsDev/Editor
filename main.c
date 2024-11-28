/**
* Visual Text Editor
* ---------------------
* C 언어로 구현된 텍스트 에디터로, Linux, Windows, Mac 환경에서 작동하도록 설계
* 기본적인 텍스트 편집 작업, 파일 저장, 검색 기능을 구현
*
* 기능:
* - `struct text`를 활용한 이중 연결 리스트로 텍스트를 동적 메모리 방식으로 관리
* - `editorScroll()`을 이용한 커서 이동 및 화면 스크롤 처리
*
* 구조체:
* - `struct text`: 에디터의 한 줄을 표현하는 구조체.
* - `struct editorConfig`: 에디터 상태를 관리하는 구조체.
* - `struct searchResult`: 검색 작업의 현재 상태를 추적하는 구조체.
*
* 작성자: 신성민
* 버전: 1.0
* 작성일: 2024-11-28
**/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#if defined(_WIN32) || defined(_WIN64)
    #include <curses.h>
#else
    #include <ncurses.h>
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

struct searchResult {
    struct text *row;
    int match_pos;
};

struct searchResult S;

bool search_mode = false;
struct text *saved_currentRow;
int saved_cx, saved_cy, saved_rowoff;

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
    init_pair(1, COLOR_YELLOW, COLOR_BLACK);
    init_pair(2, COLOR_RED, COLOR_BLACK);

    use_default_colors();
}

void updateLineIndexes() {
    struct text *row = E.row;
    int idx = 0;
    while (row) {
        row->index = idx++;
        row = row->next;
    }
}

void editorScroll() {
    if (E.cy < E.rowoff) {
        E.rowoff = E.cy;
    }
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

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    #if defined(_WIN32) || defined(_WIN64)
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
        mvhline(E.screenRows + 1, 0, ' ', E.screenCols);
        mvprintw(E.screenRows + 1, 0, "Save as: ");
        echo();
        getnstr(filename, sizeof(filename) - 1);
        noecho();
        E.filename = strdup(filename);
    }

    FILE *fp = fopen(E.filename, "w");
    if (!fp) die("fopen");

    struct text *row = E.row;
    while (row) {
        fwrite(row->chars, 1, row->size, fp);
        fwrite("\n", 1, 1, fp);
        row = row->next;
    }
    fclose(fp);
    E.isSave = false;
    snprintf(E.message, sizeof(E.message), "Saved to %s", E.filename);
}

void editorInsertNewline() {
    if (E.currentRow == NULL) {
        editorAppendRow("", 0);
        return;
    }

    struct text *row = E.currentRow;

    int new_line_length = row->size - E.cx;
    char *new_line_content = (char *)malloc(new_line_length + 1);

    memcpy(new_line_content, &row->chars[E.cx], new_line_length);
    new_line_content[new_line_length] = '\0';

    row->chars[E.cx] = '\0';
    row->size = E.cx;

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

    E.currentRow = new_row;
    E.totalRows++;
    E.cx = 0;
    E.cy++;
    E.isSave = true;

    updateLineIndexes();
    editorScroll();
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
        editorInsertNewline();
    }
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

            prev_row->chars = realloc(prev_row->chars, prev_row->size + row->size + 1);
            memcpy(&prev_row->chars[prev_row->size], row->chars, row->size);
            prev_row->size += row->size;
            prev_row->chars[prev_row->size] = '\0';

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
        memmove(&row->chars[E.cx - 1], &row->chars[E.cx], row->size - E.cx);
        row->size--;
        E.cx--;
        E.isSave = true;
    }
    updateLineIndexes();
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
        case KEY_PPAGE:
            for (int i = 0; i < E.screenRows; i++) {
                if (E.currentRow->prev) {
                    E.currentRow = E.currentRow->prev;
                    E.cy--;
                }
            }
            if (E.cy < 0) E.cy = 0;
            if (E.cx > E.currentRow->size) E.cx = E.currentRow->size;
            break;
        case KEY_NPAGE:
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
    char *ext = strrchr(E.filename ? E.filename : "", '.');

    char leftStatus[40];
    snprintf(leftStatus, sizeof(leftStatus), " %s - %d lines", E.filename ? E.filename : "[No Name]", E.totalRows);

    char rightStatus[40];
    int rightLen = snprintf(rightStatus, sizeof(rightStatus), "%s | %d/%d", ext ? ++ext : "no ft", E.cy + 1, E.totalRows);

    mvhline(E.screenRows, 0, ' ', E.screenCols);
    mvprintw(E.screenRows, 0, "%s", leftStatus);
    mvprintw(E.screenRows, E.screenCols - rightLen, "%s", rightStatus);

    attroff(A_REVERSE);
}

void editorMessageBar() {
    int y = E.screenRows + 1;
    mvhline(y, 0, ' ', E.screenCols);
    char message[80];
    snprintf(message, sizeof(message), "HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");
    mvaddstr(y, 0, message);
}

void editorFind(char *query) {
    saved_cx = E.cx;
    saved_cy = E.cy;
    saved_rowoff = E.rowoff;
    saved_currentRow = E.currentRow;

    struct text *row = E.row;
    S.row = NULL;
    S.match_pos = -1;

    while (row) {
        char *match = strstr(row->chars, query);
        if (match) {
            S.row = row;
            S.match_pos = match - row->chars;

            E.cx = S.match_pos;
            E.cy = row->index;
            E.currentRow = row;
            return;
        }
        row = row->next;
    }

    snprintf(E.message, sizeof(E.message), "No match found for '%s'", query);
}

void editorHighlightMatch(char *query) {
    struct text *row = E.row;
    for (int y = 0; row && y < E.totalRows; y++, row = row->next) {
        char *match = strstr(row->chars, query);
        while (match) {
            int match_pos = match - row->chars;

            if (row == S.row && match_pos == S.match_pos) {
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
    struct text *row = direction > 0 ? S.row->next : S.row->prev;
    int found = 0;

    char *start_pos;
    if (direction > 0) {
        start_pos = S.row->chars + S.match_pos + 1;
    } else {
        start_pos = S.match_pos > 0 ? S.row->chars + S.match_pos - 1 : NULL;
    }

    if (start_pos && direction < 0) {
        char *match = NULL;
        for (char *p = start_pos; p >= S.row->chars; --p) {
            if (strncmp(p, query, strlen(query)) == 0) {
                match = p;
                break;
            }
        }

        if (match) {
            S.match_pos = match - S.row->chars;
            found = 1;
        }
    } else if (start_pos) {
        char *match = strstr(start_pos, query);
        if (match) {
            S.match_pos = match - S.row->chars;
            found = 1;
        }
    }

    if (!found) {
        while (row) {
            char *match = direction > 0 ? strstr(row->chars, query) : NULL;
            if (!match && direction < 0) {
                for (char *p = row->chars + row->size - 1; p >= row->chars; --p) {
                    if (strncmp(p, query, strlen(query)) == 0) {
                        match = p;
                        break;
                    }
                }
            }

            if (match) {
                S.row = row;
                S.match_pos = match - row->chars;
                found = 1;
                break;
            }
            row = direction > 0 ? row->next : row->prev;
        }
    }

    if (found) {
        E.cx = S.match_pos;
        E.cy = S.row->index;
        E.currentRow = S.row;
        editorScroll();
    } else {
        snprintf(E.message, sizeof(E.message), "No more matches found.");
    }
}


void editorRefreshScreen() {
    clear();
    editorRows();
    editorStatusBar();
    editorMessageBar();
    move(E.cy - E.rowoff, E.cx < E.screenCols ? E.cx : E.screenCols - 1);
    refresh();
}

void updateWindowSize() {
    getmaxyx(stdscr, E.screenRows, E.screenCols);
    E.screenRows -= 2;
    editorRefreshScreen();
}

void editorSearchMode(char *query) {
    while (search_mode) {
        editorRefreshScreen();
        editorHighlightMatch(query);

        int c = getch();
        switch (c) {
            case KEY_RIGHT:
                editorSearchNext(query, 1);
                break;
            case KEY_LEFT:
                editorSearchNext(query, -1);
                break;
            case '\n':
            case '\r':
                search_mode = false;
                return;
            case 27:
                E.cx = saved_cx;
                E.cy = saved_cy;
                E.rowoff = saved_rowoff;
                E.currentRow = saved_currentRow;
                search_mode = false;
                return;
            case KEY_RESIZE:
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
            case CTRL_KEY('s'):
                editorSave();
                break;
            case CTRL_KEY('f'):
                search_mode = true;
                char query[256];
                mvhline(E.screenRows + 1, 0, ' ', E.screenCols);
                mvprintw(E.screenRows + 1, 0, "Search: ");
                echo();
                getnstr(query, sizeof(query) - 1);
                noecho();
                editorFind(query);
                editorSearchMode(query);
                break;
            case KEY_UP:
            case KEY_DOWN:
            case KEY_HOME:
            case KEY_END:
            case KEY_NPAGE:
            case KEY_PPAGE:
            case KEY_LEFT:
            case KEY_RIGHT:
                editorMoveCursor(c);
                break;
            case '\b':
            case 127:
            case KEY_BACKSPACE:
                editorDelChar();
                break;
            case '\r':
            case '\n':
                editorInsertNewline();
                break;
            #if defined(_WIN32) || defined(_WIN64)
                case 546:
                    resize_term(0,0);
            #else
                case KEY_RESIZE:
            #endif
                updateWindowSize();
                editorScroll();
                break;
            default:    
                editorInsertChar(c);
                break;
        }
    }
}