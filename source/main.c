#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define W 256
#define H 192
#define ROWS 8

/* RGB555 helper. */
#define RGB(r,g,b) ((u16)(((r)>>3) | (((g)>>3)<<5) | (((b)>>3)<<10)))

/* Pastel palette: cream, pink, blue, lavender, mint. */
#define CREAM RGB(255,248,238)
#define PINK RGB(255,190,205)
#define BLUE RGB(170,215,245)
#define LAV RGB(210,195,240)
#define MINT RGB(185,235,210)
#define RED RGB(245,125,145)
#define DARK RGB(65,60,78)
#define WHITE RGB(255,255,255)
#define SHADOW RGB(190,180,195)
#define GREEN RGB(110,205,150)

typedef enum { MENU, SPRINT, STREAK, BONUS } Mode;

typedef struct { int n[4]; } Row;

static Row q[ROWS];
static int cur;
static Mode mode = MENU;
static int score, streak, bestStreak, seconds, elapsed;
static int frameCount;
static int flashFrames;
static bool flashGood;

static u16 *topBmp;
static u16 *bottomBmp;

static void rect(u16 *bmp, int x, int y, int w, int h, u16 c) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x+w > W) w = W-x;
    if (y+h > H) h = H-y;
    if (w <= 0 || h <= 0) return;
    for (int yy=y; yy<y+h; yy++)
        for (int xx=x; xx<x+w; xx++)
            bmp[yy*W+xx] = c;
}

static void border(u16 *bmp, int x, int y, int w, int h, u16 c) {
    rect(bmp,x,y,w,3,c);
    rect(bmp,x,y+h-3,w,3,c);
    rect(bmp,x,y,3,h,c);
    rect(bmp,x+w-3,y,3,h,c);
}

static int rnd9(void) { return 1 + rand()%9; }

static void newRow(Row *r) {
    for (int i=0;i<4;i++) r->n[i]=rnd9();
}

static void refill(void) {
    for (int i=0;i<ROWS;i++) newRow(&q[i]);
    cur=0;
}

static int low(const Row *r) {
    int v=r->n[0];
    for(int i=1;i<4;i++) if(r->n[i]<v) v=r->n[i];
    return v;
}

static void nextRow(void) {
    cur++;
    if(cur >= ROWS-3) {
        for(int i=0;i<ROWS-2;i++) q[i]=q[i+2];
        newRow(&q[ROWS-2]);
        newRow(&q[ROWS-1]);
        cur=0;
    }
}

static void textTop(const char *s, int x, int y) {
    consoleSelect(&__console);
    iprintf("\x1b[%d;%dH%s", y+1, x/8+1, s);
}

static void textBottom(const char *s, int x, int y) {
    consoleSelect(&__consoleSub);
    iprintf("\x1b[%d;%dH%s", y+1, x/8+1, s);
}

/*
 * The DS console font is deliberately used for labels, while the bitmap
 * layer supplies the pastel game panels and large touch targets.
 */
static void drawTopArt(void) {
    memset(topBmp,0,W*H*2);
    rect(topBmp,0,0,W,H,CREAM);

    rect(topBmp,12,8,232,34,PINK);
    border(topBmp,12,8,232,34,WHITE);

    rect(topBmp,12,48,232,126,WHITE);
    border(topBmp,12,48,232,126,LAV);

    /* Preview cards. */
    for(int r=0;r<3;r++) {
        int y=57+r*38;
        u16 col=(r==0)?BLUE:(r==1)?MINT:LAV;
        rect(topBmp,22,y,212,30,col);
        border(topBmp,22,y,212,30,WHITE);
    }

    rect(topBmp,12,180,232,8,SHADOW);
}

static void drawBottomArt(void) {
    memset(bottomBmp,0,W*H*2);
    rect(bottomBmp,0,0,W,H,CREAM);

    rect(bottomBmp,10,7,236,30,BLUE);
    border(bottomBmp,10,7,236,30,WHITE);

    /* Four giant touch cards. */
    for(int i=0;i<4;i++) {
        int x=8+i*61;
        u16 col=(i%2==0)?PINK:MINT;
        rect(bottomBmp,x,58,55,82,col);
        border(bottomBmp,x,58,55,82,WHITE);
        rect(bottomBmp,x+5,63,45,72,WHITE);
    }

    rect(bottomBmp,34,151,188,28,LAV);
    border(bottomBmp,34,151,188,28,WHITE);
}

static void redrawText(void) {
    consoleSelect(&__console);
    consoleClear();
    consoleSelect(&__consoleSub);
    consoleClear();

    consoleSelect(&__console);
    textTop("LOWEST NUMBER",16,12);

    char line[64];
    const char *name = mode==SPRINT ? "30 SECOND SPRINT" :
                       mode==STREAK ? "STREAK" : "TIME BONUS";
    snprintf(line,sizeof(line),"%s  SCORE %d",name,score);
    textTop(line,16,38);

    if(mode==SPRINT || mode==BONUS)
        snprintf(line,sizeof(line),"TIME %02d  STREAK %d",seconds,streak);
    else
        snprintf(line,sizeof(line),"RUN %02d  BEST %d",elapsed,bestStreak);
    textTop(line,16,51);

    textTop("UP NEXT",28,61);

    for(int r=1;r<=3;r++) {
        snprintf(line,sizeof(line),"%d   %d   %d   %d",
                 q[cur+r].n[0],q[cur+r].n[1],q[cur+r].n[2],q[cur+r].n[3]);
        textTop(line,36,69+r*4);
    }

    consoleSelect(&__consoleSub);
    for(int i=0;i<4;i++) {
        snprintf(line,sizeof(line),"%d",q[cur].n[i]);
        textBottom(line,31+i*61,85);
    }
    textBottom("TAP THE LOWEST",67,156);

    if(flashFrames>0)
        textBottom(flashGood ? "NICE!" : "MISS!",102,164);
}

static void setupScreens(void) {
    videoSetMode(MODE_5_2D);
    videoSetModeSub(MODE_5_2D);

    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankC(VRAM_C_SUB_BG);

    int topBg = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0,0);
    int botBg = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0,0);

    topBmp = bgGetGfxPtr(topBg);
    bottomBmp = bgGetGfxPtr(botBg);

    /* Text console on BG0 over the bitmap art. */
    consoleInit(NULL,0,BgType_Text4bpp,BgSize_T_256x256,31,0,true,true);
    consoleInitDefault();
}

static void menuArt(void) {
    memset(topBmp,0,W*H*2);
    memset(bottomBmp,0,W*H*2);

    rect(topBmp,0,0,W,H,CREAM);
    rect(bottomBmp,0,0,W,H,CREAM);

    rect(topBmp,18,18,220,42,PINK);
    border(topBmp,18,18,220,42,WHITE);
    rect(topBmp,28,76,200,82,BLUE);
    border(topBmp,28,76,200,82,WHITE);

    rect(bottomBmp,20,15,216,30,LAV);
    border(bottomBmp,20,15,216,30,WHITE);

    rect(bottomBmp,20,55,216,32,PINK);
    rect(bottomBmp,20,94,216,32,MINT);
    rect(bottomBmp,20,133,216,32,BLUE);
}

static void menuText(void) {
    consoleSelect(&__console); consoleClear();
    consoleSelect(&__consoleSub); consoleClear();

    textTop("LOWEST NUMBER",42,30);
    textTop("TAP THE LOWEST",52,91);
    textTop("SEE THREE ROWS AHEAD",34,107);

    textBottom("CHOOSE A MODE",70,21);
    textBottom("30 SECOND SPRINT",43,62);
    textBottom("STREAK",92,101);
    textBottom("TIME BONUS",85,140);
}

static void start(Mode m) {
    mode=m; score=streak=bestStreak=elapsed=0; frameCount=0;
    seconds=(m==SPRINT)?30:(m==BONUS)?15:0;
    flashFrames=0;
    refill();
    drawTopArt();
    drawBottomArt();
    redrawText();
}

static void over(void) {
    consoleSelect(&__console); consoleClear();
    consoleSelect(&__consoleSub); consoleClear();

    memset(topBmp,0,W*H*2);
    memset(bottomBmp,0,W*H*2);
    rect(topBmp,0,0,W,H,PINK);
    rect(bottomBmp,0,0,W,H,CREAM);
    rect(bottomBmp,25,65,206,55,WHITE);
    border(bottomBmp,25,65,206,55,LAV);

    char s[64];
    textTop("GAME OVER",74,35);
    snprintf(s,sizeof(s),"SCORE %d",score); textTop(s,82,65);
    snprintf(s,sizeof(s),"BEST %d",bestStreak); textTop(s,82,80);
    textBottom("TAP TO RETURN",72,84);
    mode=MENU;
}

static void choose(int c) {
    bool good = q[cur].n[c] == low(&q[cur]);
    flashFrames=12; flashGood=good;

    if(good) {
        score++; streak++;
        if(streak>bestStreak) bestStreak=streak;
        if(mode==BONUS) { seconds+=2; if(seconds>99) seconds=99; }
        nextRow();
    } else {
        streak=0;
        if(mode==STREAK) { over(); return; }
        if(mode==BONUS) {
            seconds-=2;
            if(seconds<=0) { seconds=0; over(); return; }
        }
    }
    redrawText();
}

int main(void) {
    powerOn(POWER_ALL_2D);
    setupScreens();
    srand((unsigned)timerTicks2ms(timerGetValue(0)));

    menuArt();
    menuText();

    while(1) {
        scanKeys();
        u16 k=keysDown();

        if(mode==MENU) {
            if(k & KEY_TOUCH) {
                touchPosition t; touchRead(&t);
                if(t.py>=55 && t.py<87) start(SPRINT);
                else if(t.py>=94 && t.py<127) start(STREAK);
                else if(t.py>=133 && t.py<166) start(BONUS);
            }
        } else {
            if(k & KEY_TOUCH) {
                touchPosition t; touchRead(&t);
                int c=t.px/64;
                if(c>3)c=3;
                choose(c);
            }

            if(flashFrames>0) {
                flashFrames--;
                if(flashFrames==0) redrawText();
            }

            frameCount++;
            if(frameCount>=60) {
                frameCount=0;
                if(mode==SPRINT || mode==BONUS) {
                    seconds--;
                    if(seconds<=0) { seconds=0; over(); }
                } else if(mode==STREAK) {
                    elapsed++;
                }
                if(mode!=MENU) redrawText();
            }
        }
        swiWaitForVBlank();
    }
}
