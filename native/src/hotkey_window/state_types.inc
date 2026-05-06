typedef struct KboWindowTextBuffer {
    char* data;
    size_t capacity;
    size_t length;
} KboWindowTextBuffer;

typedef struct KboMainWindowSearch {
    DWORD pid;
    HWND hwnd;
} KboMainWindowSearch;
