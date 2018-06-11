// Hyperbolic Rogue for Android
// Copyright (C) 2012-2018 Zeno Rogue

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#define ISANDROID 1
#define GL_ES
#define CAP_ACHIEVE 1
#define CAP_SHADER 1
#define CAP_VERTEXBUFFER 0

#define HNEW 1

#define MOBPAR_FORMAL JNIEnv *env, jobject thiz
#define MOBPAR_ACTUAL env, thiz

#include <android/log.h>
#include <stdio.h>

char android_log_buf[1000000];
int android_log_bufpos = 0;
#define XPRINTF
#pragma clang diagnostic ignored "-Wformat-security"
template<class...T>
// __attribute__((__format__ (__printf__, 1, 2)))
void Xprintf(const char *fmt, T... t) { 
  snprintf(android_log_buf+android_log_bufpos, 1000000-android_log_bufpos, fmt, t...); 
  int last_cr = 0;
  int i;
  for(i=0; android_log_buf[i]; i++) if(android_log_buf[i] == 10) {
    android_log_buf[i] = 0;
    __android_log_print(ANDROID_LOG_VERBOSE, "RUG", "%s", android_log_buf+last_cr); 
    last_cr = i+1;
    }
  android_log_bufpos = 0;
  while(i > last_cr) android_log_buf[android_log_bufpos++] = android_log_buf[last_cr++];
  }

#include <jni.h>
#include <string>

namespace hr {
void gdpush(int t);

void shareScore(MOBPAR_FORMAL);

const char *scorefile, *conffile;
std::string levelfile, picfile;

bool settingsChanged = false;

struct transmatrix getOrientation();
}

#include "/home/eryx/proj/rogue/hyper/init.cpp"

using namespace hr;

// #define delref env->DeleteLocalRef(_thiz)

int semaphore = 0;
bool crash = false;

#define LOCK(s, x) \
  semaphore++; const char *xs = x; if(semaphore > 1) { crash = true; \
    __android_log_print(ANDROID_LOG_WARN, "HyperRogue", "concurrency crash in %s\n", x); semaphore--; fflush(stdout); return s; }
#define UNLOCK semaphore--; if(crash) { crash = false; __android_log_print(ANDROID_LOG_WARN, "HyperRogue", "concurrency crashed with %s\n", xs); fflush(stdout); }


extern "C" jintArray
Java_com_roguetemple_hyperroid_HyperRogue_loadMap
  ( MOBPAR_FORMAL)
{
//  if(debfile) fprintf(debfile, "loadmap started.\n"), fflush(debfile);
    LOCK(NULL, "loadMap")

    jintArray result;
    result = env->NewIntArray(size(graphdata));
    if(result == NULL) return NULL;
    
    env->SetIntArrayRegion(result, 0, size(graphdata), &*graphdata.begin());
    // delref;
//  env->DeleteLocalRef(result);
//  if(debfile) fprintf(debfile, "loadmap finished.\n"), fflush(debfile);

    UNLOCK
    return result;
}

extern "C" bool
Java_com_roguetemple_hyperroid_HyperRogue_captureBack
  ( MOBPAR_FORMAL) {
    if(cmode & sm::NORMAL) return false;
    popScreenAll();
    return true;
  }

void uploadAll(MOBPAR_FORMAL);

extern "C" bool
Java_com_roguetemple_hyperroid_HyperRogue_keepinmemory
  ( MOBPAR_FORMAL) {
  saveStats(true);
  uploadAll(MOBPAR_ACTUAL);
  if(!canmove) return false;
  if(items[itOrbSafety]) return false;
  return gold() >= 10 || tkills() >= 50;
  }

extern "C" int
Java_com_roguetemple_hyperroid_HyperRogue_getland
  ( MOBPAR_FORMAL)
{
    return getCurrentLandForMusic();
    }

extern "C" int
Java_com_roguetemple_hyperroid_HyperRogue_getLanguage
  ( MOBPAR_FORMAL)
{
    return vid.language;
    }

extern "C" int
Java_com_roguetemple_hyperroid_HyperRogue_getMusicVolume
  ( MOBPAR_FORMAL)
{
    return musicvolume;
    }

extern "C" int
Java_com_roguetemple_hyperroid_HyperRogue_getEffVolume
  ( MOBPAR_FORMAL)
{
    return effvolume;
    }

extern "C" int
Java_com_roguetemple_hyperroid_HyperRogue_getImmersive(MOBPAR_FORMAL)
{
    return vid.full;
    }

extern "C" int
Java_com_roguetemple_hyperroid_HyperRogue_getGL(MOBPAR_FORMAL)
{
    return vid.usingGL;
    }

extern "C" void
Java_com_roguetemple_hyperroid_HyperRogue_glhrinit(MOBPAR_FORMAL)
{
    __android_log_print(ANDROID_LOG_WARN, "HyperRogue", "glhr::init %d\n", 0);
    #if HNEW
    glhr::init();
    #endif
    __android_log_print(ANDROID_LOG_WARN, "HyperRogue", "glhr::init done %d\n", 0);
    }

extern "C" int
Java_com_roguetemple_hyperroid_HyperRogue_getaPosition(MOBPAR_FORMAL)
{
  return glhr::aPosition;
  }

extern "C" int
Java_com_roguetemple_hyperroid_HyperRogue_getaTexture(MOBPAR_FORMAL)
{
  return glhr::aTexture;
  }

extern "C" int
Java_com_roguetemple_hyperroid_HyperRogue_getuColor(MOBPAR_FORMAL)
{
  return glhr::current->uColor;
  }

string sscorefile, sconffile, scachefile;

#include <sys/stat.h>

extern "C" void
Java_com_roguetemple_hyperroid_HyperRogue_setFilesDir(MOBPAR_FORMAL, jstring dir)
{
  const char *nativeString = env->GetStringUTFChars(dir, 0);
  sscorefile = nativeString; sscorefile += "/hyperrogue.log";
  sconffile = nativeString; sconffile += "/hyperrogue.ini";
  scachefile = nativeString; scachefile += "/scorecache.txt";
  levelfile = nativeString; levelfile += "/hyperrogue.lev"; 
  picfile = nativeString; picfile += "/hyperrogue.pic"; 
  scorefile = sscorefile.c_str();
  conffile = sconffile.c_str();
  chmod(scorefile, 0777);
  chmod(conffile, 0777);
  chmod(nativeString, 0777);
  env->ReleaseStringUTFChars(dir, nativeString);
  }

bool gamerunning;

extern "C" 
jint
Java_com_roguetemple_hyperroid_HyperRogue_initGame(MOBPAR_FORMAL) {
                                                  
//  debfile = fopen("/sdcard/hyperdebug.txt", "wt");
//  if(!debfile) debfile = fopen("/storage/simulated/0/hyperdebug.txt", "wt");
  
//  if(debfile) fprintf(debfile, "initgame started.\n"), fflush(debfile);
  

  __android_log_print(ANDROID_LOG_VERBOSE, "HyperRogue", "Initializing game, gamerunning = %d\n", gamerunning);
  printf("test\n");
  fflush(stdout);
  if(gamerunning) return 1;
  gamerunning = true;
  initAll();
  if(showstartmenu) pushScreen(showStartMenu);
  uploadAll(MOBPAR_ACTUAL);
  __android_log_print(ANDROID_LOG_VERBOSE, "HyperRogue", "Game initialized, gamerunning = %d\n", gamerunning);
  return 0;
  }

JNIEnv *tw_env; jobject tw_thiz;

int textwidth(int siz, const string &str) {
  jclass cls = tw_env->GetObjectClass(tw_thiz);
  jmethodID mid = tw_env->GetMethodID(cls, "getTextWidth", "(Ljava/lang/String;I)I");
  jobject jstr = tw_env->NewStringUTF(str.c_str());
  int res = tw_env->CallIntMethod(tw_thiz, mid, jstr, siz);
  tw_env->DeleteLocalRef(jstr);
  tw_env->DeleteLocalRef(cls);
  return res;
  }

bool achievementsConnected = false;
string doViewLeaderboard;
bool doViewAchievements;
bool doOpenURL;

bool currentlyConnecting() { return false; }
bool currentlyConnected() { return false; }

void viewLeaderboard(string what) { doViewLeaderboard = what; }
void viewAchievements() { doViewAchievements = true; }

vector<pair<int, int> > scoresToUpload;
vector<const char *> achievementsToUpload;
vector<pair<string, int> > soundsToPlay;

void openURL() {
  doOpenURL = true;
  }
  
int last_upload[NUMLEADER];

void shareScore(MOBPAR_FORMAL) {
  string s = buildScoreDescription();
  jclass cls = env->GetObjectClass(thiz);
  jmethodID mid = env->GetMethodID(cls, "shareScore", "(Ljava/lang/String;)V");
  jobject str = env->NewStringUTF(s.c_str());
  env->CallVoidMethod(thiz, mid, str);
  env->DeleteLocalRef(str);
  env->DeleteLocalRef(cls);
  }

int nticks; int getticks() { return nticks; }

extern "C" void Java_com_roguetemple_hyperroid_HyperRogue_draw(MOBPAR_FORMAL) {
//  if(debfile) fprintf(debfile, "draw started.\n"), fflush(debfile);

  LOCK(, "draw")
  /* static int infoticks;
  if(getticks() - infoticks > 10000 && !timerghost)  {
    addMessage("ticks: " + its(getticks()));
    infoticks = getticks();
    } */
  tw_thiz = thiz; tw_env = env;

  #if HNEW
  glhr::be_textured();
  glhr::be_nontextured();

  #if CAP_SHADER
  glEnableVertexAttribArray(glhr::aPosition);
  #else
  glEnableClientState(GL_VERTEX_ARRAY);
  #endif 
  #endif

  mobile_draw(MOBPAR_ACTUAL);
  uploadAll(MOBPAR_ACTUAL);

  #if HNEW
  // text is drawn with 'textured'  
  glhr::be_textured();
  glhr::set_depthtest(false);
  stereo::set_viewport(0);
  stereo::set_mask(0);
  glhr::new_projection();
  glhr::projection_multiply(glhr::translate(-1,-1,0));
  glhr::projection_multiply(glhr::ortho(vid.xres/2, vid.yres/2, 1));
  glhr::set_modelview(glhr::id);
    glhr::color2(0xC08040F0);
  #endif

  UNLOCK
  }

extern "C" void Java_com_roguetemple_hyperroid_HyperRogue_drawScreenshot(MOBPAR_FORMAL) {
  dynamicval<bool> d(vid.usingGL, false);
  Java_com_roguetemple_hyperroid_HyperRogue_draw(MOBPAR_ACTUAL);
  }

extern "C" void Java_com_roguetemple_hyperroid_HyperRogue_handleKey(MOBPAR_FORMAL, jint keycode) {
  flashMessages(); mousing = false;
  handlekey(keycode, keycode);
  }

extern "C" void Java_com_roguetemple_hyperroid_HyperRogue_update
  ( MOBPAR_FORMAL, 
    jint xres, jint yres, jint _ticks,
    jint _mousex, jint _mousey, jboolean _clicked) {

  LOCK(, "update")

//  if(debfile) fprintf(debfile, "update started.\n"), fflush(debfile);
  
  if(xres != vid.xres || yres != vid.yres)
    vid.killreduction = 0;
    
  vid.xres = xres;
  vid.yres = yres;
  vid.fsize = (min(vid.xres, vid.yres) * fontscale + 50) / 3200;

  mousex = _mousex;
  mousey = _mousey;
  clicked = _clicked;
  nticks = _ticks;
  uploadAll(MOBPAR_ACTUAL);
  UNLOCK
  // delref;
//  if(debfile) fprintf(debfile, "update stopped.\n"), fflush(debfile);
  }
    
void playSound(cell *c, const string& fname, int vol) {
  soundsToPlay.push_back(make_pair(fname, vol));
  }

transmatrix orientation;
bool orientation_requested;

transmatrix getOrientation() {
  orientation_requested = true;
  return orientation;
  }

void uploadAll(JNIEnv *env, jobject thiz) {

  jclass cls = env->GetObjectClass(thiz);
  
  if(orientation_requested) {
    jmethodID mid = env->GetMethodID(cls, "getOrientation", "(II)D");
    for(int i=0; i<3; i++)
    for(int j=0; j<3; j++)
      orientation[i][j] = env->CallDoubleMethod(thiz, mid, i, j);
    orientation_requested = false;
    }
  
  for(int i=0; i<size(soundsToPlay); i++) {
    jmethodID mid = env->GetMethodID(cls, "playSound", "(Ljava/lang/String;I)V");
    jobject str = env->NewStringUTF(soundsToPlay[i].first.c_str());
    env->CallVoidMethod(thiz, mid, str, soundsToPlay[i].second);
    env->DeleteLocalRef(str);
    }
  soundsToPlay.clear();

  if(settingsChanged) {
    jmethodID mid = env->GetMethodID(cls, "applyUserSettings", "()V");
    env->CallVoidMethod(thiz, mid);
    settingsChanged = false;
    }

  if(doOpenURL) {
    jmethodID mid = env->GetMethodID(cls, "openWebsite", "()V");
    env->CallVoidMethod(thiz, mid);
    doOpenURL = false;
    }
    
  env->DeleteLocalRef(cls);
  }

void achievement_init() {}
void achievement_close() {}
void achievement_gain(const char* s, char flags) {}

#include <unistd.h>

