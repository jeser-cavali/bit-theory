//
// Created by jeser on 28/03/2026.
//
#include "input.h";

#ifdef _WIN32
#include <conio.h>
#include <windows.h>

void setupTerminal() {
    SetConsoleOutputCP(CP_UTF8);
}

void restoreTerminal() {
    // Windows automatically cleans up standard console changes on exit
}

PlayerCommand getKeyboardInput() {
    if (_kbhit()) {
        int key = _getch();
        if (key == 13) return PlayerCommand::Exit;
        if (key == 32) return PlayerCommand::PlayPause;

        if (key == 0 || key == 224) {
            switch (_getch()) {
                case 72: return PlayerCommand::VolUp;
                case 80: return PlayerCommand::VolDown;
                case 75: return PlayerCommand::SkipBack;
                case 77: return PlayerCommand::SkipForward;
            }
        }
    }
    return PlayerCommand::None;
}

#else
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>

struct termios orig_termios;

void setupTerminal() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON); // Disable echo and line-buffering
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

void restoreTerminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}

// POSIX non-blocking key check
bool posix_kbhit() {
    struct timeval tv = {0L, 0L};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

PlayerCommand getKeyboardInput() {
    if (posix_kbhit()) {
        unsigned char ch;
        if (read(STDIN_FILENO, &ch, 1) != 1) return PlayerCommand::None;

        if (ch == 10) return PlayerCommand::Exit;
        if (ch == 32) return PlayerCommand::PlayPause;

        if (ch == 27) { // Escape sequence started
            struct timeval tv = {0L, 50000L}; // 50ms timeout for Schrödinger's Escape
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(STDIN_FILENO, &fds);

            if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0) {
                unsigned char seq1;
                if (read(STDIN_FILENO, &seq1, 1) == 1 && seq1 == 91) { // '['
                    if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0) {
                        unsigned char seq2;
                        if (read(STDIN_FILENO, &seq2, 1) == 1) {
                            switch (seq2) {
                                case 65: return PlayerCommand::VolUp;
                                case 66: return PlayerCommand::VolDown;
                                case 67: return PlayerCommand::SkipForward;
                                case 68: return PlayerCommand::SkipBack;
                            }
                        }
                    }
                }
            }
        }
    }
    return PlayerCommand::None;
}
#endif