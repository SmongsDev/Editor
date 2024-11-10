#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#define CTRL_KEY(k) ((k) & 0x1f)

struct text {
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

void editorOpen(const char *filename) {
    free(E.filename);
    E.filename = strdup(filename);

    FILE *fp = fopen(filename, "r");
    if (!fp) die("fopen");

    char *line = NULL; // 읽어온 줄을 저장할 문자열 포인터
    size_t linecap = 0; // 버퍼의 크기
    ssize_t linelen; // 문자열의 길이 (문자 수)
    while ((linelen = getline(&line, &linecap, fp)) != -1) { // 파일의 끝에 도달시 -1을 반환
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
        // 파일 이름 입력받기
        char filename[256];
        mvprintw(E.screenRows, 0, "Save as: ");
        echo(); // ncurses의 입력 에코 활성화. 입력한 문자가 화면에 표시됨
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
            scroll(stdscr);  // 화면을 자동으로 스크롤
            E.cy = E.screenRows - 1;
        }
    }
    editorScroll();  // 스크롤 조정
}

void editorInsertNewline() {
    if (E.currentRow == NULL) {
        editorAppendRow("", 0);
        return;
    }

    struct text *row = E.currentRow;
    char *new_line_content = strdup(&row->chars[E.cx]);
    row->chars[E.cx] = '\0';
    row->size = E.cx;

   struct text *new_row = (struct text *)malloc(sizeof(struct text));
    new_row->size = strlen(new_line_content);
    new_row->chars = new_line_content;
    new_row->next = row->next;
    new_row->prev = row;

    if (row->next) {
        row->next->prev = new_row;
    }
    row->next = new_row;

    E.currentRow = new_row;
    E.totalRows++;
    E.cx = 0;
    E.cy++;
    E.isSave = true;

    editorScroll();
}

void editorDelChar() {
    if (E.currentRow == NULL || E.cx == 0) return;
    struct text *row = E.currentRow;
    memmove(&row->chars[E.cx - 1], &row->chars[E.cx], row->size - E.cx);
    row->size--;
    E.cx--;
    E.isSave = true;
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

    editorScroll();  // 스크롤 조정
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
    int y = E.screenRows;
    attron(A_REVERSE);
    mvhline(y, 0, ' ', E.screenCols);
    char status[80];
    snprintf(status, sizeof(status), " %s - %d lines", E.filename ? E.filename : "[No Name]", E.totalRows);
    mvaddstr(y, 0, status);
    attroff(A_REVERSE);
}

void editorMessageBar() {
    int y = E.screenRows + 1;
    mvhline(y, 0, ' ', E.screenCols);
    char message[80];
    snprintf(message, sizeof(message), "HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");
    mvaddstr(y, 0, message);
}

void updateWindowSize() {
    // 화면 크기를 다시 가져와서 screenRows와 screenCols를 업데이트
    getmaxyx(stdscr, E.screenRows, E.screenCols);
    E.screenRows -= 2;  // 상태 바와 메시지 바를 위한 두 줄 확보
}

void editorRefreshScreen() {
    clear();
    editorRows();
    editorStatusBar();
    editorMessageBar();
    // move(E.cy - E.rowoff, E.cx);
    move(E.cy - E.rowoff, E.cx < E.screenCols ? E.cx : E.screenCols - 1);
    refresh();
}
int main(int argc, char *argv[]) {
    initscr();
    raw();
    keypad(stdscr, TRUE);
    scrollok(stdscr, TRUE);
    noecho();
    initEditor();

    if (argc >= 2) {
        editorOpen(argv[1]);
    }

    while (1) {
        editorRefreshScreen();
        int c = getch();
        switch (c) {
            case CTRL_KEY('q'):
                if (E.isSave) {
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
            case KEY_BACKSPACE:
                editorDelChar();
                break;
            case '\n':
                editorInsertNewline();
                break;
            case KEY_RESIZE:  // 창 크기가 변경되었을 때
                updateWindowSize();
                editorScroll();  // 화면이 새 크기에 맞춰 스크롤
                break;
            default:
                editorInsertChar(c);
                break;
        }
    }
    return 0;
}
