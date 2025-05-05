#ifndef SNAKEGAME_H
#define SNAKEGAME_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "pindefs.h"

// Difficulty levels
enum Difficulty { EASY, HARD };
extern Difficulty gameDifficulty;

// Initialize snake game state (once on entry)
void setupSnakeGame();

// Run one frame.  
// Returns true if the player long‐pressed SELECT (i.e. requests exit to menu).
bool loopSnakeGame();

#endif // SNAKEGAME_H
