/* Original-style menus and three-button interaction. GPL-3.0-or-later.
 * Uploaded art and sound scores come from the user's walker, never this
 * distribution. Menu behaviour/layout cross-checked with lumirth/pw (CC0).
 * This is a presentation/controller for the companion core, not H8 emulation. */
#include "walkerclassic.h"
#include "walkerfile.h"
#include <string.h>
#include <stdio.h>
#include "species.h"
#include "menuicons.h"
#include "movefacts.h"

enum { HOME,MENU,RADAR,DOWSE,BATTLE,PACK,TRAINER,SETTINGS,CONNECT,RESULT,PEERS };
enum { VIEW,SEL,SUB,CHOICE,TARGET,ROUND,TRIES,EMPTY,DEADLINE,RESULT_ID,SOUND,BAT,
       B1,B2,B3,B4,B5,B6,REPLACE,VOLUME,CONTRAST,ANIM,PREV_HP,PREV_ENEMY,LAST_ACTION,
       RADAR_SOUND_UNTIL };
enum { RADAR_DELAY,RADAR_ACTIVE,RADAR_REVEAL,RADAR_CLOSING,RADAR_ESCAPED };
enum { DOWSE_SELECT,DOWSE_REVEAL,DOWSE_FOUND,DOWSE_EMPTY,DOWSE_HINT };
enum { LCD_FRAME_MS=250 };
enum { BATTLE_ENCOUNTER_SCORE=10 };
enum { BPHASE=26,BSTART,BCODE,BACT,BPASS,BHPVIS,BWATTS,INVFRIEND,TRAINDAYS,CLOCK_NOW=37,CLOCK_MS,CLOCK_TZ };
static const unsigned radarWindows[]={64,48,32,24};
static const unsigned radarDelays[]={16,32,48};
static unsigned pollTime(unsigned polls){return polls*1000/32;}
static unsigned scoreDuration(const uint8_t *ee,unsigned score){
    return (walkerClassicSound(ee,score,0,220500)*1000+22049)/22050;
}
static unsigned radarSound(const uint8_t *ee,int32_t *s,unsigned score,uint32_t ms){
    unsigned duration=scoreDuration(ee,score);
    s[SOUND]=score+1;s[RADAR_SOUND_UNTIL]=ms+duration;
    return duration;
}
/* Item names for the 16-character LCD: a TM/HM name that is too long drops
 * its "TM59 " prefix so the move name stays readable. */
static const char *caps(const char *name);
static const char *lcdItem(unsigned id){
    const char *name=itemName(id);
    if(strlen(name)>16 && (name[0]=='T'||name[0]=='H') && name[1]=='M' && name[4]==' ')name+=5;
    return caps(name);
}
static unsigned rnd(uint8_t *ee,unsigned n){return (walkerRandom(ee)>>3)%n;}
int walkerRadarMarkCount(int round){
    if(round<2)return 1;
    return round==2?2:4; /* ! ! !! !!!! */
}
static void result(int32_t *s,int value,int score){s[VIEW]=RESULT;s[RESULT_ID]=value;s[SOUND]=score+1;}
static void battleStart(const uint8_t *ee,int32_t *s,uint32_t ms){
    WalkerBattle b;walkerBattleStart(&b,(unsigned)s[CHOICE]);
    s[BAT]=b.playerHp;s[BAT+1]=b.enemyHp;s[BAT+2]=b.row;s[BAT+3]=b.finished;
    s[BAT+4]=b.caught;s[BAT+5]=b.encounter;s[BAT+6]=b.lastOutcome;
    s[VIEW]=BATTLE;s[SUB]=0;s[ANIM]=-1;s[LAST_ACTION]=-1;s[SOUND]=BATTLE_ENCOUNTER_SCORE+1;
    s[BPHASE]=0;s[BSTART]=(int32_t)ms;s[BCODE]=s[BACT]=s[BPASS]=s[BHPVIS]=0;
    unsigned duration=s[VOLUME]?scoreDuration(ee,BATTLE_ENCOUNTER_SCORE):0;
    s[DEADLINE]=ms+(duration>650?duration:650);
}
static void storeReward(uint8_t *ee,int32_t *s,int slot,uint32_t now){
    int item=s[REPLACE]==2;
    int ok=item?walkerStoreItem(ee,s[CHOICE],slot,now):walkerStorePokemon(ee,s[CHOICE],slot,now);
    if(ok<0){s[VIEW]=PACK;s[SEL]=1;s[SUB]=0;s[SOUND]=0;} /* discard picker, starting on the middle slot */
    else {s[REPLACE]=0;result(s,ok?item?2:1:3,ok?5:4);}
}
/* ---- Original battle (phases, timings and tables from the firmware reconstruction) ---- */
enum { B_OPENING,B_ENCOUNTER,B_CHOOSE,B_PLAYER,B_OPPONENT,B_DEFEATED,B_WATTS,B_LEAVING,B_STARE,B_FAILED,
       B_THROW,B_CLOSE,B_SETTLE,B_CHECK,B_PAUSE,B_STARS,B_CONFIRM,B_BREAK };
enum { BATTLE_FRAME_MS=200 };
enum { MSG_NO_ITEMS=22,MSG_CAPTURED=32,MSG_ESCAPE,MSG_DEFEAT,MSG_ATTACK,MSG_DODGE,MSG_CRITICAL,MSG_HIT,MSG_THROW,MSG_ALMOST,MSG_STARE,MSG_DROPPED };
enum { SC_CONFIRM=0,SC_BACK=1,SC_MOVE=2,SC_FAILURE=4,SC_SUCCESS=7,SC_HIT=11,SC_DODGE=12,SC_CRITICAL=13,SC_ESCAPE=14,SC_THROW=15 };
static const uint8_t bLimit[]={6,3,0,8,8,6,8,10,6,6,6,2,3,4,4,6,6,1};
static const int8_t bEntrance[4]={-16,-4,8,8};
static const int8_t bPlayerTurn[9][2]={{56,8},{56,8},{54,8},{52,8},{53,0},{54,0},{55,4},{56,8},{56,8}};
static const int8_t bOpponentTurn[9][2]={{56,8},{56,8},{56,10},{56,12},{64,12},{64,11},{64,10},{60,9},{56,8}};
static const uint8_t bWeights[5][3]={{45,35,20},{40,30,30},{50,40,10},{60,30,10},{20,30,50}};
static const uint8_t bCapture[4]={0x61,0x4f,0x42,0x38};
static unsigned bFrame(const int32_t *s,uint32_t ms){
    unsigned f=(ms-(uint32_t)s[BSTART])/BATTLE_FRAME_MS,l=bLimit[s[BPHASE]];
    return l&&f>l?l:f;
}
static void bGo(int32_t *s,int phase,uint32_t ms){s[BPHASE]=phase;s[BSTART]=(int32_t)ms;s[ANIM]=-1;}
static void bLeave(int32_t *s,uint32_t ms){s[SOUND]=SC_ESCAPE+1;bGo(s,B_LEAVING,ms);}
static void bHome(int32_t *s){s[VIEW]=HOME;s[SEL]=0;s[SUB]=0;s[REPLACE]=0;}
/* Advance the battle exactly like BattleUpdate: damage lands when an animation finishes. */
static void battleStep(uint8_t *ee,int32_t *s,int button,uint32_t now,uint32_t ms){
    int pressed=button>=0&&button<=2;
    unsigned f=bFrame(s,ms),l=bLimit[s[BPHASE]];
    int done=l&&f>=l;
    switch(s[BPHASE]){
    case B_OPENING:if(done)bGo(s,B_ENCOUNTER,ms);break;
    case B_ENCOUNTER:
        if(f>=3)s[BHPVIS]=1;
        if(f>=3 && (int32_t)(ms-(uint32_t)s[DEADLINE])>=0 && pressed)bGo(s,B_CHOOSE,ms);
        break;
    case B_CHOOSE:{
        if(!pressed)break;
        unsigned roll=rnd(ee,100);const uint8_t *w=bWeights[s[BAT+2]%5];
        int code=roll<w[2]?2:roll<(unsigned)w[2]+w[1]?1:0;
        s[BCODE]=code;
        if(button==0){s[BACT]=0;bGo(s,B_PLAYER,ms);}
        else if(button==2){
            s[BACT]=1;
            if(code==2)bLeave(s,ms);
            else if(code==1){s[SOUND]=SC_CONFIRM+1;bGo(s,B_STARE,ms);}
            else bGo(s,B_OPPONENT,ms);
        }else{s[SOUND]=SC_THROW+1;s[BPASS]=0;bGo(s,B_THROW,ms);}
        break;}
    case B_PLAYER:
        if(f==4 && s[ANIM]!=4)s[SOUND]=(s[BCODE]==0?SC_HIT:s[BCODE]==1?SC_DODGE:SC_CRITICAL)+1;
        if(!done)break;
        if(s[BCODE]==1)bGo(s,B_OPPONENT,ms);
        else{
            int damage=s[BCODE]==2?2:1;
            if(s[BAT+1]<=damage){s[BAT+1]=0;bLeave(s,ms);}
            else{
                s[BAT+1]-=damage;
                if(s[BCODE]==0 && s[BACT]==1){s[BAT+2]=1;bGo(s,B_CHOOSE,ms);}
                else bGo(s,B_OPPONENT,ms);
            }
        }
        break;
    case B_OPPONENT:
        if(f==4 && s[ANIM]!=4)s[SOUND]=(s[BACT]==0?SC_HIT:SC_DODGE)+1;
        if(!done)break;
        if(s[BACT]==1){bGo(s,B_PLAYER,ms);break;}
        if(--s[BAT]>0){s[BAT+2]=s[BCODE]==0?1:s[BCODE]==1?3:2;bGo(s,B_CHOOSE,ms);}
        else{s[BAT]=0;bGo(s,B_DEFEATED,ms);}
        break;
    case B_DEFEATED:
        if(done){
            WalkerInfo info;unsigned lost=0;
            if(walkerRead(ee,&info))lost=info.watts<10?info.watts:10;
            if(lost)walkerSpend(ee,lost);
            s[BWATTS]=(int32_t)lost;bGo(s,B_WATTS,ms);
        }
        break;
    case B_WATTS:if(pressed){bHome(s);}break;
    case B_STARE:if(done){s[BAT+2]=4;bGo(s,B_CHOOSE,ms);}break;
    case B_LEAVING:if(pressed){s[SOUND]=SC_FAILURE+1;bHome(s);}break;
    case B_THROW:if(done){s[BHPVIS]=0;bGo(s,B_CLOSE,ms);}break;
    case B_CLOSE:if(done)bGo(s,B_SETTLE,ms);break;
    case B_SETTLE:if(done)bGo(s,B_CHECK,ms);break;
    case B_CHECK:
        if(!done)break;
        {unsigned roll=rnd(ee,100);int hp=s[BAT+1];
         if(hp>=1&&hp<=4&&roll<bCapture[hp-1]){s[BPASS]++;bGo(s,B_PAUSE,ms);}
         else bGo(s,B_BREAK,ms);}
        break;
    case B_PAUSE:
        if(!done)break;
        if(s[BPASS]<3)bGo(s,B_CHECK,ms);
        else{s[SOUND]=SC_CONFIRM+1;bGo(s,B_STARS,ms);}
        break;
    case B_STARS:if(done){s[SOUND]=SC_SUCCESS+1;bGo(s,B_CONFIRM,ms);}break;
    case B_CONFIRM:
        if(done&&pressed){
            s[BAT+4]=1;s[REPLACE]=1;storeReward(ee,s,-1,now);
            if(s[VIEW]==RESULT){bHome(s);s[SOUND]=0;} /* stored: back to the main screen */
        }
        break;
    case B_BREAK:if(done)bGo(s,B_FAILED,ms);break;
    case B_FAILED:if(done)bLeave(s,ms);break;
    }
    if(s[VIEW]==BATTLE)s[ANIM]=(int32_t)f;
}
/* ---- Pokemon & items: original entry order and navigation ---- */
static void invMasks(const uint8_t *ee,unsigned *walk,unsigned *friends){
    WalkerInfo info;unsigned w=0,f=0;
    if(walkerRead(ee,&info)&&info.hasPokemon)w|=1;
    for(unsigned n=0;n<3;n++){if(walkerPokemonId(ee,n))w|=2u<<n;if(walkerItemId(ee,n))w|=1u<<(6+n);}
    for(unsigned n=0;n<10;n++)if(walkerFriendItemId(ee,n))f|=1u<<n;
    *walk=w;*friends=f;
}
static int invNext(int *idx,unsigned mask){
    if(*idx==9)return 1;
    int i=*idx+1;
    for(int step=0;step<10;step++){if(mask&(1u<<i)){*idx=i;return 0;}if(i==9)return 1;i++;}
    return 0;
}
static int invPrevious(int *idx,unsigned mask){
    if(*idx==0)return 1;
    (*idx)--;
    for(int step=0;step<10;step++){if(mask&(1u<<*idx))return 0;if(*idx==0)return 1;(*idx)--;}
    return 0;
}
static void invNextWrap(int *idx,unsigned mask){
    *idx=(*idx+1)%10;
    for(int step=0;step<10;step++){if(mask&(1u<<*idx))return;*idx=(*idx+1)%10;}
}
static void invPreviousWrap(int *idx,unsigned mask){
    *idx=(*idx+9)%10;
    for(int step=0;step<10;step++){if(mask&(1u<<*idx))return;*idx=(*idx+9)%10;}
}
static void packBrowse(uint8_t *ee,int32_t *s,int button){
    unsigned w,f;invMasks(ee,&w,&f);int idx=s[SEL];
    if(!s[INVFRIEND]){
        if(button==0){
            if(invPrevious(&idx,w)){s[VIEW]=MENU;s[SEL]=4;s[SOUND]=SC_BACK+1;}else s[SEL]=idx;
        }else if(button==2){
            if(invNext(&idx,w)){
                if(f){idx=9;invNextWrap(&idx,f);s[INVFRIEND]=1;s[SEL]=idx;}else s[SOUND]=SC_BACK+1;
            }else s[SEL]=idx;
        }else{
            if(f){idx=9;invNextWrap(&idx,f);s[INVFRIEND]=1;s[SEL]=idx;}else{s[VIEW]=HOME;s[SEL]=0;s[SUB]=0;}
            s[SOUND]=SC_CONFIRM+1;
        }
    }else{
        if(button==0){
            if(invPrevious(&idx,f)){if(w){idx=0;invPreviousWrap(&idx,w);s[INVFRIEND]=0;s[SEL]=idx;}else s[SOUND]=SC_BACK+1;}
            else s[SEL]=idx;
        }else if(button==2){
            if(invNext(&idx,f))s[SOUND]=SC_BACK+1;else s[SEL]=idx;
        }else{s[VIEW]=HOME;s[SEL]=0;s[SUB]=0;s[INVFRIEND]=0;s[SOUND]=SC_CONFIRM+1;}
    }
}
static void trainerInput(int32_t *s,int button,uint32_t total){
    if(s[SUB]==0){
        if(button==0){
            if(s[TRAINDAYS]==0){s[VIEW]=MENU;s[SEL]=3;s[SOUND]=SC_BACK+1;}else s[TRAINDAYS]--;
            return;
        }
        if(button==2){if(s[TRAINDAYS]<7)s[TRAINDAYS]++;return;}
        s[SOUND]=SC_CONFIRM+1;
        if(total<9999999u){s[VIEW]=HOME;s[SEL]=0;s[SUB]=0;}else s[SUB]=1;
        return;
    }
    if(button==1){s[SOUND]=SC_CONFIRM+1;s[VIEW]=HOME;s[SEL]=0;s[SUB]=0;}
}
static void settingsInput(int32_t *s,int button){
    if(s[SUB]==0){
        if(button==0){if(s[SEL]==0){s[VIEW]=MENU;s[SEL]=5;s[SOUND]=SC_BACK+1;}else s[SEL]=0;}
        else if(button==2)s[SEL]=1;
        else{s[SUB]=s[SEL]+1;s[SOUND]=SC_CONFIRM+1;}
        return;
    }
    if(button==1){s[SOUND]=SC_CONFIRM+1;s[VIEW]=HOME;s[SEL]=0;s[SUB]=0;return;}
    if(s[SUB]==1)s[VOLUME]=(s[VOLUME]+(button==0?2:1))%3;
    else if(button==0){if(s[CONTRAST]>0)s[CONTRAST]--;else s[SOUND]=0;}
    else{if(s[CONTRAST]<9)s[CONTRAST]++;else s[SOUND]=0;}
}
/* Full party/pack: choose which slot to replace; leaving the new find asks first. */
static void discardInput(uint8_t *ee,int32_t *s,int button,uint32_t now){
    if(s[SUB]==1){ /* confirm: right = yes, leave it; anything else keeps it */
        if(button==2){s[REPLACE]=0;s[VIEW]=HOME;s[SEL]=0;s[SUB]=0;s[SOUND]=SC_BACK+1;}
        else{s[SUB]=0;s[SOUND]=SC_BACK+1;}
        return;
    }
    if(button==0){if(s[SEL]<=0){s[SUB]=1;s[SOUND]=SC_CONFIRM+1;}else s[SEL]--;return;}
    if(button==2){if(s[SEL]>=2)s[SOUND]=SC_BACK+1;else s[SEL]++;return;}
    storeReward(ee,s,s[SEL],now);
    if(s[VIEW]==RESULT){s[VIEW]=HOME;s[SEL]=0;s[SUB]=0;s[SOUND]=SC_CONFIRM+1;}
}
int walkerClassicInput(uint8_t *ee,int32_t s[40],int button,uint32_t now,uint32_t ms,int expanded){
    WalkerInfo info;if(!walkerRead(ee,&info)||!walkerCanWalk(ee))return 0;
    if(s[VIEW]<HOME||s[VIEW]>PEERS){memset(s,0,40*sizeof(*s));s[VOLUME]=2;s[CONTRAST]=1;}
    s[SOUND]=0;s[CLOCK_NOW]=(int32_t)now;s[CLOCK_MS]=(int32_t)ms;
    if(s[VIEW]==BATTLE){battleStep(ee,s,button,now,ms);return 1;}
    if(s[VIEW]==RADAR && (int32_t)(ms-(uint32_t)s[RADAR_SOUND_UNTIL])<0)return 1;
    if(s[VIEW]==RADAR && s[SUB]!=RADAR_ESCAPED && (int32_t)(ms-(uint32_t)s[DEADLINE])>=0){
        if(s[SUB]==RADAR_DELAY){
            s[SUB]=RADAR_ACTIVE;
            s[DEADLINE]+=pollTime(radarWindows[s[ROUND]]);
        }
        if(s[SUB]==RADAR_ACTIVE && (int32_t)(ms-(uint32_t)s[DEADLINE])>=0){
            s[SUB]=RADAR_ESCAPED;s[SOUND]=15;return 1;
        }
        if(s[SUB]==RADAR_REVEAL){
            if(s[ROUND]+1>=s[TRIES]){
                s[SUB]=RADAR_CLOSING;s[ANIM]=ms;s[DEADLINE]=ms+5*LCD_FRAME_MS;
            }else{
                unsigned delay=16+(uint8_t)(walkerRandom(ee)>>2)%radarDelays[s[ROUND]];
                s[ROUND]++;s[SUB]=RADAR_DELAY;s[TARGET]=(walkerRandom(ee)>>5)&3;
                s[DEADLINE]=ms+pollTime(delay);
            }
            return 1;
        }
        if(s[SUB]==RADAR_CLOSING){battleStart(ee,s,ms);return 1;}
    }
    if(s[VIEW]==DOWSE && s[SUB]==DOWSE_REVEAL && (int32_t)(ms-(uint32_t)s[DEADLINE])>=0){
        if(s[SEL]==s[TARGET]){
            unsigned choice;
            if(!walkerChooseItemEx(ee,&choice,expanded)){result(s,3,4);return 1;}
            s[CHOICE]=choice;s[SUB]=DOWSE_FOUND;s[SOUND]=6;
        }else{
            if(s[TRIES]==2)s[EMPTY]=s[SEL];
            s[TRIES]--;s[SUB]=DOWSE_EMPTY;s[SOUND]=5;
        }
        return 1;
    }
    /* A held centre button exits browsing, without abandoning paid activities. */
    if(button==3){
        if(s[VIEW]==MENU || (s[VIEW]==PACK && !s[REPLACE]) || s[VIEW]==TRAINER || s[VIEW]==SETTINGS || s[VIEW]==CONNECT){
            s[VIEW]=HOME;s[SEL]=0;s[SUB]=0;s[SOUND]=2;
        }
        return 1;
    }
    if(button<0)return 1;
    if(button>2)return 0;
    s[SOUND]=3;
    switch(s[VIEW]){
    case HOME:s[VIEW]=MENU;s[SUB]=0;s[SEL]=button==0?5:button==1?2:0;s[SOUND]=1;break;
    case MENU:
        if(s[SUB]!=0){s[SUB]=0;s[SOUND]=SC_CONFIRM+1;break;} /* any button dismisses an error */
        if(button!=1){ /* moving past either end of the carousel returns to the main screen */
            if((button==0&&s[SEL]<=0)||(button==2&&s[SEL]>=5)){s[VIEW]=HOME;s[SEL]=0;s[SUB]=0;s[SOUND]=2;break;}
            s[SEL]+=button==0?-1:1;break;
        }
        s[SEL]=s[SEL]<0||s[SEL]>5?0:s[SEL];s[SUB]=0;s[SOUND]=1;
        {
            const unsigned cost=s[SEL]==0?10u:s[SEL]==1?3u:0u;
            if(info.watts<cost){s[SUB]=1;s[SOUND]=SC_FAILURE+1;break;}      /* not enough watts */
            if(s[SEL]==0){
                unsigned visitor=walkerPendingEncounter(ee);WalkerEncounter encounter;
                if(!info.hasPokemon){s[SUB]=2;s[SOUND]=SC_FAILURE+1;break;} /* no Pokemon */
                if(visitor){s[CHOICE]=visitor-1;walkerClearEncounter(ee);battleStart(ee,s,ms);break;}
                if(!walkerChooseEncounter(ee,&encounter)||!walkerSpend(ee,10)){s[SUB]=2;s[SOUND]=SC_FAILURE+1;break;}
                s[CHOICE]=encounter.index;s[ROUND]=0;s[TRIES]=3-(int)encounter.index+(int)rnd(ee,2);
                s[VIEW]=RADAR;s[SEL]=0;s[SUB]=RADAR_DELAY;
                s[TARGET]=(walkerRandom(ee)>>5)&3;s[DEADLINE]=ms+pollTime(5)+radarSound(ee,s,0,ms);
            }else if(s[SEL]==1){
                if(!walkerSpend(ee,3)){s[SUB]=1;s[SOUND]=SC_FAILURE+1;break;}
                s[VIEW]=DOWSE;s[SUB]=DOWSE_SELECT;s[TARGET]=(uint8_t)(walkerRandom(ee)>>5)%6;
                s[SEL]=0;s[TRIES]=2;s[EMPTY]=-1;
            }else if(s[SEL]==4){
                unsigned w,f;invMasks(ee,&w,&f);
                if(!w&&!f){s[SUB]=3;s[SOUND]=SC_FAILURE+1;break;}         /* nothing to show */
                int idx=9;invNextWrap(&idx,w?w:f);
                s[VIEW]=PACK;s[REPLACE]=0;s[INVFRIEND]=w==0;s[SEL]=idx;
            }else{
                s[VIEW]=s[SEL]==2?CONNECT:s[SEL]==3?TRAINER:SETTINGS;s[SEL]=0;s[REPLACE]=0;s[TRAINDAYS]=0;s[INVFRIEND]=0;
            }
        }
        break;
    case RADAR:
        if(s[SUB]==RADAR_ESCAPED){s[VIEW]=HOME;s[SOUND]=1;break;}
        if(s[SUB]!=RADAR_DELAY && s[SUB]!=RADAR_ACTIVE){s[SOUND]=0;break;}
        if(button!=1){
            s[SEL]=(s[SEL]+(button==0?3:1))%4;
            s[DEADLINE]+=radarSound(ee,s,2,ms);
            break;
        }
        if(s[SEL]!=s[TARGET]){
            if(s[ROUND]>0){s[SUB]=RADAR_ESCAPED;s[SOUND]=15;}
            else{s[DEADLINE]+=pollTime(1)+radarSound(ee,s,4,ms);}
            break;
        }
        s[SUB]=RADAR_REVEAL;s[DEADLINE]=ms+pollTime(16)+radarSound(ee,s,3,ms);
        break;
    case DOWSE:
        if(s[SUB]==DOWSE_REVEAL){s[SOUND]=0;break;}
        if(s[SUB]==DOWSE_FOUND){
            s[REPLACE]=2;storeReward(ee,s,-1,now);
            if(s[VIEW]==PACK)s[SUB]=0;
            if(s[VIEW]==RESULT && s[RESULT_ID]==2){s[VIEW]=HOME;s[SOUND]=1;}
            break;
        }
        if(s[SUB]==DOWSE_EMPTY){
            if(s[TRIES]==0)s[VIEW]=HOME;
            else s[SUB]=DOWSE_HINT;
            s[SOUND]=1;break;
        }
        if(s[SUB]==DOWSE_HINT){s[SUB]=DOWSE_SELECT;s[SOUND]=1;break;}
        if(button!=1){s[SEL]=(s[SEL]+(button==0?5:1))%6;break;}
        if(s[SEL]==s[EMPTY]){s[SOUND]=2;break;}
        s[SUB]=DOWSE_REVEAL;s[ANIM]=ms;s[DEADLINE]=ms+4*LCD_FRAME_MS;s[SOUND]=1;
        break;
    case BATTLE:break; /* driven by battleStep above */
    case PACK:
        if(s[REPLACE])discardInput(ee,s,button,now);else packBrowse(ee,s,button);
        break;
    case TRAINER:trainerInput(s,button,info.totalSteps);break;
    case SETTINGS:settingsInput(s,button);break;
    case CONNECT:
        if(button!=1)s[SEL]=(s[SEL]+1)%2;
        else {s[VIEW]=s[SEL]?PEERS:CONNECT;s[SUB]=s[SEL]?0:1;s[SOUND]=1;}
        break;
    case PEERS:break; /* host supplies the available local profiles */
    case RESULT:s[VIEW]=HOME;s[SOUND]=2;break;
    }
    return 1;
}
static void pixel(uint8_t *out,int x,int y,unsigned color){if(x>=0&&x<96&&y>=0&&y<64)out[y*96+x]=(uint8_t)color;}
static void rect(uint8_t *out,int x,int y,int w,int h,unsigned color){for(int j=0;j<h;j++)for(int i=0;i<w;i++)pixel(out,x+i,y+j,color);}
static void drawRaster(const uint8_t *ee,uint8_t *out,unsigned offset,int x,int y,int w,int h,int flip,int opaque){
    if(offset+(unsigned)(w*h/4)>65536)return;
    for(int j=0;j<h;j++)for(int i=0;i<w;i++){
        unsigned n=offset+(j/8)*w*2+i*2,c=(((ee[n]>>(j&7))&1)<<1)|((ee[n+1]>>(j&7))&1);
        if(c || opaque)pixel(out,x+(flip?w-1-i:i),y+j,c);
    }
}
static void raster(const uint8_t *ee,uint8_t *out,unsigned offset,int x,int y,int w,int h,int flip){
    drawRaster(ee,out,offset,x,y,w,h,flip,0);
}
/* Original uploaded 8x16 number glyphs. */
static void number(const uint8_t *ee,uint8_t *out,uint32_t value,int x,int y,int digits){
    char text[16];snprintf(text,sizeof(text),"%0*lu",digits,(unsigned long)value);
    unsigned length=(unsigned)strlen(text);if(length>10)return;
    for(unsigned n=0;n<length;n++)raster(ee,out,0x280+(text[n]-'0')*32,x+n*8,y,8,16,0);
}
/* Names (Pokemon, items, trainer, course) are drawn in capitals like the rest of the LCD; messages use sentence case. */
static const char *caps(const char *name){
    static char buffer[4][40];static unsigned slot;
    char *out=buffer[slot++&3];unsigned n=0;
    for(;name[n]&&n<39;n++)out[n]=name[n]>='a'&&name[n]<='z'?(char)(name[n]-32):name[n];
    out[n]=0;return out;
}
static const char *speciesCaps(unsigned id){return caps(speciesName(id));}
/* ---- Text for screens the walker has no artwork for ----
 * The walker's own text is pre-rendered art in its EEPROM. To draw new text in the same font we read the
 * letter shapes straight out of that art (names, course, trainer, item names all have known spelling) and
 * fall back to a built-in copy of the font for any letter the walker's text happens not to contain. */
/* Pixel shapes of the walker LCD text font (cap height 8, x-height 5). Rows start at `top` relative to the cap line. */
static const struct { char ch; signed char top; unsigned char rows; const char *bits[10]; } builtinFont[]={
    {'!',0,8,{"#","#","#","#","#",".",".","#"}},
    {'%',0,8,{"#..#","...#","..#.","..#.",".#..",".#..","#...","#..#"}},
    {'\'',-1,4,{"##","##",".#","#."}},
    {'(',0,8,{"..#",".#.","#..","#..","#..","#..","#..",".#."}},
    {')',0,8,{"#..",".#.","..#","..#","..#","..#","..#",".#."}},
    {'+',2,5,{".#.",".#.","###",".#.",".#."}},
    {',',7,2,{"#","#"}},
    {'-',4,1,{"###"}},
    {'.',7,1,{"#"}},
    {'/',0,8,{"...#","...#","..#.","..#.",".#..",".#..","#...","#..."}},
    {'0',0,8,{".##.","#..#","#..#","#..#","#..#","#..#","#..#",".##."}},
    {'1',0,8,{".#.","##.",".#.",".#.",".#.",".#.",".#.","###"}},
    {'2',0,8,{".##.","#..#","...#","..#.",".#..","#...","#...","####"}},
    {'3',0,8,{"###.","...#","...#",".##.","...#","...#","#..#",".##."}},
    {'4',0,8,{"...#","..##",".#.#","#..#","####","...#","...#","...#"}},
    {'5',0,8,{"####","#...","###.","...#","...#","...#","#..#",".##."}},
    {'6',0,8,{".##.","#...","#...","###.","#..#","#..#","#..#",".##."}},
    {'7',0,8,{"####","...#","...#","..#.","..#.",".#..",".#..",".#.."}},
    {'8',0,8,{".##.","#..#","#..#",".##.","#..#","#..#","#..#",".##."}},
    {'9',0,8,{".##.","#..#","#..#",".###","...#","...#","...#",".##."}},
    {':',2,5,{"#",".",".",".","#"}},
    {'?',0,8,{".##.","#..#","...#","..#.",".#..","....","....",".#.."}},
    {'A',0,8,{".##.","#..#","#..#","#..#","####","#..#","#..#","#..#"}},
    {'B',0,8,{"###.","#..#","#..#","###.","#..#","#..#","#..#","###."}},
    {'C',0,8,{".##.","#..#","#..#","#...","#...","#..#","#..#",".##."}},
    {'D',0,8,{"###.","#..#","#..#","#..#","#..#","#..#","#..#","###."}},
    {'E',0,8,{"####","#...","#...","####","#...","#...","#...","####"}},
    {'F',0,8,{"####","#...","#...","###.","#...","#...","#...","#..."}},
    {'G',0,8,{".##.","#..#","#...","#.##","#..#","#..#","#..#",".##."}},
    {'H',0,8,{"#..#","#..#","#..#","####","#..#","#..#","#..#","#..#"}},
    {'I',0,8,{"###",".#.",".#.",".#.",".#.",".#.",".#.","###"}},
    {'J',0,8,{"...#","...#","...#","...#","...#","#..#","#..#",".##."}},
    {'K',0,8,{"#..#","#..#","#.#.","##..","##..","#.#.","#..#","#..#"}},
    {'L',0,8,{"#...","#...","#...","#...","#...","#...","#...","####"}},
    {'M',0,8,{"#...#","##.##","##.##","#.#.#","#.#.#","#...#","#...#","#...#"}},
    {'N',0,8,{"#..#","#..#","##.#","##.#","#.##","#.##","#..#","#..#"}},
    {'O',0,8,{".##.","#..#","#..#","#..#","#..#","#..#","#..#",".##."}},
    {'P',0,8,{"###.","#..#","#..#","###.","#...","#...","#...","#..."}},
    {'Q',0,8,{".##.","#..#","#..#","#..#","#..#","#.##","#..#",".###"}},
    {'R',0,8,{"###.","#..#","#..#","#..#","###.","#.#.","#..#","#..#"}},
    {'S',0,8,{".##.","#..#","#...",".#..","..#.","...#","#..#",".##."}},
    {'T',0,8,{"###",".#.",".#.",".#.",".#.",".#.",".#.",".#."}},
    {'U',0,8,{"#..#","#..#","#..#","#..#","#..#","#..#","#..#",".##."}},
    {'V',0,8,{"#..#","#..#","#..#","#..#","#..#","#..#",".##.",".##."}},
    {'W',0,8,{"#...#","#...#","#...#","#...#","#.#.#","#.#.#","##.##","#...#"}},
    {'X',0,8,{"#..#","#..#",".##.",".##.",".##.",".##.","#..#","#..#"}},
    {'Y',0,8,{"#..#","#..#","#..#",".##.",".##.",".##.",".##.",".##."}},
    {'Z',0,8,{"####","...#","..#.","..#.",".#..",".#..","#...","####"}},
    {'a',3,5,{".##.","...#",".###","#..#",".###"}},
    {'b',0,8,{"#...","#...","#...","###.","#..#","#..#","#..#","###."}},
    {'c',3,5,{".##.","#..#","#...","#..#",".##."}},
    {'d',0,8,{"...#","...#","...#",".###","#..#","#..#","#..#",".###"}},
    {'e',3,5,{".##.","#..#","####","#...",".###"}},
    {'f',0,8,{".##","#..","#..","###","#..","#..","#..","#.."}},
    {'g',3,7,{".###","#..#","#..#",".###","...#","...#",".##."}},
    {'h',0,8,{"#...","#...","#...","###.","#..#","#..#","#..#","#..#"}},
    {'i',1,7,{"#",".","#","#","#","#","#"}},
    {'j',1,9,{".#","..",".#",".#",".#",".#",".#",".#","#."}},
    {'k',0,8,{"#...","#...","#...","#..#","#.#.","##..","#.#.","#..#"}},
    {'l',0,8,{"#","#","#","#","#","#","#","#"}},
    {'m',3,5,{"####.","#.#.#","#.#.#","#.#.#","#.#.#"}},
    {'n',3,5,{"###.","#..#","#..#","#..#","#..#"}},
    {'o',3,5,{".##.","#..#","#..#","#..#",".##."}},
    {'p',3,7,{"###.","#..#","#..#","#..#","###.","#...","#..."}},
    {'q',3,7,{".###","#..#","#..#","#..#",".###","...#","...#"}},
    {'r',3,5,{"#.##","##..","#...","#...","#..."}},
    {'s',3,5,{".###","#...",".##.","...#","###."}},
    {'t',2,6,{".#.","###",".#.",".#.",".#.","..#"}},
    {'u',3,5,{"#..#","#..#","#..#","#..#",".###"}},
    {'v',3,5,{"#..#","#..#","#..#",".##.",".##."}},
    {'w',3,5,{"#...#","#...#","#.#.#","#.#.#",".#.#."}},
    {'x',3,5,{"#..#","#..#",".##.","#..#","#..#"}},
    {'y',3,7,{"#..#","#..#","#..#",".###","...#","...#",".##."}},
    {'z',3,5,{"####","...#",".##.","#...","####"}},
};
typedef struct { uint16_t col[8]; uint8_t width,advance,have; } Glyph;   /* column bit n = row n-1 below the cap line */
static Glyph builtinGlyphs[128],atlasGlyphs[128];
static int builtinReady;
static const uint8_t *labelSource;           /* walker image the current frame is drawn from */
static uint32_t atlasSignature;static int atlasValid;
static void buildBuiltin(void){
    if(builtinReady)return;
    for(unsigned n=0;n<sizeof(builtinFont)/sizeof(builtinFont[0]);n++){
        Glyph *g=&builtinGlyphs[(unsigned char)builtinFont[n].ch&127];
        for(int r=0;r<builtinFont[n].rows;r++){
            const char *bits=builtinFont[n].bits[r];
            for(int c=0;bits[c]&&c<8;c++){
                if(bits[c]=='#')g->col[c]|=(uint16_t)(1u<<(builtinFont[n].top+r+1));
                if(c+1>g->width)g->width=(uint8_t)(c+1);
            }
        }
        g->advance=(uint8_t)(g->width+1);g->have=1;
    }
    builtinGlyphs['\'' ].advance=4;
    builtinReady=1;
}
static unsigned artPixel(const uint8_t *ee,unsigned addr,int w,int x,int y){
    unsigned n=addr+(unsigned)((y/8)*w*2+x*2);
    return (((ee[n]>>(y&7))&1)<<1)|((ee[n+1]>>(y&7))&1);
}
/* Learn glyphs from one piece of text art whose spelling we know. Skipped unless every letter checks out. */
static void atlasLearn(const uint8_t *ee,unsigned addr,int w,const char *text){
    if(!text||!*text||strlen(text)>30||strstr(text,"Poke"))return;
    char chars[32];unsigned spaceBefore[32];unsigned count=0,pendingSpace=0;
    for(const char *p=text;*p;p++){
        if(*p==' '){pendingSpace=1;continue;}
        if(!((*p>='A'&&*p<='Z')||(*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||*p=='\''||*p=='.'||*p=='-'))return;
        if(count>=31)return;
        chars[count]=*p;spaceBefore[count++]=pendingSpace;pendingSpace=0;
    }
    int gStart[32],gEnd[32],groups=0,start=-1;
    for(int x=0;x<=w;x++){
        int ink=0;
        if(x<w)for(int y=0;y<16;y++)if(artPixel(ee,addr,w,x,y)==3){ink=1;break;}
        if(ink&&start<0)start=x;
        else if(!ink&&start>=0){if(groups>=32)return;gStart[groups]=start;gEnd[groups++]=x-1;start=-1;}
    }
    if((unsigned)groups!=count)return;
    int top=99,minY[32],maxY[32];
    for(int g=0;g<groups;g++){
        minY[g]=99;maxY[g]=-1;
        for(int x=gStart[g];x<=gEnd[g];x++)for(int y=0;y<16;y++)if(artPixel(ee,addr,w,x,y)==3){if(y<minY[g])minY[g]=y;if(y>maxY[g])maxY[g]=y;}
        if((chars[g]>='A'&&chars[g]<='Z')||(chars[g]>='0'&&chars[g]<='9')){if(minY[g]<top)top=minY[g];}
    }
    if(top==99)return;
    for(int g=0;g<groups;g++){   /* reject text whose letters don't sit where this font puts them (different case/spelling) */
        char c=chars[g];int rel=minY[g]-top,low=maxY[g]-top;
        if((c>='A'&&c<='Z')||(c>='0'&&c<='9')){if(rel!=0||low<6||low>7)return;}
        else if(strchr("acemnorsuvwxz",c)){if(rel<2||rel>4||low!=7)return;}
        else if(strchr("bdfhkl",c)){if(rel>1||low!=7)return;}
        else if(c=='t'){if(rel<1||rel>3||low!=7)return;}
        else if(c=='i'){if(rel<0||rel>2||low!=7)return;}
        else if(strchr("gpqy",c)){if(rel<2||rel>4||low<8)return;}
        else if(c=='j'){if(rel<0||rel>2||low<8)return;}
        if(gEnd[g]-gStart[g]+1>8)return;
    }
    for(int g=0;g<groups;g++){
        Glyph *dst=&atlasGlyphs[(unsigned char)chars[g]&127];
        if(dst->have)continue;
        Glyph tmp;memset(&tmp,0,sizeof(tmp));
        tmp.width=(uint8_t)(gEnd[g]-gStart[g]+1);
        for(int c=0;c<tmp.width;c++)for(int y=top-1;y<=top+9&&y<16;y++)
            if(y>=0&&artPixel(ee,addr,w,gStart[g]+c,y)==3)tmp.col[c]|=(uint16_t)(1u<<(y-top+1));
        tmp.advance=(uint8_t)(g+1<groups&&!spaceBefore[g+1]?gStart[g+1]-gStart[g]:tmp.width+1);
        tmp.have=1;*dst=tmp;
    }
}
static uint32_t atlasHash(const uint8_t *ee){
    static const struct { unsigned addr,bytes; } spans[]={{0x1250,320},{0x907e,320},{0x993e,320},{0xa4fe,960},{0xa8be,3840},{0x910+3*320,320}};
    uint32_t h=2166136261u;
    for(unsigned s=0;s<sizeof(spans)/sizeof(spans[0]);s++)for(unsigned n=0;n<spans[s].bytes;n++)h=(h^ee[spans[s].addr+n])*16777619u;
    return h;
}
static void atlasBuild(const uint8_t *ee){
    uint32_t sig=atlasHash(ee);
    if(atlasValid&&sig==atlasSignature)return;
    memset(atlasGlyphs,0,sizeof(atlasGlyphs));
    WalkerInfo info;
    if(walkerRead(ee,&info)){
        atlasLearn(ee,0x1250,80,info.trainer);
        atlasLearn(ee,0x907e,80,info.route);
        if(info.hasPokemon)atlasLearn(ee,0x993e,80,caps(speciesName(info.species)));
        for(unsigned n=0;n<3;n++)atlasLearn(ee,0xa4fe + n*320,80,caps(speciesName(walkerRoutePokemonId(ee,n))));
        for(unsigned n=0;n<10;n++)atlasLearn(ee,0xa8be + n*384,96,itemName(walkerRouteItemId(ee,n)));
    }
    atlasLearn(ee,0x910+3*320,80,"TRAINER CARD");
    atlasSignature=sig;atlasValid=1;
}
/* Draw text in the LCD font: 1px ink with the font's grey drop shadow, proportional spacing. */
static void label(uint8_t *out,const char *text,int x,int y){
    buildBuiltin();
    if(labelSource)atlasBuild(labelSource);
    for(int pass=0;pass<2;pass++){
        int gx=x;
        for(const char *t=text;*t;t++){
            unsigned char c=(unsigned char)*t&127;
            if(c==' '){gx+=3;continue;}
            const Glyph *g=atlasValid&&atlasGlyphs[c].have?&atlasGlyphs[c]:builtinGlyphs[c].have?&builtinGlyphs[c]:&builtinGlyphs['?'];
            if(!g->have)continue;
            if(gx+g->width>95)break;
            for(int cx=0;cx<g->width;cx++)for(int bit=0;bit<11;bit++)if(g->col[cx]&(1u<<bit)){
                int px=gx+cx+(pass==0),py=y+bit-1+(pass==0);
                if(px<0||px>=96||py<0||py>=64)continue;
                if(pass==0){if(!out[py*96+px])out[py*96+px]=2;}else out[py*96+px]=3;
            }
            gx+=g->advance;
        }
    }
}

/* Retail EEPROM artwork offsets from the Pokéwalker firmware reconstruction. */
enum {
    LCD_TREASURE=0x488, LCD_CURSOR_UP=0x4f8, LCD_CURSOR_RIGHT=0x588,
    LCD_NEXT=0x638, LCD_NEXT_MASK=0x648,
    LCD_GRASS=0x1b50, LCD_SEARCHED_GRASS=0x1b90, LCD_ATTEMPTS=0x1bd0,
    LCD_ATTEMPTS_SUFFIX=0x1c50, LCD_RADAR_GRASS=0x1cb0, LCD_RADAR_BUBBLE=0x1d70,
    LCD_HP=0x2030, LCD_BATTLE_MENU=0x2050, LCD_MESSAGES=0x2530,
    LCD_ENCOUNTER_NAMES=0xa4fe, LCD_ITEM_NAMES=0xa8be
};
enum { LCD_TOP=1,LCD_BOTTOM=2,LCD_LEFT=4,LCD_RIGHT=8,LCD_SIDES=12,LCD_BOX=15 };
enum {
    LCD_DOWSE_SEARCH=23,LCD_FOUND,LCD_DOWSE_EMPTY,LCD_DOWSE_NEAR,LCD_DOWSE_FAR,
    LCD_RADAR_SEARCH,LCD_RADAR_RESPONSE,LCD_RADAR_ESCAPE,LCD_BATTLE_ENCOUNTER
};

static int hasArtwork(const uint8_t *ee,unsigned offset,unsigned length){
    for(unsigned i=1;i<length;i++)if(ee[offset+i]!=ee[offset])return 1;
    return 0;
}

static void lcdPanel(const uint8_t *ee,uint8_t *out,unsigned address,int y,
                     int edges,int prompt,unsigned frame,const char *fallback){
    rect(out,0,y,96,16,0);
    if(address && hasArtwork(ee,address,384))drawRaster(ee,out,address,0,y,96,16,0,1);
    else label(out,fallback,2,y+4);
    if(edges&LCD_TOP)rect(out,0,y,96,1,3);
    if(edges&LCD_BOTTOM)rect(out,0,y+15,96,1,3);
    if(edges&LCD_LEFT)rect(out,0,y,1,16,3);
    if(edges&LCD_RIGHT)rect(out,95,y,1,16,3);
    if(prompt && (frame&2)){
        for(int x=0;x<8;x++)for(int row=0;row<8;row++)
            if(!(ee[LCD_NEXT_MASK+x]&(1u<<row)))pixel(out,88+x,y+8+row,0);
        raster(ee,out,LCD_NEXT,88,y+8,8,8,0);
    }
}

static void lcdMessage(const uint8_t *ee,uint8_t *out,int message,int edges,
                       int prompt,unsigned frame,const char *fallback){
    lcdPanel(ee,out,LCD_MESSAGES+message*384,48,edges,prompt,frame,fallback);
}

int walkerVisitFrame(const uint8_t *ee,const WalkerVisitReward *reward,
                     unsigned frame,uint8_t out[6144]){
    enum { ENTER,GREETING,PLAY,PRESENT,REWARD };
    enum { NOTE_ART=0x2470,PRESENT_ART=0x1a90,
           GREETING_MESSAGE=43,ONE_NOTE_MESSAGE=48,PRESENT_MESSAGE=49,RECEIVED_MESSAGE=15 };
    labelSource=ee;
    if(frame>=WALKER_VISIT_FRAME_COUNT || reward->notes<1 || reward->notes>5 ||
       reward->itemIndex>=10 || reward->watts>99)return 0;
    unsigned phase=frame/8, step=frame%8;
    memset(out,0,WALKER_CLASSIC_PIXELS);
    if(phase<PRESENT){
        int peerX=phase==ENTER?8-(7-(int)step)*3:8;
        raster(ee,out,0x91be + (frame&1)*192,56,8,32,24,0);
        raster(ee,out,0xf400+(frame&1)*192,peerX,8,32,24,!(ee[0xf6f7]&1));
    }
    if(phase==GREETING){
        drawRaster(ee,out,0xf580,2,32,80,16,0,1);
        rect(out,0,32,96,1,3);
        rect(out,0,32,1,16,3);
        rect(out,95,32,1,16,3);
        lcdMessage(ee,out,GREETING_MESSAGE,LCD_BOTTOM|LCD_SIDES,0,frame,"Hello!");
    }else if(phase==PLAY){
        static const unsigned shifts[]={0,1,2,1,0};
        unsigned notes=reward->notes, count=notes>=4?step+1:step/2+1;
        unsigned start=notes>=4?0:notes==1?2:1;
        int uploadedNote=hasArtwork(ee,NOTE_ART,16);
        if(count>notes)count=notes;
        for(unsigned n=start;n<start+count;n++){
            int left=28+n*8;
            if(uploadedNote){
                for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++){
                    unsigned bit=y+shifts[n];
                    unsigned a=ee[NOTE_ART+x*2], b=ee[NOTE_ART+x*2+1];
                    pixel(out,left+x,y,((a>>bit)&1)*2+((b>>bit)&1));
                }
            }else{
                int y=2-(int)shifts[n];
                rect(out,left+2,y+4,3,2,3);
                rect(out,left+4,y,1,5,3);
                rect(out,left+5,y+1,2,1,3);
            }
        }
        lcdMessage(ee,out,ONE_NOTE_MESSAGE+1-notes,LCD_BOX,0,frame,"They played!");
    }else if(phase>=PRESENT){
        raster(ee,out,PRESENT_ART,32,4,32,24,0);
        if(phase==PRESENT){
            lcdMessage(ee,out,PRESENT_MESSAGE,LCD_BOX,0,frame,"A present!");
        }else{
            if(reward->watts){
                number(ee,out,reward->watts,reward->watts>=10?2:10,32,1);
                raster(ee,out,0x420,18,32,16,16,0);
                rect(out,0,32,96,1,3);
                rect(out,0,32,1,16,3);
                rect(out,95,32,1,16,3);
            }else{
                unsigned item=walkerRouteItemId(ee,reward->itemIndex);
                lcdPanel(ee,out,LCD_ITEM_NAMES+384*reward->itemIndex,32,LCD_TOP|LCD_SIDES,
                         0,frame,lcdItem(item));
            }
            lcdMessage(ee,out,RECEIVED_MESSAGE,LCD_BOTTOM|LCD_SIDES,0,frame,"Received!");
        }
    }
    return 1;
}

static void radarFrame(const uint8_t *ee,const int32_t *s,uint8_t *out,uint32_t ms,unsigned frame){
    static const int patchX[]={8,16,56,64};
    int phase=s[SUB];
    if(phase!=RADAR_ESCAPED)
        raster(ee,out,LCD_CURSOR_RIGHT+16*(frame&1),patchX[s[SEL]]-8,24*(s[SEL]&1)+8,8,8,0);
    for(int i=0;i<4;i++)raster(ee,out,LCD_RADAR_GRASS,patchX[i],24*(i&1),32,24,0);
    if(phase==RADAR_ACTIVE || phase==RADAR_REVEAL || phase==RADAR_CLOSING){
        int marks=walkerRadarMarkCount(s[ROUND]);
        int bubble=phase==RADAR_ACTIVE?marks-1:3;
        int x=patchX[s[TARGET]]+16,y=24*(s[TARGET]&1);
        unsigned address=LCD_RADAR_BUBBLE+64*bubble;
        if(hasArtwork(ee,address,64))drawRaster(ee,out,address,x,y,16,16,0,1);
        else{
            static const char *const fallback[]={"!","!!","!!!","!!!!"};
            label(out,fallback[bubble],x,y+4);
        }
    }
    if(phase==RADAR_CLOSING){
        unsigned height=(ms-(uint32_t)s[ANIM])/LCD_FRAME_MS*8;
        if(height>32)height=32;
        rect(out,0,0,96,height,3);rect(out,0,64-height,96,height,3);
    }else if(phase==RADAR_ESCAPED){
        lcdMessage(ee,out,LCD_RADAR_ESCAPE,LCD_BOX,1,frame,"It got away!");
    }else if(phase==RADAR_REVEAL){
        lcdMessage(ee,out,LCD_RADAR_RESPONSE,LCD_BOX,0,frame,"There it is!");
    }else{
        lcdMessage(ee,out,LCD_RADAR_SEARCH,LCD_BOX,0,frame,"Find a Pokemon!");
    }
}

static void dowsingFrame(const uint8_t *ee,const int32_t *s,uint8_t *out,unsigned frame){
    int phase=s[SUB];
    raster(ee,out,WALKER_ROUTE+0xbe,0,0,32,24,0);
    raster(ee,out,LCD_ATTEMPTS,32,0,32,16,0);
    number(ee,out,s[TRIES],64,0,1);
    raster(ee,out,LCD_ATTEMPTS_SUFFIX,72,0,24,16,0);
    for(int i=0;i<6;i++){
        if(phase==DOWSE_FOUND && i==s[SEL])continue;
        int y=24;
        if(phase==DOWSE_REVEAL && i==s[SEL])y+=(frame&1)?-2:2;
        raster(ee,out,i==s[EMPTY]?LCD_SEARCHED_GRASS:LCD_GRASS,16*i,y,16,16,0);
    }
    switch(phase){
    case DOWSE_SELECT:
        raster(ee,out,LCD_CURSOR_UP+16*(frame&1),16*s[SEL]+4,40,8,8,0);
        /* fall through */
    case DOWSE_REVEAL:
        lcdMessage(ee,out,LCD_DOWSE_SEARCH,LCD_BOX,0,frame,"Discover an item!");
        break;
    case DOWSE_FOUND:{
        raster(ee,out,LCD_TREASURE,16*s[SEL]+4,24,8,8,0);
        const char *name=lcdItem(walkerRouteItemId(ee,s[CHOICE]));
        unsigned address=s[CHOICE]<10?LCD_ITEM_NAMES+384*s[CHOICE]:0;
        lcdPanel(ee,out,address,32,LCD_TOP|LCD_SIDES,0,frame,name);
        lcdMessage(ee,out,LCD_FOUND,LCD_BOTTOM|LCD_SIDES,1,frame,"Found!");
        break;
    }
    case DOWSE_EMPTY:
        if(s[TRIES]==0)drawRaster(ee,out,LCD_TREASURE,16*s[TARGET]+4,22,8,8,0,1);
        lcdMessage(ee,out,LCD_DOWSE_EMPTY,LCD_BOX,1,frame,"Nothing here!");
        break;
    case DOWSE_HINT:{
        int distance=s[SEL]-s[TARGET];
        int near=distance>-2 && distance<2;
        lcdMessage(ee,out,near?LCD_DOWSE_NEAR:LCD_DOWSE_FAR,LCD_BOX,1,frame,near?"IT'S NEAR!":"IT'S FAR AWAY!");
        break;
    }
    }
}
/* ---- Original screen compositions (coordinates from the firmware reconstruction) ---- */
enum { ART_BALL=0x460,ART_BALL_MASK=0x480,ART_TREASURE=0x488,ART_CURSORS=0x4f8,ART_LEFT=0x5b8,ART_RIGHT=0x5d8,ART_RETURN=0x5f8,
       ART_MENU_LABELS=0x910,ART_TRAINER_ICON=0x1210,ART_TRAINER_NAME=0x1250,ART_COURSE_ICON=0x1390,ART_STEPS=0x13d0,
       ART_TIME=0x1470,ART_DAYS=0x14f0,ART_TOTAL_DAYS=0x1590,ART_VOLUME=0x1690,ART_CONTRAST=0x1730,ART_SOUND_LEVELS=0x17d0,
       ART_CONTRAST_SAMPLE=0x18f0,ART_ITEM_TREASURE=0x1910,ART_ITEM_PRESENT=0x1a90,ART_HIT=0x1e70,ART_CRITICAL=0x1ef0,
       ART_CLOUD=0x1f70,ART_STAR=0x2040,ART_COLON=0x3c0,ART_HYPHEN=0x3e0,ART_WATTS=0x420,ART_HELD_NAME=0x993e,ART_COURSE_NAME=0x907e };
enum { CURSOR_DOWN_ART=1,CURSOR_RIGHT_ART=3 };
static void cursor(const uint8_t *ee,uint8_t *out,int direction,int variant,int x,int y){
    raster(ee,out,ART_CURSORS+(variant+direction*3)*16,x,y,8,8,0);
}
/* A 96x16 band with the name frame the firmware draws behind an 80x16 name. */
static void nameBand(const uint8_t *ee,uint8_t *out,unsigned address,int y,int edges,const char *fallback){
    rect(out,0,y,96,16,0);
    if(y==48)rect(out,0,y+15,96,1,3);else rect(out,0,y,96,1,3);
    rect(out,0,y,1,16,3);rect(out,95,y,1,16,3);
    if(address && hasArtwork(ee,address,320))drawRaster(ee,out,address,0,y,80,16,0,1);
    else label(out,fallback,2,y+4);
    if(edges&LCD_TOP)rect(out,0,y,80,1,3);
    if(edges&LCD_BOTTOM)rect(out,0,y+15,80,1,3);
    if(edges&LCD_LEFT)rect(out,0,y,1,16,3);
    if(edges&LCD_RIGHT)rect(out,79,y,1,16,3);
}
static void enemyName(const uint8_t *ee,uint8_t *out,int index,int y,int edges){
    char name[17];index=index<0||index>2?0:index;
    snprintf(name,sizeof(name),"%.16s",speciesCaps(walkerRoutePokemonId(ee,(unsigned)index)));
    nameBand(ee,out,LCD_ENCOUNTER_NAMES+index*320,y,edges,name);
}
static void heldName(const uint8_t *ee,uint8_t *out,int y,int edges){
    WalkerInfo info;char name[17]="";
    if(walkerRead(ee,&info))snprintf(name,sizeof(name),"%.16s",speciesCaps(info.species));
    nameBand(ee,out,ART_HELD_NAME,y,edges,name);
}
static void numberRight(const uint8_t *ee,uint8_t *out,uint32_t value,int rightX,int y){
    unsigned digits=1;for(uint32_t rest=value;rest>=10;rest/=10)digits++;
    number(ee,out,value,rightX-(int)digits*8,y,(int)digits);
}
static void ballArt(const uint8_t *ee,uint8_t *out,int x,int y,int masked){
    if(!hasArtwork(ee,ART_BALL,16)){
        rect(out,x+1,y,6,8,3);rect(out,x,y+1,8,6,3);rect(out,x+2,y+2,4,4,0);rect(out,x+1,y+3,6,1,3);return;
    }
    if(!masked){drawRaster(ee,out,ART_BALL,x,y,8,8,0,1);return;}
    /* The firmware ANDs this mask into the screen, so its set bits normally mean "keep the background"
     * and the clear bits are the ball. Pick the polarity from the data: a round 8x8 ball covers most of
     * the square, so whichever reading gives the larger area is the silhouette. */
    int set=0;for(int i=0;i<8;i++)for(int j=0;j<8;j++)if(ee[ART_BALL_MASK+i]&(1u<<j))set++;
    int ballIsSet=set>=32;
    for(int i=0;i<8;i++)for(int j=0;j<8;j++){
        unsigned n=ART_BALL+i*2,c=(((ee[n]>>j)&1)<<1)|((ee[n+1]>>j)&1);
        int inside=((ee[ART_BALL_MASK+i]>>j)&1)==(unsigned)ballIsSet;
        if(inside||c)pixel(out,x+i,y+j,c);
    }
}
static void effectArt(const uint8_t *ee,uint8_t *out,unsigned address){
    if(hasArtwork(ee,address,128)){drawRaster(ee,out,address,0x28,0,16,32,0,1);return;}
    rect(out,47,6,3,19,3);rect(out,40,14,16,3,3);
}
static void battleRender(const uint8_t *ee,const int32_t *s,uint8_t *out,uint32_t ms){
    int phase=s[BPHASE];unsigned f=bFrame(s,ms),sprite=(ms/LCD_FRAME_MS)&1,tick=ms/LCD_FRAME_MS;
    int playerX=56,opponentX=8,code=s[BCODE],act=s[BACT],index=s[CHOICE];
    if(phase==B_OPENING)opponentX=-32;
    else if(phase==B_ENCOUNTER)opponentX=bEntrance[f>3?3:f];
    else if(phase==B_PLAYER){playerX=bPlayerTurn[f>8?8:f][0];opponentX=bPlayerTurn[f>8?8:f][1];}
    else if(phase==B_OPPONENT){playerX=bOpponentTurn[f>8?8:f][0];opponentX=bOpponentTurn[f>8?8:f][1];}
    else if(phase==B_LEAVING)opponentX=f<=3?bEntrance[3-f]:-32;
    raster(ee,out,0x91be + sprite*192,playerX,8,32,24,0);
    for(int n=0;n<s[BAT];n++)raster(ee,out,LCD_HP,56+8*n,0,8,8,0);
    if(s[BHPVIS])for(int n=0;n<s[BAT+1];n++)raster(ee,out,LCD_HP,8+8*n,24,8,8,0);
    if(phase<=B_THROW)
        raster(ee,out,0x9a7e + index*384+sprite*192,opponentX,0,32,24,!(ee[WALKER_ROUTE+0x52+index*16+14]&1));
    switch(phase){
    case B_OPENING:
        if(f<3){int h=(3-(int)f)*8;rect(out,0,0,96,h,3);rect(out,0,64-h,96,h,3);}
        break;
    case B_ENCOUNTER:
        if(f>=3 && (int32_t)(ms-(uint32_t)s[DEADLINE])>=0){
            enemyName(ee,out,index,32,LCD_TOP|LCD_LEFT);
            lcdMessage(ee,out,31,LCD_BOTTOM|LCD_SIDES,1,tick,"A wild Pokemon!");
        }
        break;
    case B_CHOOSE:
        if(hasArtwork(ee,LCD_BATTLE_MENU,768))drawRaster(ee,out,LCD_BATTLE_MENU,0,32,96,32,0,1);
        else{label(out,"Attack Catch Evade",2,47);label(out,"<    O    >",8,57);}
        break;
    case B_PLAYER:
        if(code==0){
            if(f==4)effectArt(ee,out,ART_HIT);
            if(f>=4){heldName(ee,out,32,LCD_TOP|LCD_LEFT);lcdMessage(ee,out,MSG_ATTACK,LCD_BOTTOM|LCD_SIDES,0,tick,"Attacked!");}
        }else if(code==1){
            if(f>=4){enemyName(ee,out,index,32,LCD_TOP|LCD_LEFT);lcdMessage(ee,out,MSG_DODGE,LCD_BOTTOM|LCD_SIDES,0,tick,"Dodged!");}
        }else{
            if(f==4)effectArt(ee,out,ART_CRITICAL);
            if(f>=4){
                lcdPanel(ee,out,LCD_MESSAGES+MSG_CRITICAL*384,32,LCD_TOP|LCD_SIDES,0,tick,"Critical hit!");
                lcdMessage(ee,out,MSG_HIT,LCD_BOTTOM|LCD_SIDES,0,tick,"Hit!");
            }
        }
        break;
    case B_OPPONENT:
        if(act==0){
            if(f==4)effectArt(ee,out,ART_HIT);
            if(f>=4){enemyName(ee,out,index,32,LCD_TOP|LCD_LEFT);lcdMessage(ee,out,MSG_ATTACK,LCD_BOTTOM|LCD_SIDES,0,tick,"Attacked!");}
        }else if(f>=4){heldName(ee,out,32,LCD_TOP|LCD_LEFT);lcdMessage(ee,out,MSG_DODGE,LCD_BOTTOM|LCD_SIDES,0,tick,"Dodged!");}
        break;
    case B_DEFEATED:
        enemyName(ee,out,index,32,LCD_TOP|LCD_LEFT);
        lcdMessage(ee,out,MSG_DEFEAT,LCD_BOTTOM|LCD_SIDES,0,tick,"You lost!");
        break;
    case B_WATTS:{
        rect(out,0,32,96,16,0);rect(out,0,32,96,1,3);rect(out,0,32,1,16,3);rect(out,95,32,1,16,3);
        unsigned lost=(unsigned)s[BWATTS],digits=lost>=10?2:1;
        number(ee,out,lost,2+(2-(int)digits)*8,32,(int)digits);
        raster(ee,out,ART_WATTS,2+16+4,32,16,16,0);
        lcdMessage(ee,out,MSG_DROPPED,LCD_BOTTOM|LCD_SIDES,1,tick,"Watts dropped");
        break;}
    case B_STARE:lcdMessage(ee,out,MSG_STARE,LCD_BOX,0,tick,"A stare-down");break;
    case B_LEAVING:
        if(f>3){enemyName(ee,out,index,32,LCD_TOP|LCD_LEFT);lcdMessage(ee,out,MSG_ESCAPE,LCD_BOTTOM|LCD_SIDES,1,tick,"It fled!");}
        break;
    case B_THROW:{
        int step=(int)f<<2;
        static const int rise[7]={0,1,2,4,5,6,8};
        int bx=0x2c-step,by=0x14-rise[f>6?6:f];
        ballArt(ee,out,bx,by,1);
        lcdMessage(ee,out,MSG_THROW,LCD_BOX,0,tick,"Poke Ball!");
        break;}
    case B_CLOSE:case B_BREAK:
        if(hasArtwork(ee,ART_CLOUD,192))drawRaster(ee,out,ART_CLOUD,8,0,32,24,0,1);
        else rect(out,12,4,24,16,3);
        break;
    case B_SETTLE:case B_PAUSE:ballArt(ee,out,0x14,0x0c,0);break;
    case B_CHECK:{static const int xs[4]={0x13,0x14,0x15,0x14};ballArt(ee,out,xs[f&3],0x0c,0);break;}
    case B_STARS:
        ballArt(ee,out,0x14,0x0c,0);
        if(hasArtwork(ee,ART_STAR,16)){
            raster(ee,out,ART_STAR,0x0c-(int)f,0x0a-(int)f*2,8,8,0);raster(ee,out,ART_STAR,(int)f+0x1c,0x0c-(int)f*2,8,8,0);
        }else{pixel(out,0x0c-(int)f+4,0x0a-(int)f*2+4,3);pixel(out,(int)f+0x1c+4,0x0c-(int)f*2+4,3);}
        break;
    case B_CONFIRM:
        ballArt(ee,out,0x14,0x0c,0);
        enemyName(ee,out,index,32,LCD_TOP|LCD_LEFT);
        lcdMessage(ee,out,MSG_CAPTURED,LCD_BOTTOM|LCD_SIDES,1,tick,"Caught!");
        break;
    case B_FAILED:lcdMessage(ee,out,MSG_ALMOST,LCD_BOX,0,tick,"Almost had it!");break;
    default:break;
    }
}
static void inventoryRender(const uint8_t *ee,uint8_t *out,const int32_t *s,uint32_t ms){
    unsigned w,f;invMasks(ee,&w,&f);unsigned frame=(ms/LCD_FRAME_MS)&1;int idx=s[SEL];
    raster(ee,out,ART_MENU_LABELS+4*320,8,0,80,16,0);
    if(s[INVFRIEND]){
        raster(ee,out,ART_LEFT,0,0,8,16,0);
        drawRaster(ee,out,ART_ITEM_PRESENT,0x3c,0x18,32,24,0,1);
        cursor(ee,out,CURSOR_DOWN_ART,(int)frame,(idx%5)*8+0x10,(idx/5)*0x10+0x10);
        for(int n=0;n<10;n++)if(f&(1u<<n))raster(ee,out,ART_TREASURE,(n%5)*8+0x10,n<5?0x18:0x28,8,8,0);
        unsigned id=walkerFriendItemId(ee,(unsigned)idx);char name[17];snprintf(name,sizeof(name),"%.16s",lcdItem(id));
        int found=-1;for(int n=0;n<10;n++)if(walkerRouteItemId(ee,(unsigned)n)==id)found=n;
        lcdPanel(ee,out,found>=0?LCD_ITEM_NAMES+384*found:0,48,LCD_BOX,0,frame,name);
        return;
    }
    if(idx==0){drawRaster(ee,out,0x91be + frame*192,0x3c,0x18,32,24,0,1);heldName(ee,out,48,LCD_TOP|LCD_BOTTOM|LCD_LEFT);}
    else if(idx<=3){
        unsigned id=walkerPokemonId(ee,(unsigned)idx-1);int found=0;
        for(int n=0;n<3;n++)if(walkerRoutePokemonId(ee,(unsigned)n)==id){found=n;break;}
        drawRaster(ee,out,0x9a7e + found*384+frame*192,0x3c,0x18,32,24,0,1);
        enemyName(ee,out,found,48,LCD_TOP|LCD_BOTTOM|LCD_LEFT);
    }else if(idx>=6&&idx<=8){
        unsigned id=walkerItemId(ee,(unsigned)idx-6);char name[17];snprintf(name,sizeof(name),"%.16s",lcdItem(id));
        drawRaster(ee,out,ART_ITEM_TREASURE,0x3c,0x18,32,24,0,1);
        int found=-1;for(int n=0;n<10;n++)if(walkerRouteItemId(ee,(unsigned)n)==id)found=n;
        lcdPanel(ee,out,found>=0?LCD_ITEM_NAMES+384*found:0,48,LCD_BOX,0,frame,name);
    }
    cursor(ee,out,CURSOR_DOWN_ART,(int)frame,(idx%5)*8+0x10-(idx==0?8:0),(idx/5)*0x10+0x10);
    raster(ee,out,ART_RETURN,0,0,8,16,0);
    if(f)raster(ee,out,ART_RIGHT,0x58,0,8,16,0);
    if(w&1)raster(ee,out,ART_BALL,8,0x18,8,8,0);
    for(int n=0;n<3;n++){
        if(w&(2u<<n))raster(ee,out,ART_BALL,n*8+0x18,0x18,8,8,0);
        if(w&(1u<<(6+n)))raster(ee,out,ART_TREASURE,n*8+0x18,0x28,8,8,0);
    }
}
static void digitRaster(const uint8_t *ee,uint8_t *out,unsigned digit,int x,int y){raster(ee,out,0x280+digit*32,x,y,8,16,0);}
static void trainerRender(const uint8_t *ee,uint8_t *out,const int32_t *s,uint32_t ms){
    WalkerInfo info;if(!walkerRead(ee,&info))return;
    if(s[SUB]==1){ /* step cap reached */
        for(int n=0;n<7;n++)digitRaster(ee,out,9,n*8,8);
        raster(ee,out,ART_STEPS,0x38,8,40,16,0);
        lcdMessage(ee,out,67,LCD_BOX,1,0,"Step limit!");
        return;
    }
    int days=s[TRAINDAYS];
    if(days==0){
        raster(ee,out,ART_MENU_LABELS+3*320,8,0,80,16,0);
        raster(ee,out,ART_RETURN,0,0,8,16,0);raster(ee,out,ART_RIGHT,0x58,0,8,16,0);
        raster(ee,out,ART_TRAINER_ICON,0,16,16,16,0);
        if(hasArtwork(ee,ART_TRAINER_NAME,320))raster(ee,out,ART_TRAINER_NAME,16,16,80,16,0);else label(out,caps(info.trainer),18,20);
        raster(ee,out,ART_COURSE_ICON,0,32,16,16,0);
        if(hasArtwork(ee,ART_COURSE_NAME,320))raster(ee,out,ART_COURSE_NAME,16,32,80,16,0);else label(out,caps(info.route),18,36);
        raster(ee,out,ART_TIME,0,48,32,16,0);
        int64_t secs=((int64_t)s[CLOCK_NOW]+s[CLOCK_TZ]+(int64_t)((ms-(uint32_t)s[CLOCK_MS])/1000u))%86400;
        if(secs<0)secs+=86400;
        unsigned h=(unsigned)(secs/3600),m=(unsigned)(secs/60%60),sec=(unsigned)(secs%60);
        digitRaster(ee,out,h/10,0x20,48);digitRaster(ee,out,h%10,0x28,48);raster(ee,out,ART_COLON,0x30,48,8,16,0);
        digitRaster(ee,out,m/10,0x38,48);digitRaster(ee,out,m%10,0x40,48);raster(ee,out,ART_COLON,0x48,48,8,16,0);
        digitRaster(ee,out,sec/10,0x50,48);digitRaster(ee,out,sec%10,0x58,48);
        return;
    }
    const uint8_t *p=ee+0xcef0+(days-1)*4;
    uint32_t daySteps=(uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];
    unsigned totalDays=ee[0x162]<<8|ee[0x163];
    raster(ee,out,ART_LEFT,0,0,8,16,0);
    if(days<7)raster(ee,out,ART_RIGHT,0x58,0,8,16,0);
    raster(ee,out,ART_DAYS,0x28,0,40,16,0);raster(ee,out,ART_HYPHEN,0x18,0,8,16,0);digitRaster(ee,out,(unsigned)days,0x20,0);
    raster(ee,out,ART_TOTAL_DAYS,0,0x20,64,16,0);
    raster(ee,out,ART_STEPS,0x38,0x10,40,16,0);raster(ee,out,ART_STEPS,0x38,0x30,40,16,0);
    numberRight(ee,out,daySteps>9999999u?9999999u:daySteps,0x38,0x10);
    numberRight(ee,out,totalDays,0x60,0x20);
    numberRight(ee,out,info.totalSteps,0x38,0x30);
}
static void settingsRender(const uint8_t *ee,uint8_t *out,const int32_t *s,uint32_t ms){
    unsigned frame=(ms/LCD_FRAME_MS)&1;int page=s[SUB],selection=s[SEL]?1:0;
    raster(ee,out,ART_MENU_LABELS+5*320,8,0,80,16,0);
    raster(ee,out,ART_VOLUME,8,16,40,16,0);raster(ee,out,ART_CONTRAST,56,16,40,16,0);
    if(page==0)cursor(ee,out,CURSOR_RIGHT_ART,(int)frame,selection*48,20);
    else if(page==1){
        cursor(ee,out,CURSOR_RIGHT_ART,2,selection*48,20);
        cursor(ee,out,CURSOR_RIGHT_ART,(int)frame,s[VOLUME]*32,44);
        for(int n=0;n<3;n++)raster(ee,out,ART_SOUND_LEVELS+n*96,8+n*32,40,24,16,0);
    }else{
        cursor(ee,out,CURSOR_DOWN_ART,(int)frame,s[CONTRAST]*8+8,32);
        cursor(ee,out,CURSOR_RIGHT_ART,2,selection*48,20);
        for(int n=0;n<10;n++)raster(ee,out,ART_CONTRAST_SAMPLE,n*8+8,40,8,16,0);
    }
    raster(ee,out,ART_RETURN,0,0,8,16,0);
}
static void discardRender(const uint8_t *ee,uint8_t *out,const int32_t *s,uint32_t ms){
    unsigned frame=(ms/LCD_FRAME_MS)&1;int pokemon=s[REPLACE]==1,slot=s[SEL]<0?0:s[SEL]>2?2:s[SEL];
    if(s[SUB]==1){ /* are you sure you want to leave the new find behind? */
        char name[17];
        if(pokemon){
            raster(ee,out,0x9a7e + s[CHOICE]*384+frame*192,0,0,32,24,0);
            nameBand(ee,out,LCD_ENCOUNTER_NAMES+s[CHOICE]*320,26,LCD_TOP|LCD_BOTTOM|LCD_LEFT,
                     (snprintf(name,sizeof(name),"%.12s",speciesCaps(walkerRoutePokemonId(ee,(unsigned)s[CHOICE]))),name));
        }else{
            drawRaster(ee,out,ART_ITEM_TREASURE,0,0,32,24,0,0);
            snprintf(name,sizeof(name),"%.12s",lcdItem(walkerRouteItemId(ee,(unsigned)s[CHOICE])));
            lcdPanel(ee,out,LCD_ITEM_NAMES+384*s[CHOICE],26,LCD_BOX,0,frame,name);
        }
        label(out,"Leave",38,2);label(out,"New one?",38,13);
        raster(ee,out,ART_LEFT,0,48,8,16,0);label(out,"No",10,52);
        label(out,"Yes",62,52);raster(ee,out,ART_RIGHT,88,48,8,16,0);
        return;
    }
    raster(ee,out,ART_RETURN,0,0,8,16,0);
    if(hasArtwork(ee,0x8b30,320))raster(ee,out,0x8b30,8,0,80,16,0);else label(out,"Replace?",14,4);
    cursor(ee,out,CURSOR_DOWN_ART,(int)frame,slot*0x14+0x18,0x18);
    for(int n=0;n<3;n++)raster(ee,out,pokemon?ART_BALL:ART_TREASURE,n*0x14+0x18,0x20,8,8,0);
    char name[17];
    if(pokemon){
        unsigned id=walkerPokemonId(ee,(unsigned)slot);int found=0;
        for(int n=0;n<3;n++)if(walkerRoutePokemonId(ee,(unsigned)n)==id){found=n;break;}
        enemyName(ee,out,found,48,LCD_TOP|LCD_BOTTOM|LCD_LEFT);
    }else{
        unsigned id=walkerItemId(ee,(unsigned)slot);snprintf(name,sizeof(name),"%.16s",lcdItem(id));
        int found=-1;for(int n=0;n<10;n++)if(walkerRouteItemId(ee,(unsigned)n)==id)found=n;
        lcdPanel(ee,out,found>=0?LCD_ITEM_NAMES+384*found:0,48,LCD_BOX,0,frame,name);
    }
}
static void menuRender(const uint8_t *ee,uint8_t *out,const int32_t *s,uint32_t ms,const WalkerInfo *i){
    static const uint8_t iconY[6]={8,11,13,13,11,8};
    unsigned sel=s[SEL]>=0&&s[SEL]<6?(unsigned)s[SEL]:0,frame=(ms/LCD_FRAME_MS)&1;
    raster(ee,out,ART_MENU_LABELS+sel*320,8,0,80,16,0);
    for(unsigned n=0;n<6;n++){
        raster(ee,out,0x1090+n*64,(int)n*16,16+iconY[n],16,16,0);
        if(n==sel)cursor(ee,out,CURSOR_DOWN_ART,(int)frame,(int)n*16+4,16+iconY[n]-8);
    }
    if(s[SUB]==1)lcdMessage(ee,out,20,LCD_BOX,1,ms/LCD_FRAME_MS,"Not enough watts");
    else if(s[SUB]==2)lcdMessage(ee,out,21,LCD_BOX,1,ms/LCD_FRAME_MS,"No Pokemon");
    else if(s[SUB]==3)lcdMessage(ee,out,MSG_NO_ITEMS,LCD_BOX,1,ms/LCD_FRAME_MS,"No items");
    else{
        numberRight(ee,out,i->watts,0x50,48);raster(ee,out,ART_WATTS,0x50,48,16,16,0);
        if(sel<2){numberRight(ee,out,sel?3:10,0x10,48);raster(ee,out,ART_WATTS,0x18,48,16,16,0);raster(ee,out,0x400,0x28,48,8,16,0);}
    }
    raster(ee,out,ART_LEFT,0,0,8,16,0);raster(ee,out,ART_RIGHT,0x58,0,8,16,0);
}
static void connectRender(const uint8_t *ee,uint8_t *out,const int32_t *s,uint32_t ms){
    unsigned tick=ms/LCD_FRAME_MS;
    /* As on the device: the walker, a blinking signal icon, and the CONNECTING message. */
    if(hasArtwork(ee,0x2450,32)&&(tick&1))raster(ee,out,0x2450,0x2c,0,8,16,0);
    if(s[VIEW]==PEERS||s[SEL]){ /* two walkers when linking with another phone/profile */
        raster(ee,out,0x2350,8,16,32,32,0);raster(ee,out,0x2350,56,16,32,32,1);
    }else raster(ee,out,0x2350,0x20,0x10,32,32,0);
    raster(ee,out,ART_LEFT,0,0,8,16,0);raster(ee,out,ART_RIGHT,0x58,0,8,16,0);
    lcdMessage(ee,out,0,LCD_BOX,0,tick,"Connecting...");
}
void walkerClassicFrame(const uint8_t *ee,const int32_t s[40],uint8_t out[6144],uint32_t ms){
    labelSource=ee;
    memset(out,0,6144);WalkerInfo i;if(!walkerRead(ee,&i))return;
    unsigned frame=(ms/400)&1;
    if(s[VIEW]==HOME){
        raster(ee,out,0x8fbe,0,24,32,24,0);
        if(i.hasPokemon)raster(ee,out,0x933e + frame*768,32,0,64,48,0);
        rect(out,0,47,96,1,3);
        unsigned steps=i.strollSteps>9999999?9999999:i.strollSteps,digits=1;
        for(unsigned rest=steps;rest>=10;rest/=10)digits++;
        number(ee,out,steps,96-(int)digits*8,48,(int)digits);
        uint16_t rows[16];
        /* Icons for what you are carrying, each with how many, in the walker's own digits (hidden when the step count needs the room). */
        int counted=digits<=5;int itemX=counted?28:20;
        if(i.pokemonCount){walkerMenuIcon(1,rows);for(int y=0;y<16;y++)for(int x=0;x<16;x++)if(rows[y]&(1u<<x))pixel(out,x,48+y,3);if(counted)number(ee,out,i.pokemonCount>9?9:i.pokemonCount,16,48,1);}
        if(i.itemCount){walkerMenuIcon(3,rows);for(int y=0;y<16;y++)for(int x=0;x<16;x++)if(rows[y]&(1u<<x))pixel(out,itemX+x,48+y,3);if(counted)number(ee,out,i.itemCount>9?9:i.itemCount,itemX+16,48,1);}
    }else if(s[VIEW]==MENU){
        menuRender(ee,out,s,ms,&i);
    }else if(s[VIEW]==RADAR){
        radarFrame(ee,s,out,ms,ms/LCD_FRAME_MS);
    }else if(s[VIEW]==DOWSE){
        dowsingFrame(ee,s,out,ms/LCD_FRAME_MS);
    }else if(s[VIEW]==BATTLE){
        battleRender(ee,s,out,ms);
    }else if(s[VIEW]==PACK && s[REPLACE]){
        discardRender(ee,out,s,ms);
    }else if(s[VIEW]==PACK){
        inventoryRender(ee,out,s,ms);
    }else if(s[VIEW]==TRAINER){
        trainerRender(ee,out,s,ms);
    }else if(s[VIEW]==SETTINGS){
        settingsRender(ee,out,s,ms);
    }else if(s[VIEW]==CONNECT || s[VIEW]==PEERS){
        connectRender(ee,out,s,ms);
    }else if(s[VIEW]==RESULT){
        static const uint8_t messages[]={30,0,0,30,21,20,22};  /* 3 got away, 4 no Pokemon, 5 watts, 6 no items */
        unsigned id=(unsigned)s[RESULT_ID];
        if(id==1&&s[CHOICE]>=0&&s[CHOICE]<3){
            raster(ee,out,0x9a7e + s[CHOICE]*384+frame*192,32,8,32,24,0);enemyName(ee,out,s[CHOICE],32,LCD_TOP|LCD_LEFT);
            lcdMessage(ee,out,MSG_CAPTURED,LCD_BOTTOM|LCD_SIDES,1,ms/LCD_FRAME_MS,"Caught!");
        }else if(id==2){
            drawRaster(ee,out,ART_ITEM_TREASURE,32,8,32,24,0,0);
            char name[17];snprintf(name,sizeof(name),"%.16s",lcdItem(walkerRouteItemId(ee,(unsigned)s[CHOICE])));
            lcdPanel(ee,out,LCD_ITEM_NAMES+384*(s[CHOICE]<0?0:s[CHOICE]>9?9:s[CHOICE]),32,LCD_TOP|LCD_SIDES,0,ms/LCD_FRAME_MS,name);
            lcdMessage(ee,out,24,LCD_BOTTOM|LCD_SIDES,1,ms/LCD_FRAME_MS,"Found!");
        }else lcdMessage(ee,out,id<7&&messages[id]?messages[id]:30,LCD_BOX,1,ms/LCD_FRAME_MS,"It got away!");
    }
}
unsigned walkerClassicSound(const uint8_t *ee,unsigned score,int16_t *out,unsigned cap){
    if(score>=16||cap>220500)return 0;
    unsigned at=0x8cb0+score*4,start=0x8cf0+(ee[at]|ee[at+1]<<8),size=ee[at+2],sum=0;
    /* The walker can load 96 notes, and checksums only complete note pairs. */
    if(size<2||size>192||start+size>0x8ed0)return 0;
    unsigned bytes=size&~1u;
    for(unsigned j=0;j<bytes;j++)sum+=ee[start+j];
    if((sum&255)!=ee[at+3] || (ee[start+bytes-1]&127)<126)return 0;
    /* Equal-tempered compare values approximate the original piezo timbre.
     * Score order, tempo, durations and pitch changes use the uploaded data. */
    static const uint16_t frequency[]={131,139,147,156,165,175,185,196,208,220,233,247,262,277,294,311,330,349,370,392,415,440,466,494,523,554,587,622,659,698,740,784,831,880,932,988,1047,1109,1175,1245,1319,1397,1480,1568,1661,1760,1865,1976,2093,2217,2349,2489,2637,2794,2960,3136,3322,3520,3729,3951};
    unsigned length=0,tempo=120,loops=0,phase=0;
    for(unsigned n=0;n+1<bytes && length<cap && loops<128;n+=2,loops++){
        unsigned duration=ee[start+n],pitch=ee[start+n+1]&127;
        if(pitch==127)break;
        if(pitch==126){n=(unsigned)-2;continue;}
        if(pitch==123){if(!duration)return 0;tempo=duration;continue;}
        if(pitch>=60 && pitch!=125)return 0;
        unsigned samples=(unsigned)((uint64_t)22050*duration*60/(tempo*24));
        if(samples>cap-length)samples=cap-length;
        if(!out){length+=samples;continue;}
        unsigned silence=(ee[start+n+1]&128)?0:216;if(silence>samples)silence=samples;
        for(unsigned k=0;k<samples;k++){
            if(pitch!=125)phase=(phase+frequency[pitch])%22050;
            out[length++]=(pitch==125||k+silence>=samples)?0:phase<11025?4200:-4200;
        }
    }
    return length;
}

void walkerMenuIcon(unsigned kind,uint16_t rows[16]){
    if(kind>=sizeof(walkerMenuIcons)/sizeof(walkerMenuIcons[0]))kind=0;
    memcpy(rows,walkerMenuIcons[kind],16*sizeof(*rows));
}
static const unsigned peerOffset[]={0xed,0x156,0x272,0x10,0x8f00,0x91be,0x993e,0xcc00};
static const unsigned peerLength[]={104,24,10,96,190,384,320,548};
static void peerSeal(uint8_t *ee,unsigned a,unsigned b,unsigned bytes){
    unsigned sum=1;for(unsigned n=0;n<bytes;n++)sum+=ee[a+n];
    ee[a+bytes]=(uint8_t)sum;memcpy(ee+b,ee+a,bytes+1);
}
int walkerPeerExport(const uint8_t *ee,uint8_t out[1688]){
    WalkerInfo info;if(!walkerRead(ee,&info)||!info.hasPokemon||!info.registered)return 0;
    memcpy(out,"PWPEER15",8);unsigned at=12;
    for(unsigned n=0;n<8;n++){memcpy(out+at,ee+peerOffset[n],peerLength[n]);at+=peerLength[n];}
    out[12+14]=out[12+15]=0; /* Secret ID is not needed for a phone meeting. */
    uint32_t crc=walkerCrc(out+12,1676);for(int n=0;n<4;n++)out[8+n]=crc>>(n*8);
    return 1;
}
int walkerPeerImport(const uint8_t *packet,unsigned bytes,uint8_t out[65536]){
    if(bytes!=1688||memcmp(packet,"PWPEER15",8))return 0;
    uint32_t crc=(uint32_t)packet[8]|(uint32_t)packet[9]<<8|(uint32_t)packet[10]<<16|(uint32_t)packet[11]<<24;
    if(walkerCrc(packet+12,1676)!=crc)return 0;
    memset(out,0,65536);memcpy(out,"nintendo",8);unsigned at=12;
    for(unsigned n=0;n<8;n++){memcpy(out+peerOffset[n],packet+at,peerLength[n]);at+=peerLength[n];}
    peerSeal(out,0xed,0x1ed,104);peerSeal(out,0x156,0x256,24);
    WalkerInfo info;if(!walkerRead(out,&info)||!info.hasPokemon||!info.registered||!walkerCanWalk(out))return 0;
    for(unsigned n=0;n<4;n++)if(walkerPartnerMove(out,n)>467)return 0;
    return 1;
}
unsigned walkerPartnerMove(const uint8_t *ee,unsigned slot){
    if(slot>=4)return 0;unsigned at=WALKER_ROUTE+4+slot*2;
    return ee[at]|ee[at+1]<<8;
}
const char *walkerMoveName(unsigned id){return id<468?walkerMoves[id].name:"Unknown move";}
void walkerFriendlyStart(const uint8_t *a,const uint8_t *b,int32_t s[24],uint32_t seed){
    memset(s,0,24*sizeof(*s));s[0]=s[1]=100;s[11]=(int32_t)(seed?seed:0x51504231);
    for(unsigned side=0;side<2;side++)for(unsigned n=0;n<4;n++){
        unsigned move=walkerPartnerMove(side?b:a,n);s[2+side*4+n]=move<468?walkerMoves[move].pp:0;
    }
    s[13]=s[14]=-1;
}
static uint32_t friendlyRandom(int32_t *s){uint32_t value=(uint32_t)s[11];value^=value<<13;value^=value>>17;value^=value<<5;s[11]=(int32_t)value;return value;}
static unsigned selectedMove(const uint8_t *ee,const int32_t *s,unsigned side,unsigned choice){
    int available=0;for(unsigned n=0;n<4;n++)if(s[2+side*4+n]>0)available=1;
    if(!available)return 165; /* Struggle when all retained moves are exhausted. */
    unsigned move=walkerPartnerMove(ee,choice);
    return choice<4&&s[2+side*4+choice]>0&&move&&move<468?move:0;
}
int walkerFriendlyTurn(const uint8_t *a,const uint8_t *b,int32_t s[24],unsigned choiceA,unsigned choiceB){
    if(s[12]||choiceA>3||choiceB>3||s[0]<1||s[0]>100||s[1]<1||s[1]>100)return 0;
    unsigned move[2]={selectedMove(a,s,0,choiceA),selectedMove(b,s,1,choiceB)};if(!move[0]||!move[1])return 0;
    unsigned first;
    if(walkerMoves[move[0]].priority!=walkerMoves[move[1]].priority)first=walkerMoves[move[0]].priority>walkerMoves[move[1]].priority?0:1;
    else if(a[WALKER_ROUTE+12]!=b[WALKER_ROUTE+12])first=a[WALKER_ROUTE+12]>b[WALKER_ROUTE+12]?0:1;
    else first=friendlyRandom(s)&1;
    s[13]=choiceA;s[14]=choiceB;s[15]=s[16]=0;
    for(unsigned n=0;n<2;n++){
        unsigned side=first^n,other=side^1,id=move[side],choice=side?choiceB:choiceA;const WalkerMoveFact *fact=walkerMoves+id;
        if(s[2+side*4+choice]>0)s[2+side*4+choice]--;
        if(fact->accuracy && friendlyRandom(s)%100>=fact->accuracy)continue;
        if(fact->category==2){s[18+side]=1;continue;} /* Simplified brace effect. */
        unsigned damage=4+fact->power/8+(side?b:a)[WALKER_ROUTE+12]/10;
        if((friendlyRandom(s)&15)==0)damage+=damage/2;
        if(s[18+other]){damage=(damage+1)/2;s[18+other]=0;}
        if(damage>(unsigned)s[other])damage=(unsigned)s[other];s[other]-=damage;s[15+side]=damage;s[17]=side;
        if(!s[other]){s[12]=side+1;break;}
    }
    if(++s[10]>=30 && !s[12])s[12]=s[0]==s[1]?3:s[0]>s[1]?1:2;
    return 1;
}
void walkerFriendlyFrame(const uint8_t *a,const uint8_t *b,const int32_t s[24],unsigned side,unsigned selected,uint8_t out[6144],uint32_t ms){
    memset(out,0,6144);if(side>1||selected>3)return;
    labelSource=side?b:a;
    const uint8_t *own=side?b:a,*peer=side?a:b;unsigned frame=(ms/400)&1,other=side^1;
    char text[16];snprintf(text,sizeof(text),"%.6s",speciesCaps(own[WALKER_ROUTE]|own[WALKER_ROUTE+1]<<8));label(out,text,54,0);
    snprintf(text,sizeof(text),"%.6s",speciesCaps(peer[WALKER_ROUTE]|peer[WALKER_ROUTE+1]<<8));label(out,text,0,0);
    rect(out,1,9,39,4,1);rect(out,56,9,39,4,1);rect(out,1,9,(s[other]*39+99)/100,4,3);rect(out,56,9,(s[side]*39+99)/100,4,3);
    int motion=(int)((ms/100)%3)-1;
    raster(peer,out,0x91be + frame*192,4+motion,17,32,24,!(peer[WALKER_ROUTE+14]&1));
    raster(own,out,0x91be + frame*192,60-motion,17,32,24,0);
    unsigned move=walkerPartnerMove(own,selected);snprintf(text,sizeof(text),"%u.%.10s",selected+1,walkerMoveName(move));label(out,text,0,45);
    snprintf(text,sizeof(text),"PP %ld",(long)s[2+side*4+selected]);label(out,text,0,55);
    snprintf(text,sizeof(text),"Turn %ld",(long)s[10]+1);label(out,text,44,55);
}
int walkerChooseBattleItem(uint8_t *ee,unsigned *out,int expanded){
    WalkerInfo info;if(!walkerRead(ee,&info)||!info.hasPokemon)return 0;
    if(expanded && walkerExpandedReward(ee,out,1))return 1;
    /* Course slots are ordered from rarer finds to common finds. Use the
     * original table only, with 70% weight on its first three entries. Invalid
     * entries are omitted and the remaining weights are normalized. */
    static const unsigned weight[]={30,25,15,12,8,5,2,1,1,1};
    unsigned total=0;
    for(unsigned n=0;n<10;n++){unsigned id=walkerRouteItemId(ee,n);if(id>0&&id<=536)total+=weight[n];}
    if(!total)return 0;
    unsigned roll=(walkerRandom(ee)>>3)%total;
    for(unsigned n=0;n<10;n++){
        unsigned id=walkerRouteItemId(ee,n);if(!id||id>536)continue;
        if(roll<weight[n]){*out=n;return 1;}roll-=weight[n];
    }
    return 0;
}
