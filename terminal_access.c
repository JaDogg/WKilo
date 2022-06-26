#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif

// Some code taken from - https://github.com/microsoft/terminal/issues/8820

static HANDLE hStdin = NULL;
static HANDLE hStdout = NULL;
static int savedConsoleOutputModeIsValid = 0;
static DWORD savedConsoleOutputMode = 0;
static int savedConsoleInputModeIsValid = 0;
static DWORD savedConsoleInputMode = 0;

static void disableRawMode(void) {
    printf("\x1b[0m");
    fflush(stdout);

    if (savedConsoleOutputModeIsValid)
        SetConsoleMode(hStdout, savedConsoleOutputMode);
    if (savedConsoleInputModeIsValid)
        SetConsoleMode(hStdin, savedConsoleInputMode);
}

int enableRawMode(void) {
    // Make sure the console state will be returned to its original state
    // when this program ends
    atexit(disableRawMode);

    // Get handles for stdin and stdout
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (hStdin == INVALID_HANDLE_VALUE || hStdin == NULL)
        return 1;
    hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStdout == INVALID_HANDLE_VALUE || hStdout == NULL)
        return 1;

    // Set console to "raw" mode

    if (!GetConsoleMode(hStdout, &savedConsoleOutputMode))
        return 1;
    savedConsoleOutputModeIsValid = 1;
    DWORD newOutputMode = savedConsoleOutputMode;
    newOutputMode |= ENABLE_PROCESSED_OUTPUT;
    newOutputMode &= ~ENABLE_WRAP_AT_EOL_OUTPUT;
    newOutputMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hStdout, newOutputMode))
        return 1;
    if (!GetConsoleMode(hStdin, &savedConsoleInputMode))
        return 1;
    savedConsoleInputModeIsValid = 1;
    DWORD newInputMode = savedConsoleInputMode;
    newInputMode &= ~ENABLE_ECHO_INPUT;
    newInputMode &= ~ENABLE_LINE_INPUT;
    newInputMode &= ~ENABLE_PROCESSED_INPUT;
    newInputMode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
    if (!SetConsoleMode(hStdin, newInputMode))
        return 1;

    return 0;
}


// https://stackoverflow.com/questions/6812224/getting-terminal-size-in-c-for-windows
int getWindowSize(int *rows, int *cols) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    *cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    *rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    return 0;
}

// Following code uses the Windows api to read and write to console
//  instead the C library functions
//  below stuff works as expected.

int winRead(int ignored, char *c, int toread) {
    unsigned long read = 0;
    ReadConsoleA(hStdin, c, toread, &read, NULL);
    return (int) read;
}

int winWrite(int ignored, char *buf, int length) {
    unsigned long wrote = 0;
    WriteConsoleA(hStdout, buf, length, &wrote, NULL);
    return (int) wrote;
}

//  https://stackoverflow.com/a/47229318/1355145

/* The original code is public domain -- Will Hartung 4/9/09 */
/* Modifications, public domain as well, by Antti Haapala, 11/10/17
   - Switched to getc on 5/23/19 */

// if typedef doesn't exist (msvc, blah)
typedef intptr_t ssize_t;

ssize_t getline(char **lineptr, size_t *n, FILE *stream) {
    size_t pos;
    int c;

    if (lineptr == NULL || stream == NULL || n == NULL) {
        errno = EINVAL;
        return -1;
    }

    c = getc(stream);
    if (c == EOF) {
        return -1;
    }

    if (*lineptr == NULL) {
        *lineptr = malloc(128);
        if (*lineptr == NULL) {
            return -1;
        }
        *n = 128;
    }

    pos = 0;
    while (c != EOF) {
        if (pos + 1 >= *n) {
            size_t new_size = *n + (*n >> 2);
            if (new_size < 128) {
                new_size = 128;
            }
            char *new_ptr = realloc(*lineptr, new_size);
            if (new_ptr == NULL) {
                return -1;
            }
            *n = new_size;
            *lineptr = new_ptr;
        }

        ((unsigned char *) (*lineptr))[pos++] = c;
        if (c == '\n') {
            break;
        }
        c = getc(stream);
    }

    (*lineptr)[pos] = '\0';
    return pos;
}

// ==========

wchar_t *yk__utf8_to_utf16_null_terminated(const char *str) {
    if (!str) return NULL;
    UINT cp = CP_UTF8;
    if (strlen(str) >= 3 && str[0] == (char) 0xef && str[1] == (char) 0xbb &&
        str[2] == (char) 0xbf)
        str += 3;
    size_t pwcl = MultiByteToWideChar(cp, 0, str, -1, NULL, 0);
    wchar_t *pwcs = (wchar_t *) malloc(sizeof(wchar_t) * (pwcl + 1));
    pwcl = MultiByteToWideChar(cp, 0, str, -1, pwcs, pwcl + 1);
    pwcs[pwcl] = '\0';
    return pwcs;
}
int yk__io_writefile(char *fpath, char *data, int len) {
    wchar_t *wpath = yk__utf8_to_utf16_null_terminated(fpath);
    if (wpath == NULL) {
        return -1;
    }
#if defined(_MSC_VER)// MSVC
    FILE *file;
    errno_t openerr = _wfopen_s(&file, wpath, L"wb+");
    if (0 != openerr) {
        if (NULL != file) {
            fclose(file);
        }
        free(wpath);
        return -1;
    }
#else // GCC, MingW, etc
    FILE *file = _wfopen(wpath, L"wb+");
    if (file == NULL) {
        free(wpath);
        return -1;
    }
#endif// msvc check
    size_t written = fwrite(data, sizeof(char), len, file);
    free(wpath);
    fclose(file);
    return (written == len) ? 0 : -2;
}