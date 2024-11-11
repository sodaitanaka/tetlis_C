#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ncurses.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <locale.h>
#include <unistd.h>

#define FIELD_WIDTH     (10)
#define FIELD_HEIGHT     (20)

#define SHAPE_WIDTH_MAX   (4)
#define SHAPE_HEIGHT_MAX  (4)
#define FPS           (1)
#define INITIAL_INTERVAL      (1.0)
#define SCORE_FILE "tetlis_score.txt"

#define BGM_FILE "tetlis_bgm.mp3"
#define MOVE_SOUND_FILE "tetlis_move.wav"
#define HIT_SOUND_FILE "tetlis_hit.wav"
#define LINE_CLEAR_SOUND_FILE "tetlis_line_clear.wav"
#define ROTATE_SOUND_FILE "tetlis_rotate.wav"

enum {
  SHAPE_I,
  SHAPE_O,
  SHAPE_S,
  SHAPE_Z,
  SHAPE_J,
  SHAPE_L,
  SHAPE_T,
  SHAPE_MAX
};

typedef struct {
  int  width, height;
  int pattern[SHAPE_HEIGHT_MAX][SHAPE_WIDTH_MAX];
}SHAPE;

typedef struct {
  int x, y;
  SHAPE shape;
}MINO;

typedef struct {
  int score;
  char date[20];
} ScoreEntry;

void SaveScore(int score);
void DisplayTopScores();

ScoreEntry rankings[10];
int score = 0;
double interval = INITIAL_INTERVAL;
Mix_Music *bgm = NULL;

SHAPE shapes[SHAPE_MAX] = {
  //SHAPE_I
  {
    4,4,//int  width, height;
    //int pattern[SHAPE_HEIGHT_MAX][SHAPE_WIDTH_MAX];
    {
      {0,0,0,0},
      {1,1,1,1},
      {0,0,0,0},
      {0,0,0,0}
    }
  },
  //SHAPE_O
  {
    2,2,//int  width, height;
    //int pattern[SHAPE_HEIGHT_MAX][SHAPE_WIDTH_MAX];
    {
      {1,1},
      {1,1}
    }
  },
  //SHAPE_S
  {
    3,3,//int  width, height;
    //int pattern[SHAPE_HEIGHT_MAX][SHAPE_WIDTH_MAX];
    {
      {0,1,1},
      {1,1,0},
      {0,0,0}
    }
  },
  //SHAPE_Z
  {
    3,3,//int  width, height;
    //int pattern[SHAPE_HEIGHT_MAX][SHAPE_WIDTH_MAX];
    {
    {1,1,0},
    {0,1,1},
    {0,0,0} 
    }
  },
  //SHAPE_J
  {
    3,3,//int  width, height;
    //int pattern[SHAPE_HEIGHT_MAX][SHAPE_WIDTH_MAX];
    {
    {1,1,1},
    {0,0,1},
    {0,0,0} 
    }
  },
  //SHAPE_L
  {
    3,3,//int  width, height;
    //int pattern[SHAPE_HEIGHT_MAX][SHAPE_WIDTH_MAX];
    {
    {0,0,1},
    {1,1,1},
    {0,0,0} 
    }
  },
  //SHAPE_T
  {
    3,3,//int  width, height;
    //int pattern[SHAPE_HEIGHT_MAX][SHAPE_WIDTH_MAX];
    {
    {0,1,0},
    {1,1,1},
    {0,0,0} 
    }
  }
};

int field[FIELD_HEIGHT][FIELD_WIDTH];
int screen[FIELD_HEIGHT][FIELD_WIDTH];
MINO mino;
MINO nextMino;
time_t startTime;

//効果音用の変数
Mix_Chunk *moveSound = NULL;
Mix_Chunk *hitSound = NULL;
Mix_Chunk *lineClearSound = NULL;
Mix_Chunk *rotateSound = NULL;

// スコアを保持する関数
void SaveScore(int score) {
  FILE *file = fopen(SCORE_FILE, "a");
  if(file) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char date[20];
    strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", t);

    fprintf(file, "%d %s\n", score, date);
    fclose(file);
  }
}

// 上位５位のスコアを表示する関数
void DisplayTopScores() {
  FILE *file = fopen(SCORE_FILE, "r");
  if (!file) return;

  ScoreEntry scores[100];
  int scoreCount = 0;

  while (fscanf(file, "%d %19s", &scores[scoreCount].score, scores[scoreCount].date) == 2) {
    scoreCount++;
  }
  fclose(file);

  for (int i = 0; i < scoreCount - 1; i++) {
    for (int j = i + 1; j < scoreCount; j++) {
      if (scores[i].score < scores[j].score) {
        ScoreEntry temp = scores[i];
        scores[i] = scores[j];
        scores[j] = temp;
      }
    }
  }
  
  for (int i = 0; i < 5 && i < scoreCount; i++) {
        mvprintw(FIELD_HEIGHT + 10 + i, 0, "Ranking%d | Score: %d, Date: %s", i + 1, scores[i].score, scores[i].date);
  }
}

//下方向の衝突のみ判定する関数
bool MinoIntersectBottom(MINO *m) {
    for(int y=0; y < m->shape.height; y++){
      for(int x=0; x < m->shape.width; x++){
        if(m->shape.pattern[y][x]) {  //ブロックがある部分のみ判定
          int newY = m->y+y;  //下方向のみ判定
          int newX = m->x+x;
          if(newY >= FIELD_HEIGHT || field[newY][newX]) {
            return true; //フィールド外または既存ブロックに衝突を判定
          }
        }
      }
    }
    return false;
}

// 任意の方向の衝突を判定する関数
bool MinoIntersectField(MINO *m) {
  for (int y = 0; y < m->shape.height; y++) {
    for (int x = 0; x < m->shape.width; x++) {
      if (m->shape.pattern[y][x]) {
        int newY = m->y + y;
        int newX = m->x + x;
        if (newY < 0 || newY >= FIELD_HEIGHT || newX < 0 || newX >= FIELD_WIDTH || field[newY][newX]) {
          return true;  // 任意方向の衝突
        }
      }
    }
  }
  return false;
}

//フィールド上部でのゲームオーバー確認
bool CheckGameOver() {
  for (int x = 0; x < mino.shape.width; x++) {
    if (mino.shape.pattern[0][x] && field[mino.y][mino.x+x]) {
      return true;
    }
  }
  return false;
}

//次のテトリミノ生成
void GenerateNextMino() {
  nextMino.shape = shapes[rand() % SHAPE_MAX];
  nextMino.x = 0;
  nextMino.y = 0;
}

void DrawScreen() {

  //経過時間の計算
  time_t currentTime = time(NULL);
  int elapsedTime = (int)difftime(currentTime, startTime);
  int hours = elapsedTime / 3600;
  int minutes = (elapsedTime % 3600) / 60;
  int seconds = elapsedTime % 60;

  //経過時間スコア次のテトリミノの表示
  mvprintw(0, 0, "TIME:%02d:%02d:%02d", hours, minutes, seconds);
  mvprintw(1,0, "SCORE:%d", score);
  mvprintw(2, 0, "NEXT:");
  for (int y = 0; y < nextMino.shape.height; y++) {
    for (int x = 0; x < nextMino.shape.width; x++) {
      if (nextMino.shape.pattern[y][x]) {
        mvprintw(4 + y, 6 + x, "@");  // "@": ブロックのシンボル
      } else {
        mvprintw(4 + y, 6 + x, " ");  // 空白
      }
    }
  }

  DisplayTopScores();

  //フィールド描画
  memcpy(screen, field, sizeof field);
  for(int y=0; y<mino.shape.height; y++)
    for(int x=0; x<mino.shape.width; x++)
      if (mino.shape.pattern[y][x])
      screen[mino.y + y][mino.x + x] |= 1;

  //画面描画
  for (int y = 0; y < FIELD_HEIGHT; y++){
    mvprintw(y+8, 1, "#"); //左の境界線
    for (int x = 0; x < FIELD_WIDTH; x++) {
      mvprintw(y+8, x+2, screen[y][x] ? "@" : " ");
    }
    mvprintw(y+8, FIELD_WIDTH+2, "#"); //右の境界線
  }
  for(int x = 0; x < FIELD_WIDTH + 2; x++) {
    mvprintw(FIELD_HEIGHT+8, x+1,  "#"); //下の境界線の境界線
  }
  refresh();
}

void InitMino() {
  mino = nextMino;
  mino.x = (FIELD_WIDTH - mino.shape.width) / 2;
  mino.y = 0;
  GenerateNextMino();
  //新しいミノ生成時にゲームオーバー判定
  if (CheckGameOver()) {
    SaveScore(score);
    endwin();
    Mix_FreeMusic(bgm);
    Mix_FreeChunk(moveSound);
    Mix_FreeChunk(hitSound);
    Mix_FreeChunk(lineClearSound);
    Mix_FreeChunk(rotateSound);
    Mix_CloseAudio();
    SDL_Quit();
    printf("Game Over!\nYour score: %d\n", score);
    exit(0);
  }
}

void Init() {
  memset(field, 0, sizeof field);
  GenerateNextMino();
  InitMino();
  DrawScreen();
}

void FixMinoToField() {
  for (int y = 0; y < mino.shape.height; y++){
    for (int x = 0; x < mino.shape.width; x++){
      if (mino.shape.pattern[y][x]) {
        field[mino.y+y][mino.x+x]=1;
      }
    }
  }
  Mix_PlayChannel(-1, hitSound, 0); //床に衝突した音
}

void CheckAndClearLines() {
  int linesCleared = 0;
  for(int y=0; y < FIELD_HEIGHT; y++) {
    int isFull = 1;
    for(int x=0; x<FIELD_WIDTH; x++) {
      if (field[y][x] == 0) {
        isFull = 0;
        break;
      }
    }
    if (isFull) {
      //行を削除し上の行をすべて 1行下に詰める
      for (int row = y; row > 0; row--) {
        for (int col = 0; col < FIELD_WIDTH; col++) {
          field[row][col] = field[row - 1][col];
        }
      }
      //一番上の行を空にする
      for (int col = 0; col < FIELD_HEIGHT; col++) {
        field[0][col] = 0;
      }
      linesCleared++; //消去した行をカウント
    }
  }
  //得点計算
  switch (linesCleared) {
    case 1: score += 100; break;
    case 2: score += 300; break;
    case 3: score += 500; break;
    case 4: score += 800; break;
  }
  if (linesCleared > 0) {
    Mix_PlayChannel(-1, lineClearSound, 0);
  }
}

void RotateMino() {
  MINO rotatedMino = mino;
  for (int y = 0; y < mino.shape.height; y++){
    for (int x = 0; x < mino.shape.width; x++){
      rotatedMino.shape.pattern[mino.shape.width - 1 - x][y] = mino.shape.pattern[y][x];
    }
  }
  if (!MinoIntersectField(&rotatedMino)){
    mino = rotatedMino;
    Mix_PlayChannel(-1, rotateSound, 0);
  }
}

void UpdateInterval() {
  time_t currentTime = time(NULL);
  int elapsedSeconds = (int)difftime(currentTime, startTime);

  int intervalStage = elapsedSeconds / 60;
  interval = INITIAL_INTERVAL;
  for (int i = 0; i < intervalStage; i++) {
    interval *= 0.8;
  }
}

void LoadRankings() {
  FILE *file = fopen(SCORE_FILE, "r");
  if(!file) return;

  int score;
  char date[20];
  int i = 0;

  while (fscanf(file, "%d %19[^\n]", &score, date) == 2 && i < 10) {
    rankings[i].score = score;
    strncpy(rankings[i].date, date, sizeof(rankings[i].date));
    i++;
  }
  fclose(file);
}

void DisplayRankings() {
    mvprintw(3, FIELD_WIDTH + 5, "RANKING:");
    for (int i = 0; i < 10 && rankings[i].score > 0; i++) {
        mvprintw(4 + i, FIELD_WIDTH + 5, "%d: %d %s", i + 1, rankings[i].score, rankings[i].date);
    }
}

int main(){
  //SDLと音楽の初期化
  if(SDL_Init(SDL_INIT_AUDIO) < 0) {
    fprintf(stderr, "SDL init error: %s\n", SDL_GetError());
    return 1;
  }
  if(Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
    fprintf(stderr, "SDL_mixer init error: %s\n", SDL_GetError());
  }

  //音楽ファイルの読み込み
  Mix_Music *bgm = Mix_LoadMUS(BGM_FILE);
  moveSound = Mix_LoadWAV(MOVE_SOUND_FILE);
  hitSound = Mix_LoadWAV(HIT_SOUND_FILE);
  lineClearSound = Mix_LoadWAV(LINE_CLEAR_SOUND_FILE);
  rotateSound = Mix_LoadWAV(ROTATE_SOUND_FILE);

  if (!bgm || !moveSound || !hitSound || !lineClearSound || !rotateSound) {
    fprintf(stderr, "BGM load error: %s\n", Mix_GetError());
    Mix_CloseAudio();
    SDL_Quit();
    return 1;
  }

  //BGMの音量設定と再生
  Mix_VolumeMusic(32);
  Mix_PlayMusic(bgm, -1);

  LoadRankings();
  DisplayRankings();

  //nursesとゲームの初期化
  srand((unsigned int)time(NULL));
  startTime = time(NULL);
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  nodelay(stdscr, TRUE);

  Init();

  time_t lastMoveTime = time(NULL);
  while (1) {
    int ch = getch();

    MINO lastMino = mino;
    switch (ch) {
        case 'w':
        case KEY_UP:
          break;
        case 's':
        case KEY_DOWN:
          mino.y++;
          score += 1;
          break;
        case 'a':
        case KEY_LEFT:
          mino.x--;
          if(MinoIntersectField(&mino)) mino = lastMino;
          else Mix_PlayChannel(-1, moveSound, 0);
          break;
        case 'd':
        case KEY_RIGHT:
          mino.x++;
          if(MinoIntersectField(&mino)) mino = lastMino;
          else Mix_PlayChannel(-1, moveSound, 0);
          break;
        case ' ':
          RotateMino();
          break;
        case 'q':
          Mix_FreeMusic(bgm);
          Mix_FreeChunk(moveSound);
          Mix_FreeChunk(hitSound);
          Mix_FreeChunk(lineClearSound);
          Mix_FreeChunk(rotateSound);
          Mix_CloseAudio();
          SDL_Quit();
          endwin();
          return 0;
      
      if (ch == 'q') {
        SaveScore(score);
        break;
      }
      }
      UpdateInterval();

      //前回の移動から1秒経過したら自動で下に移動
      time_t currentTime = time(NULL);
      if(currentTime - lastMoveTime >= interval){
        mino.y++;
        score += 1;
        lastMoveTime = currentTime;
      }

      if (MinoIntersectBottom(&mino)){
        mino= lastMino;     //衝突した場合は一つ前に戻す
        FixMinoToField();   //フィールド固定
        CheckAndClearLines();//行が埋まっている場合に消去
        InitMino();         //新しいミノ生成
      }

      clear();
      DrawScreen();
      usleep(30000);
    }
    Mix_FreeMusic(bgm);
    Mix_FreeChunk(moveSound);
    Mix_FreeChunk(hitSound);
    Mix_FreeChunk(lineClearSound);
    Mix_FreeChunk(rotateSound);
    Mix_CloseAudio();
    SDL_Quit();
    endwin();
    return 0;
}