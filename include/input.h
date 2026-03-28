//
// Created by jeser on 28/03/2026.
//

#ifndef STRING_THEORY_INPUT_H
#define STRING_THEORY_INPUT_H

enum class PlayerCommand {
    None, PlayPause, SkipForward, SkipBack, VolUp, VolDown, Exit
};

void setupTerminal();
void restoreTerminal();
PlayerCommand getKeyboardInput();

#endif //STRING_THEORY_INPUT_H