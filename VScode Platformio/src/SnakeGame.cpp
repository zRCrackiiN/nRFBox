#include "SnakeGame.h"
#include <Arduino.h>

// Pull in your OLED instance from main .ino
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

// Button pins (must match your main .ino)
#define BUTTON_UP_PIN     26
#define BUTTON_DOWN_PIN   33
#define BUTTON_LEFT_PIN   25
#define BUTTON_RIGHT_PIN  27
#define BUTTON_SELECT_PIN 32

// Grid config
static const int cellSize   = 4;
static const int gridWidth  = 128 / cellSize;
static const int gridHeight =  64 / cellSize;

// Snake/game state
static int snakeX[128], snakeY[128], snakeLength;
static int dirX, dirY;
static int foodX, foodY;

// Timing
static unsigned long lastMove     = 0;
static const unsigned long moveInterval = 100;

// Pause & menu navigation
static bool paused = false;
static int  pauseIndex = 0;                          // 0=Resume, 1=Toggle Diff, 2=Exit
static unsigned long lastBtnTime = 0;
static const unsigned long debounceDelay = 200;

// Difficulty
Difficulty gameDifficulty = HARD;

// Place food off the snake and never at (0,0)
static void placeFood() {
  bool conflict;
  do {
    conflict = false;
    foodX = random(0, gridWidth);
    foodY = random(0, gridHeight);
    if (foodX == 0 && foodY == 0) conflict = true;
    for (int i = 0; !conflict && i < snakeLength; i++) {
      if (snakeX[i] == foodX && snakeY[i] == foodY) conflict = true;
    }
  } while (conflict);
}

void setupSnakeGame() {
  snakeLength = 3;
  snakeX[0] = gridWidth/2;
  snakeY[0] = gridHeight/2;
  dirX = 1; dirY = 0;
  placeFood();
  paused = false;
  pauseIndex = 0;
}

// Check self‐collision
static bool checkSelf() {
  for (int i = 1; i < snakeLength; i++) {
    if (snakeX[0] == snakeX[i] && snakeY[0] == snakeY[i]) return true;
  }
  return false;
}

// Draw the pause menu centrally aligned
static void drawPauseMenu() {
  const char* items[3] = {
    "Resume",
    gameDifficulty == EASY ? "Diff: Easy" : "Diff: Hard",
    "Exit to Menu"
  };

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  // Title
  const char* title = "PAUSED";
  int titleW = u8g2.getUTF8Width(title);
  int titleH = u8g2.getMaxCharHeight();
  int nItems = 3;
  int lineH = 14;
  int gap   = 16;
  int groupH = titleH + gap + nItems * lineH;
  int startY = (64 - groupH) / 2;
  int titleY = startY + titleH;
  u8g2.drawStr((128 - titleW) / 2, titleY, title);

  // Menu items
  int itemsStartY = startY + titleH + gap;
  for (int i = 0; i < nItems; i++) {
    int y = itemsStartY + i * lineH;
    int w = u8g2.getUTF8Width(items[i]);
    int xText = (128 - w) / 2;
    if (i == pauseIndex) {
      int boxW = w + 8;
      int boxX = (128 - boxW) / 2;
      u8g2.drawBox(boxX, y - lineH + 2, boxW, lineH);
      u8g2.setDrawColor(0);
      u8g2.drawStr(xText, y, items[i]);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(xText, y, items[i]);
    }
  }
  u8g2.sendBuffer();
}

// Run one frame. Returns true if "Exit to Menu" was selected.
bool loopSnakeGame() {
  unsigned long now = millis();

  // Debounced SELECT press: enter/operate pause menu
  if (digitalRead(BUTTON_SELECT_PIN) == LOW && now - lastBtnTime > debounceDelay) {
    lastBtnTime = now;
    if (!paused) {
      // enter pause
      paused = true;
      pauseIndex = 0;
      return false;
    } else {
      // select the highlighted pause‐menu item
      switch (pauseIndex) {
        case 0: // Resume
          paused = false;
          break;
        case 1: // Toggle difficulty
          gameDifficulty = (gameDifficulty == EASY ? HARD : EASY);
          break;
        case 2: // Exit
          return true;
      }
      return false;
    }
  }

  // If paused, handle UP/DOWN navigation and redraw menu
  if (paused) {
    if (digitalRead(BUTTON_UP_PIN) == LOW && now - lastBtnTime > debounceDelay) {
      pauseIndex = (pauseIndex + 2) % 3;
      lastBtnTime = now;
    }
    if (digitalRead(BUTTON_DOWN_PIN) == LOW && now - lastBtnTime > debounceDelay) {
      pauseIndex = (pauseIndex + 1) % 3;
      lastBtnTime = now;
    }
    drawPauseMenu();
    return false;
  }

  // Normal movement
  if      (digitalRead(BUTTON_UP_PIN)    == LOW && dirY == 0) { dirX = 0; dirY = -1; }
  else if (digitalRead(BUTTON_DOWN_PIN)  == LOW && dirY == 0) { dirX = 0; dirY =  1; }
  else if (digitalRead(BUTTON_LEFT_PIN)  == LOW && dirX == 0) { dirX = -1; dirY = 0; }
  else if (digitalRead(BUTTON_RIGHT_PIN) == LOW && dirX == 0) { dirX =  1; dirY = 0; }

  // Advance game tick
  if (now - lastMove > moveInterval) {
    lastMove = now;
    for (int i = snakeLength - 1; i > 0; i--) {
      snakeX[i] = snakeX[i - 1];
      snakeY[i] = snakeY[i - 1];
    }
    snakeX[0] += dirX;
    snakeY[0] += dirY;

    // wall behavior
    if (gameDifficulty == EASY) {
      if (snakeX[0] < 0)               snakeX[0] = gridWidth - 1;
      else if (snakeX[0] >= gridWidth) snakeX[0] = 0;
      if (snakeY[0] < 0)               snakeY[0] = gridHeight - 1;
      else if (snakeY[0] >= gridHeight)snakeY[0] = 0;
    } else {
      if (snakeX[0] < 0 || snakeX[0] >= gridWidth ||
          snakeY[0] < 0 || snakeY[0] >= gridHeight) {
        setupSnakeGame();
        return false;
      }
    }

    // self‐collision
    if (checkSelf()) {
      setupSnakeGame();
      return false;
    }

    // food collision
    if (snakeX[0] == foodX && snakeY[0] == foodY) {
      if (snakeLength < 128) snakeLength++;
      placeFood();
    }
  }

  // draw snake & food
  u8g2.clearBuffer();
  for (int i = 0; i < snakeLength; i++) {
    u8g2.drawBox(snakeX[i] * cellSize, snakeY[i] * cellSize, cellSize, cellSize);
  }
  u8g2.drawBox(foodX * cellSize, foodY * cellSize, cellSize, cellSize);
  u8g2.sendBuffer();

  return false;
}
