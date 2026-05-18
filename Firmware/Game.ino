#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>

#define TFT_CS         7   
#define TFT_DC         6   
#define TFT_RST        5   
#define TFT_SCLK       21  
#define TFT_MOSI       22  

#define ENCODER_CLK    0
#define ENCODER_DT     1
#define ENCODER_SW     2   

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

enum SystemState { STATE_MENU, STATE_SNAKE, STATE_FLAPPY, STATE_TETRIS };
SystemState currentState = STATE_MENU;

const char* menuItems[] = { "1. Retro Snake", "2. Flappy Bird", "3. Tetris Blocks" };
int currentSelection = 0;
const int totalItems = 3;

// Encoder tracking variables
int lastClkState;
unsigned long lastButtonPress = 0;

// Game timing tracking (Non-blocking)
unsigned long lastGameTick = 0;

// 1. Snake Variables
struct Point { int x; int y; };
Point snake[100];
int snakeLength;
Point snakeDir;
Point food;
bool gameOver = false;

// 2. Flappy Bird Variables
float birdY, birdVelocity;
const float gravity = 0.4;
const float jumpImpulse = -5.0;
int pipeX;
int pipeGapY;
const int pipeWidth = 30;
const int pipeGapHeight = 65;
int flappyScore = 0;

// 3. Tetris Variables
#define GRID_WIDTH 10
#define GRID_HEIGHT 20
#define BLOCK_SIZE 10 // Rendering scale for Tetris grid
uint8_t tetrisGrid[GRID_WIDTH][GRID_HEIGHT] = {0};
int currentPieceX, currentPieceY;
int currentPieceType;
int currentRotation;

// Simplified Tetris tetromino shapes [type][rotation][row][col]
const uint8_t tetrominoes[7][4][4] = {
  {{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}}, // I
  {{0,0,0,0},{0,1,1,0},{0,1,1,0},{0,0,0,0}}, // O
  {{0,0,0,0},{0,1,1,1},{0,1,0,0},{0,0,0,0}}, // L
  {{0,0,0,0},{1,1,1,0},{0,0,1,0},{0,0,0,0}}, // J
  {{0,0,0,0},{0,1,1,0},{1,1,0,0},{0,0,0,0}}, // Z
  {{0,0,0,0},{1,1,0,0},{0,1,1,0},{0,0,0,0}}, // S
  {{0,0,0,0},{1,1,1,0},{0,1,0,0},{0,0,0,0}}  // T
};

void setup() {
  Serial.begin(115200);
  
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  
  tft.init(240, 240);
  tft.setRotation(2);
  tft.fillScreen(ST77XX_BLACK);
  
  lastClkState = digitalRead(ENCODER_CLK);
  drawMenu();
}

void loop() {
  handleInputs();
  
  // Game Loop Router
  switch (currentState) {
    case STATE_MENU:
      // Menu is static until input changes selection
      break;
    case STATE_SNAKE:
      runSnake();
      break;
    case STATE_FLAPPY:
      runFlappy();
      break;
    case STATE_TETRIS:
      runTetris();
      break;
  }
}

// HARDWARE INPUT
void handleInputs() {
  // Read Encoder Turn Logic
  int currentClkState = digitalRead(ENCODER_CLK);
  if (currentClkState != lastClkState && currentClkState == LOW) {
    if (digitalRead(ENCODER_DT) != currentClkState) {
      // Turned Clockwise
      if (currentState == STATE_MENU) {
        currentSelection = (currentSelection + 1) % totalItems;
        drawMenu();
      } else {
        handleGameControls(1); // Signal right/down turn to active game
      }
    } else {
      // Turned Counter-Clockwise
      if (currentState == STATE_MENU) {
        currentSelection = (currentSelection - 1 + totalItems) % totalItems;
        drawMenu();
      } else {
        handleGameControls(-1); // Signal left/up turn to active game
      }
    }
  }
  lastClkState = currentClkState;

  // Read Encoder Pushbutton Click with Debouncing
  if (digitalRead(ENCODER_SW) == LOW) {
    if (millis() - lastButtonPress > 250) { // 250ms debounce window
      lastButtonPress = millis();
      
      if (currentState == STATE_MENU) {
        // Launch selected game
        tft.fillScreen(ST77XX_BLACK);
        if (currentSelection == 0) initSnake();
        else if (currentSelection == 1) initFlappy();
        else if (currentSelection == 2) initTetris();
      } else {
        // If inside a game, button acts as action command (Jump/Rotate/Reset)
        handleGameButton();
      }
    }
  }
}

// Route navigation encoders to active arcade layers
void handleGameControls(int direction) {
  if (currentState == STATE_SNAKE) {
    // Relative turning logic
    if (snakeDir.x == 0) {
      snakeDir = {direction * 4, 0};
    } else {
      snakeDir = {0, direction * 4};
    }
  }
  else if (currentState == STATE_TETRIS) {
    // Move block left or right
    int newX = currentPieceX + direction;
    if (newX >= 0 && newX < GRID_WIDTH) { // Simple boundary safety check
      currentPieceX = newX;
    }
  }
}

void handleGameButton() {
  if (gameOver) {
    // Exit back to main launcher menu on game over click
    currentState = STATE_MENU;
    tft.fillScreen(ST77XX_BLACK);
    drawMenu();
    return;
  }

  if (currentState == STATE_FLAPPY) {
    birdVelocity = jumpImpulse; // Upward acceleration push
  }
  else if (currentState == STATE_TETRIS) {
    currentRotation = (currentRotation + 1) % 4; // Shift rotation index matrix
  }
}

// SYSTEM MENU RENDERING
void drawMenu() {
  tft.setCursor(20, 30);
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(3);
  tft.println("ARCADE OS");
  tft.drawFastHLine(10, 60, 220, ST77XX_WHITE);
  
  for (int i = 0; i < totalItems; i++) {
    tft.setCursor(20, 90 + (i * 45));
    if (i == currentSelection) {
      tft.setTextColor(ST77XX_BLACK, ST77XX_GREEN); // Highlight box look
      tft.setTextSize(2);
      tft.printf("> %s <", menuItems[i]);
    } else {
      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      tft.setTextSize(2);
      tft.printf("  %s  ", menuItems[i]);
    }
  }
}

//RETRO SNAKE
void initSnake() {
  currentState = STATE_SNAKE;
  gameOver = false;
  snakeLength = 4;
  snakeDir = {4, 0};
  for(int i=0; i<snakeLength; i++) { snake[i] = {120 - (i*4), 120}; }
  food = {rand() % 50 * 4 + 20, rand() % 50 * 4 + 20};
}

void runSnake() {
  if (gameOver) {
    drawGameOverScreen();
    return;
  }

  if (millis() - lastGameTick > 120) { // Dynamic speed frame clock
    lastGameTick = millis();

    // Clean old tail block trace
    tft.fillRect(snake[snakeLength-1].x, snake[snakeLength-1].y, 4, 4, ST77XX_BLACK);

    // Shift coordinates tracking body chain
    for (int i = snakeLength - 1; i > 0; i--) { snake[i] = snake[i - 1]; }
    
    // Process head positioning delta
    snake[0].x += snakeDir.x;
    snake[0].y += snakeDir.y;

    // Outer wall boundaries containment validation
    if (snake[0].x < 0 || snake[0].x >= 240 || snake[0].y < 0 || snake[0].y >= 240) {
      gameOver = true;
    }

    // Target fruit ingestion execution
    if (abs(snake[0].x - food.x) < 4 && abs(snake[0].y - food.y) < 4) {
      snakeLength++;
      food = {rand() % 50 * 4 + 20, rand() % 50 * 4 + 20};
    }

    // Render operations
    tft.fillRect(food.x, food.y, 4, 4, ST77XX_RED);
    for (int i = 0; i < snakeLength; i++) {
      tft.fillRect(snake[i].x, snake[i].y, 4, 4, (i == 0) ? ST77XX_YELLOW : ST77XX_GREEN);
    }
  }
}

//FLAPPY BIRD
void initFlappy() {
  currentState = STATE_FLAPPY;
  gameOver = false;
  birdY = 120;
  birdVelocity = 0;
  pipeX = 240;
  pipeGapY = 60;
  flappyScore = 0;
}

void runFlappy() {
  if (gameOver) {
    drawGameOverScreen();
    return;
  }

  if (millis() - lastGameTick > 30) {
    lastGameTick = millis();

    // Clean up old graphics traces
    tft.fillRect(40, (int)birdY, 12, 12, ST77XX_BLACK);
    tft.fillRect(pipeX, 0, 6, 240, ST77XX_BLACK); // Clean column strip ahead/behind

    // Physics Processing Engine
    birdVelocity += gravity;
    birdY += birdVelocity;

    pipeX -= 4; // Scroll pillars leftward
    if (pipeX < -pipeWidth) {
      pipeX = 240;
      pipeGapY = rand() % 100 + 40;
      flappyScore++;
    }

    // Boundary check collision calculations
    if (birdY > 230 || birdY < 0) gameOver = true;
    if (pipeX < 52 && pipeX + pipeWidth > 40) {
      if (birdY < pipeGapY || birdY + 12 > pipeGapY + pipeGapHeight) {
        gameOver = true;
      }
    }

    // Visual rendering steps
    tft.fillRect(pipeX, 0, pipeWidth, pipeGapY, ST77XX_GREEN);
    tft.fillRect(pipeX, pipeGapY + pipeGapHeight, pipeWidth, 240 - (pipeGapY + pipeGapHeight), ST77XX_GREEN);
    tft.fillRect(40, (int)birdY, 12, 12, ST77XX_YELLOW);
    
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(10, 10);
    tft.printf("Score: %d", flappyScore);
  }
}

//TETRIS BLOCKS
void initTetris() {
  currentState = STATE_TETRIS;
  gameOver = false;
  memset(tetrisGrid, 0, sizeof(tetrisGrid));
  spawnTetrisPiece();
}

void spawnTetrisPiece() {
  currentPieceX = 3;
  currentPieceY = 0;
  currentPieceType = rand() % 7;
  currentRotation = 0;
}

void runTetris() {
  if (gameOver) {
    drawGameOverScreen();
    return;
  }

  if (millis() - lastGameTick > 500) { // Drop tick every half second
    lastGameTick = millis();

    // Clear old active rendering traces safely
    renderTetrisGrid();

    // Check bottom drop collisions
    currentPieceY++;
    if (checkTetrisCollision()) {
      currentPieceY--; // Rollback position shift
      lockTetrisPiece();
      spawnTetrisPiece();
      if (checkTetrisCollision()) gameOver = true; // Blocked at spawn point
    }
    
    renderTetrisActivePiece();
  }
}

bool checkTetrisCollision() {
  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 4; col++) {
      if (tetrominoes[currentPieceType][currentRotation][row][col]) {
        int targetX = currentPieceX + col;
        int targetY = currentPieceY + row;
        if (targetX < 0 || targetX >= GRID_WIDTH || targetY >= GRID_HEIGHT) return true;
        if (targetY >= 0 && tetrisGrid[targetX][targetY]) return true;
      }
    }
  }
  return false;
}

void lockTetrisPiece() {
  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 4; col++) {
      if (tetrominoes[currentPieceType][currentRotation][row][col]) {
        int targetX = currentPieceX + col;
        int targetY = currentPieceY + row;
        if (targetY >= 0) tetrisGrid[targetX][targetY] = 1;
      }
    }
  }
  // Simplified clear full rows validation code block
  for (int y = GRID_HEIGHT - 1; y >= 0; y--) {
    bool full = true;
    for (int x = 0; x < GRID_WIDTH; x++) { if (!tetrisGrid[x][y]) full = false; }
    if (full) {
      for (int moveY = y; moveY > 0; moveY--) {
        for (int x = 0; x < GRID_WIDTH; x++) tetrisGrid[x][moveY] = tetrisGrid[x][moveY - 1];
      }
      y++; // Re-verify line substitution positioning indices parity
    }
  }
}

void renderTetrisGrid() {
  // Renders baseline static matrix canvas offset windowed coordinates
  for (int x = 0; x < GRID_WIDTH; x++) {
    for (int y = 0; y < GRID_HEIGHT; y++) {
      tft.fillRect(70 + (x * BLOCK_SIZE), 20 + (y * BLOCK_SIZE), BLOCK_SIZE - 1, BLOCK_SIZE - 1, 
                   tetrisGrid[x][y] ? ST77XX_BLUE : ST77XX_BLACK);
    }
  }
}

void renderTetrisActivePiece() {
  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 4; col++) {
      if (tetrominoes[currentPieceType][currentRotation][row][col]) {
        int drawX = 70 + ((currentPieceX + col) * BLOCK_SIZE);
        int drawY = 20 + ((currentPieceY + row) * BLOCK_SIZE);
        if (drawY >= 20) {
          tft.fillRect(drawX, drawY, BLOCK_SIZE - 1, BLOCK_SIZE - 1, ST77XX_ORANGE);
        }
      }
    }
  }
}

//GAME OVER
void drawGameOverScreen() {
  tft.fillRect(30, 70, 180, 100, ST77XX_RED);
  tft.drawRect(30, 70, 180, 100, ST77XX_WHITE);
  
  tft.setCursor(55, 90);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.println("GAME OVER");
  
  tft.setCursor(42, 130);
  tft.setTextSize(1);
  tft.println("Press Encoder to Exit");
}
