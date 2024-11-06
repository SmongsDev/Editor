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
    int screenrows, screencols;
    int numrows;
    struct text *row;
    struct text *current_row;
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
    E.numrows = 0;
    E.row = NULL;
    E.current_row = NULL;
    E.filename = NULL;
    E.isSave = false;
    getmaxyx(stdscr, E.screenrows, E.screencols);
    E.screenrows -= 2; // 상태 바와 메시지 바 공간 확보
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
        E.current_row = new_row;
    } else {
        struct text *last_row = E.row;
        while (last_row->next) last_row = last_row->next;
        last_row->next = new_row;
        new_row->prev = last_row;
    }
    E.numrows++;
}

void editorOpen(const char *filename) {
    free(E.filename);
    E.filename = strdup(filename);

    FILE *fp = fopen(filename, "r");
    if (!fp) die("fopen");

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) != -1) {
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
        mvprintw(E.screenrows, 0, "Save as: ");
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
void editorScroll() {
    // 커서가 화면 위쪽을 벗어났을 때
    if (E.cy < E.rowoff) {
        E.rowoff = E.cy;
    }
    // 커서가 화면 아래쪽을 벗어났을 때
    if (E.cy >= E.rowoff + E.screenrows) {
        E.rowoff = E.cy - E.screenrows + 1;
    }
}
void editorInsertChar(int c) {
    if (E.current_row == NULL) editorAppendRow("", 0);
    struct text *row = E.current_row;

    row->chars = (char *)realloc(row->chars, row->size + 2);
    memmove(&row->chars[E.cx + 1], &row->chars[E.cx], row->size - E.cx + 1);
    row->chars[E.cx] = c;
    row->size++;
    E.cx++;
    E.isSave = true;

    editorScroll();  // 스크롤 조정
}

void editorInsertNewline() {
    if (E.current_row == NULL) {
        editorAppendRow("", 0);
        return;
    }

    struct text *row = E.current_row;
    char *new_line_content = strdup(&row->chars[E.cx]);
    row->chars[E.cx] = '\0';
    row->size = E.cx;

    struct text *new_row = (struct text *)malloc(sizeof(struct text));
    new_row->size = strlen(new_line_content);
    new_row->chars = new_line_content;
    new_row->next = row->next;
    new_row->prev = row;

    if (row->next) row->next->prev = new_row;
    row->next = new_row;

    E.current_row = new_row;
    E.numrows++;
    E.cx = 0;
    E.cy++;
    E.isSave = true;

    editorScroll();
}


void editorMoveCursor(int key) {
//    text *row = (E.cy >= E.numrows) ? NULL : E.row; // row 변수를 text로 변경

    switch (key) {
        case KEY_LEFT:
            if (E.cx > 0) {
                E.cx--;
            } else if (E.current_row->prev) {
                E.current_row = E.current_row->prev;
                E.cy--;
                E.cx = E.current_row->size;
            }
            break;
        case KEY_RIGHT:
            if (E.cx < E.current_row->size) {
                E.cx++;
            } else if (E.current_row->next) {
                E.current_row = E.current_row->next;
                E.cy++;
                E.cx = 0;
            }
            break;
        case KEY_UP:
            if (E.current_row->prev) {
                E.current_row = E.current_row->prev;
                E.cy--;
                if (E.cx > E.current_row->size) E.cx = E.current_row->size;
            }
            break;
        case KEY_DOWN:
            if (E.current_row->next) {
                E.current_row = E.current_row->next;
                E.cy++;
                if (E.cx > E.current_row->size) E.cx = E.current_row->size;
            }
            break;
        case KEY_HOME:
            E.cx = 0;
            break;
        case KEY_END:
            E.cx = E.current_row->size;
            break;
        case KEY_PPAGE:  // Page Up
            for (int i = 0; i < E.screenrows; i++) {
                if (E.current_row->prev) {
                    E.current_row = E.current_row->prev;
                    E.cy--;
                }
            }
            if (E.cy < 0) E.cy = 0;
            if (E.cx > E.current_row->size) E.cx = E.current_row->size;
            break;
        case KEY_NPAGE:  // Page Down
            for (int i = 0; i < E.screenrows; i++) {
                if (E.current_row->next) {
                    E.current_row = E.current_row->next;
                    E.cy++;
                }
            }
            if (E.cy >= E.numrows) E.cy = E.numrows - 1;
            if (E.cx > E.current_row->size) E.cx = E.current_row->size;
            break;
    }

    editorScroll();  // 스크롤 조정
}

void editorDelChar() {
    if (E.current_row == NULL || E.cx == 0) return;
    struct text *row = E.current_row;
    memmove(&row->chars[E.cx - 1], &row->chars[E.cx], row->size - E.cx);
    row->size--;
    E.cx--;
    E.isSave = true;
}

void editorRows() {
    struct text *row = E.row;
    int y = 0;
    while (row && y < E.screenrows) {
        int len = row->size;
        if (len > E.screencols) len = E.screencols;
        mvaddnstr(y, 0, row->chars, len);
        row = row->next;
        y++;
    }
}

void editorStatusBar() {
    int y = E.screenrows + 1;
    attron(A_REVERSE);
    mvhline(y, 0, ' ', E.screencols);
    char status[80];
    snprintf(status, sizeof(status), " %s - %d lines", E.filename ? E.filename : "[No Name]", E.numrows);
    mvaddstr(y, 0, status);
    attroff(A_REVERSE);
}

void editorMessageBar() {
    int y = E.screenrows + 2;
    mvhline(y, 0, ' ', E.screencols);
    mvaddstr(y, 0, E.message);
}

void updateWindowSize() {
    // 화면 크기를 다시 가져와서 screenrows와 screencols를 업데이트
    getmaxyx(stdscr, E.screenrows, E.screencols);
    E.screenrows -= 2;  // 상태 바와 메시지 바를 위한 두 줄 확보
}

void editorRefreshScreen() {
    clear();
    editorRows();
    editorStatusBar();
    editorMessageBar();
    move(E.cy - E.rowoff, E.cx);
    refresh();
}
int main(int argc, char *argv[]) {
    initscr();
    raw();
    keypad(stdscr, TRUE);
    noecho();
    initEditor();

    if (argc >= 2) {
        editorOpen(argv[1]);
    }

    while (1) {
        editorRefreshScreen();
        int c = getch();
        switch (c) {
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
            case CTRL_KEY('q'):
                if (E.isSave) {
                    mvprintw(E.screenrows, 0, "Unsaved changes! Press Ctrl-Q again to quit.");
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
            default:
                editorInsertChar(c);
                break;
        }
    }
    return 0;
}
