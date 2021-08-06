package com.roguetemple.hyperroid;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Locale;
import java.util.StringTokenizer;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLDisplay;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.AssetFileDescriptor;
import android.content.res.Configuration;
import android.content.res.Resources; 
import android.graphics.Bitmap;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.Typeface;
import android.hardware.Sensor;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.hardware.SensorEvent;
import android.media.MediaPlayer;
import android.net.Uri;
import android.opengl.GLSurfaceView;
import android.opengl.GLSurfaceView.EGLConfigChooser;
import android.os.Bundle;
import android.os.Looper;
import android.preference.PreferenceManager;
import android.text.ClipboardManager;
import android.util.DisplayMetrics;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.Window;
import android.widget.FrameLayout;
import android.widget.FrameLayout.LayoutParams;
import android.widget.Toast;
import android.util.Log;

import java.io.InputStream;

public class HyperRogue extends Activity implements SensorEventListener {

    private static final int RESULT_SETTINGS = 1;
    private static final boolean isGold = false;

    final int SAVESIZE = 261;
    private static final int YENDOR_SIZE = 30;
    static final int NUMLEADER = 57;

     int yendor[];
    int scoretable[];
    int savedgame[];

    HyperView hv;
    GLSurfaceView glview;
    FrameLayout fl;

    public void showHelp(String s) {
      final HyperRogue game = this;
      final String helptext = s;

      if(Looper.myLooper() != Looper.getMainLooper()) {
        runOnUiThread(new Runnable() { public void run() { game.showHelp(helptext); }});
        return;
        }

      AlertDialog.Builder builder = new AlertDialog.Builder(this);
      AlertDialog ad = builder.create();
      ad.setButton("OK", new DialogInterface.OnClickListener() {
          public void onClick(DialogInterface dialog, int which) {
                  dialog.dismiss();
          }
        });
      /* builder.setNeutralButton("Share", new DialogInterface.OnClickListener() {
          public void onClick(DialogInterface dialog, int which) {
            // (HyperRogue)(getApplicationContext())
            game.shareScore(helptext);
              dialog.dismiss();
          }
        }); */
      ad.setMessage(s);
      ad.setOwnerActivity(this);
      ad.show();
      // help = s;
      // showDialog(1);
      }
      
    String sharescore;

    public void shareScore(String s) {
      final HyperRogue game = this;
      sharescore = s;

        String[] items = {
                getResources().getString(R.string.sharesam),
                getResources().getString(R.string.sharemsg),
                getResources().getString(R.string.copymsg)
        };

        if(Looper.myLooper() != Looper.getMainLooper()) {
        runOnUiThread(
                         new Runnable() { 
                                 public void run() { 
                                         game.shareScore(sharescore); 
                                         }
                                 }
                         );
        return;
        }

      AlertDialog.Builder builder = new AlertDialog.Builder(this);
      builder.setTitle(getResources().getText(R.string.sharehow));
      builder.setItems(items, new DialogInterface.OnClickListener() {

        public void onClick(DialogInterface dialog, int item) {
          if(item == 2) {
            ClipboardManager man = (ClipboardManager)getSystemService(CLIPBOARD_SERVICE);
            if(man != null) man.setText(sharescore);
            Toast.makeText(getApplicationContext(), getResources().getText(R.string.copied), Toast.LENGTH_SHORT).show();
            }
          else {
            Intent intent=new Intent(android.content.Intent.ACTION_SEND);
            intent.setType("text/plain");
            intent.putExtra(Intent.EXTRA_SUBJECT, "HyperRogue");
            intent.putExtra(Intent.EXTRA_TEXT, sharescore);
            intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_WHEN_TASK_RESET);
            if(item == 0) {
              try {
                String path = getCacheDir().toString() + "/cache.jpg";
                FileOutputStream fout = new FileOutputStream(path);
                Bitmap bitmap = hv.screenshot();
                bitmap.compress(Bitmap.CompressFormat.JPEG, 90, fout);
                fout.flush();
                fout.close();
                intent.setType("image/jpeg");
                intent.putExtra(Intent.EXTRA_STREAM, Uri.parse(HyperProvider.CONTENT_URI + "hyperrogue.jpg"));
                } catch (Exception e) {
                    Toast.makeText(getApplicationContext(), "Error with the image file", Toast.LENGTH_SHORT).show();
                }
              }
            startActivity(Intent.createChooser(intent, getResources().getText(R.string.sharevia)));
            }
          }
        });
      AlertDialog ad = builder.create();
      ad.setOwnerActivity(this);
      ad.show();
      }

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
      super.onCreate(savedInstanceState);


        File f = getFilesDir();

        setFilesDir(f.getAbsolutePath());
        initGame();

        immersive = getImmersive();
        if(immersive) requestWindowFeature(Window.FEATURE_NO_TITLE);
        setContentView(R.layout.main);

        forceCanvas = false;

      hv = (HyperView) findViewById(R.id.ascv);
      hv.game = this;
      hv.invalidate();
      
      fl = (FrameLayout) findViewById(R.id.framelayout);

      applyUserSettings();

      UiChangeListener();

      mSensorManager = (SensorManager) getSystemService(Context.SENSOR_SERVICE);
    }
    
    public native int initGame();
    public native int getland();
    public native void update(int xres, int yres, int ticks, int mousex, int mousey, boolean clicked);
    public native void draw();
    public native void drawScreenshot();
    public native int[] loadMap();
    public native boolean captureBack();
    public native boolean keepinmemory();
    public native boolean getImmersive();
    public native boolean getGL();
    public native int getMusicVolume();
    public native int getEffVolume();
    public native int getLanguage();
    public native void syncScore(int id, int val);
    public native void setFilesDir(String s);
    public native boolean getGoogleConnection();
    public native void setGoogleSignin(boolean issigned, boolean isconnecting);
    public native void handleKey(int keyCode);
    public native void glhrinit();
    public native int getaPosition();
    public native int getaTexture();
    public native int getuColor();

     static {
      System.loadLibrary("hyper");
      }    

     @Override
     public boolean onKeyDown(int keyCode, KeyEvent event)  {
         if (keyCode == KeyEvent.KEYCODE_BACK && event.getRepeatCount() == 0) {
             synchronized(this) { if(captureBack()) return true; }
         }

         int unicode = 0;
         if(keyCode == KeyEvent.KEYCODE_F1) unicode = (123001);
         else if(keyCode == KeyEvent.KEYCODE_F2) unicode = (123002);
         else if(keyCode == KeyEvent.KEYCODE_F3) unicode = (123003);
         else if(keyCode == KeyEvent.KEYCODE_F4) unicode = (123004);
         else if(keyCode == KeyEvent.KEYCODE_F5) unicode = (123005);
         else if(keyCode == KeyEvent.KEYCODE_F6) unicode = (123006);
         else if(keyCode == KeyEvent.KEYCODE_F7) unicode = (123007);
         else if(keyCode == KeyEvent.KEYCODE_F10) unicode = (123010);
         else if(keyCode == KeyEvent.KEYCODE_ESCAPE) unicode = (123099);
         else if(keyCode == KeyEvent.KEYCODE_F12) unicode = (123012);
         else if(keyCode == KeyEvent.KEYCODE_HOME) unicode = (123013);
         else if(keyCode == KeyEvent.KEYCODE_DPAD_LEFT) unicode = (123014);
         else if(keyCode == KeyEvent.KEYCODE_DPAD_RIGHT) unicode = (123015);
         else if(keyCode == KeyEvent.KEYCODE_DPAD_UP) unicode = (123021);
         else if(keyCode == KeyEvent.KEYCODE_DPAD_DOWN) unicode = (123022);
         else if(keyCode == KeyEvent.KEYCODE_PAGE_UP) unicode = (123023);
         else if(keyCode == KeyEvent.KEYCODE_PAGE_DOWN) unicode = (123024);
         else if(keyCode == KeyEvent.KEYCODE_ENTER) unicode = (123025);
         else if(keyCode == KeyEvent.KEYCODE_NUMPAD_1) unicode = (123031);
         else if(keyCode == KeyEvent.KEYCODE_NUMPAD_2) unicode = (123032);
         else if(keyCode == KeyEvent.KEYCODE_NUMPAD_3) unicode = (123033);
         else if(keyCode == KeyEvent.KEYCODE_NUMPAD_4) unicode = (123034);
         else if(keyCode == KeyEvent.KEYCODE_NUMPAD_5) unicode = (123035);
         else if(keyCode == KeyEvent.KEYCODE_NUMPAD_6) unicode = (123036);
         else if(keyCode == KeyEvent.KEYCODE_NUMPAD_7) unicode = (123037);
         else if(keyCode == KeyEvent.KEYCODE_NUMPAD_8) unicode = (123038);
         else if(keyCode == KeyEvent.KEYCODE_NUMPAD_9) unicode = (123039);
         else if(keyCode == KeyEvent.KEYCODE_NUMPAD_DOT) unicode = (123051);
         else if(keyCode == KeyEvent.KEYCODE_DEL) unicode = (123052);
         else unicode = event.getUnicodeChar();

         if(unicode != 0) synchronized(this) { handleKey(unicode); }
         return super.onKeyDown(keyCode, event);
     }

     void openWebsite() {
      Runnable r = new Runnable() {
        public void run() {
          String url = "http://roguetemple.com/z/hyper/";
          Intent i = new Intent(Intent.ACTION_VIEW);
          i.setData(Uri.parse(url));
          startActivity(i);
          }
        };
      runOnUiThread(r);      
      }
   
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
    	super.onActivityResult(requestCode, resultCode, data);
    	if(requestCode == RESULT_SETTINGS)
    		applyUserSettings();
        }

    public boolean forceCanvas;
    int musicvolume, effvolume;

    // returns 'true' iff the activity is refreshed
    
    public boolean setLocale(String lang) { 
      Locale myLocale = new Locale(lang);
      Resources res = getResources(); 
      DisplayMetrics dm = res.getDisplayMetrics(); 
      Configuration conf = res.getConfiguration();
      String oldlang = conf.locale.getLanguage();
      conf.locale = myLocale; 
      res.updateConfiguration(conf, dm); 
      if(lang != oldlang) {
//        Intent refresh = new Intent(this, HyperRogue.class);
//        startActivity(refresh);
//        return true;
        return false;
        }
      return false;
      } 

    boolean immersive;

    void applyUserSettings() {

        if (Looper.myLooper() != Looper.getMainLooper()) {
            runOnUiThread(new Runnable() {
                public void run() {
                    applyUserSettings();
                }
            });
            return;
        }

        int lang = getLanguage();
        boolean usegl = getGL();
        musicvolume = getMusicVolume();
        changevol = true;
        effvolume = getEffVolume();
        immersive = getImmersive();

        boolean reset = false;
        if(lang == 0) reset = setLocale("en");
        if(lang == 1) reset = setLocale("pl");
        if(lang == 2) reset = setLocale("tr");
        if(lang == 3) reset = setLocale("cz");
        if(lang == 4) reset = setLocale("ru");

        if(reset) return; // no point in doing the following twice

        if(glview != null && !usegl) {
          fl.removeView(glview);
          glview = null;
          hv.invalidate();
          }
        
        if(usegl && glview == null) {      
          glview = new GLSurfaceView(this);
          glview.setEGLContextClientVersion(2);

          HyperRenderer hr =new HyperRenderer();
          hr.game = this;
          glview.setEGLConfigChooser(new HRConfigChooser());
          glview.setRenderer(hr);
          glview.setZOrderMediaOverlay(true);
          LayoutParams lp =new LayoutParams(LayoutParams.FILL_PARENT, LayoutParams.FILL_PARENT);
          fl.addView(glview, lp);      
          }

    }

    MediaPlayer backgroundmusic;
    boolean changevol = false;
    int lastland;
    int curpos[];
    float curvol;

    public static boolean activityVisible = true;

    @Override
    protected void onPause() {
        super.onPause();
        activityVisible = false;

        boolean keep;

        synchronized (this) {
            // recordscore();
            keep = keepinmemory();
        }

        if (keep) {
            Intent startIntent = new Intent(HyperRogue.this, ForegroundService.class);
            startIntent.setAction(ForegroundService.STARTFOREGROUND_ACTION);
            startService(startIntent);
        } else {
            Intent stopIntent = new Intent(HyperRogue.this, ForegroundService.class);
            stopIntent.setAction(ForegroundService.STOPFOREGROUND_ACTION);
            startService(stopIntent);
        }

        if (backgroundmusic != null) backgroundmusic.stop();
        if(listening) { 
          mSensorManager.unregisterListener(this);
          listening = false;
          }
    }

    // remove for the lite version START

     public void UiChangeListener()
     {
         int currentApiVersion = android.os.Build.VERSION.SDK_INT;
         if(currentApiVersion >= 19) {
             final View decorView = getWindow().getDecorView();
             final int flags = 
               immersive ?
                     View.SYSTEM_UI_FLAG_FULLSCREEN |
                             View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
                             View.SYSTEM_UI_FLAG_HIDE_NAVIGATION :
                     0;

             decorView.setSystemUiVisibility(flags);

             decorView.setOnSystemUiVisibilityChangeListener(new View.OnSystemUiVisibilityChangeListener() {
                 @Override
                 public void onSystemUiVisibilityChange(int visibility) {
                     if ((visibility & View.SYSTEM_UI_FLAG_FULLSCREEN) == 0) {
                         decorView.setSystemUiVisibility(flags);
                     }
                 }
             });
         }
     }

    protected void onResume() {
    	super.onResume();
        activityVisible = true;
        if(backgroundmusic != null) {
        	try {backgroundmusic.prepare(); } catch(Exception e) {}
        	backgroundmusic.start();
            }
        UiChangeListener();
        }

    long lastclock;

   MediaPlayer sounds[];
   private static final int CHANNELS = 4;

   protected void playSound(String s, int vol) {
       if(effvolume == 0) return;
       if (Looper.myLooper() != Looper.getMainLooper()) {
           final String final_s = s;
           final int final_vol = vol;
           runOnUiThread(new Runnable() {
               public void run() {
                   playSound(final_s, final_vol);
               }
           });
           return;
       }
       try {
         if(sounds == null) sounds = new MediaPlayer[CHANNELS];
         AssetFileDescriptor afd = getAssets().openFd("sounds/" + s + ".ogg");

         int i;
         for(i=0; i<CHANNELS; i++)
               if(sounds[i] == null || !sounds[i].isPlaying())
                   break;

         if(i == CHANNELS) return;
         if(sounds[i] == null) {
             sounds[i] = new MediaPlayer();
             sounds[i].setOnErrorListener(new MediaPlayer.OnErrorListener()
             {
                 @Override
                 public boolean onError(MediaPlayer mp, int what, int extra)
                 {
                     /* Toast.makeText(getApplicationContext(), String.format("Error(%s%s)", what, extra),
                             Toast.LENGTH_SHORT).show(); */
                     return true;
                 }
             });
             /* sounds[i].setOnCompletionListener(new MediaPlayer.OnCompletionListener() {
                 public void onCompletion(MediaPlayer mp) {
                     mp.release();

                 };
             }); */
         } else sounds[i].reset();
         sounds[i].setDataSource(afd.getFileDescriptor(), afd.getStartOffset(), afd.getLength());
         afd.close();
         sounds[i].prepare();
         sounds[i].setVolume((float)(vol * effvolume / 12800.), (float)(vol * effvolume / 12800.));
         sounds[i].start();
         /* Toast.makeText(getApplicationContext(), "playing " + s
                   + " on "+Integer.toString(i)+ " vol "+Integer.toString(vol), Toast.LENGTH_SHORT).show(); */

       }
     catch(IOException ex) {
     }
     }

    protected void checkMusic() {
    	if(musicvolume == 0) {
    	  if(backgroundmusic != null) {
    		  backgroundmusic.stop();
    		  backgroundmusic.release();
    		  backgroundmusic = null;
              curvol = 0; changevol = true;
              lastland = -1;
    	  }
    	  return;
    	}
    	int curland = getland();

    	long curclock = System.currentTimeMillis();
    	float delta = (curclock - lastclock) / 1000.0f;
    	lastclock = curclock;

  	    if(lastland == curland) {
  	      if(curvol < 1 || changevol) {
    	    curvol += delta;
    	    if(curvol > 1) curvol = 1;
    	    if(backgroundmusic != null)
      	      backgroundmusic.setVolume(curvol * musicvolume / 128, curvol * musicvolume / 128);
    	    }
    	  }
    	else if(backgroundmusic == null) {
    	  int id = R.raw.crossroads;

  if(curland ==  2) id = R.raw.crossroads; // Crossroads
  if(curland ==  3) id = R.raw.desert; // Desert
  if(curland ==  4) id = R.raw.icyland; // Icy Land
  if(curland ==  5) id = R.raw.caves; // Living Cave
  if(curland ==  6) id = R.raw.jungle; // Jungle
  if(curland ==  7) id = R.raw.laboratory; // Alchemist Lab
  if(curland ==  8) id = R.raw.mirror; // Hall of Mirrors
  if(curland ==  9) id = R.raw.graveyard; // Graveyard
  if(curland == 10) id = R.raw.rlyeh; // R'Lyeh
  if(curland == 11) id = R.raw.hell; // Hell
  if(curland == 12) id = R.raw.icyland; // Cocytus
  if(curland == 13) id = R.raw.motion; // Land of Eternal Motion
  if(curland == 14) id = R.raw.jungle; // Dry Forest
  if(curland == 15) id = R.raw.caves; // Emerald Mine
  if(curland == 16) id = R.raw.laboratory; // Vineyard
  if(curland == 17) id = R.raw.caves; // Dead Cave
  if(curland == 18) id = R.raw.motion; // Hive
  if(curland == 19) id = R.raw.mirror; // Land of Power
  if(curland == 20) id = R.raw.palace; // Camelot
  if(curland == 21) id = R.raw.rlyeh; // Temple of Cthulhu
  if(curland == 22) id = R.raw.crossroads; // Crossroads II
  if(curland == 23) id = R.raw.caribbean; // Caribbean
  if(curland == 24) id = R.raw.desert; // Red Rock Valley
  if(curland == 25) id = R.raw.hell; // Minefield
  if(curland == 26) id = R.raw.ocean; // Ocean
  if(curland == 27) id = R.raw.rlyeh; // Whirlpool
  if(curland == 28) id = R.raw.palace; // Palace
  if(curland == 29) id = R.raw.ocean; // Living Fjord
  if(curland == 30) id = R.raw.ivory; // Ivory Tower
  if(curland == 31) id = R.raw.motion; // Zebra
  if(curland == 32) id = R.raw.hell; // Plane of Fire
  if(curland == 33) id = R.raw.motion; // Plane of Air
  if(curland == 34) id = R.raw.caves; // Plane of Earth
  if(curland == 35) id = R.raw.caribbean; // Plane of Water
  if(curland == 36) id = R.raw.crossroads; // Crossroads III
  if(curland == 39) id = R.raw.laboratory; // Canvas
  if(curland == 41) id = R.raw.caribbean; // Wild West
  if(curland == 42) id = R.raw.laboratory; // Land of Storms
  if(curland == 43) id = R.raw.jungle; // Overgrown Woods
  if(curland == 44) id = R.raw.jungle; // Clearing
  if(curland == 45) id = R.raw.graveyard; // Haunted Woods
  if(curland == 48) id = R.raw.ivory; // Windy Plains
  if(curland == 49) id = R.raw.hell; // Rose Garden
  if(curland == 50) id = R.raw.ocean; // Warped Coast
  if(curland == 52) id = R.raw.crossroads; // Crossroads IV
  if(curland == 53) id = R.raw.ivory; // Yendorian Forest
  if(curland == 54) id = R.raw.crossroads; // Galápagos
  if(curland == 55) id = R.raw.caves; // Dragon Chasms
  if(curland == 56) id = R.raw.ocean; // Kraken Depths
  if(curland == 57) id = R.raw.graveyard; // Burial Grounds
  if(curland == 58) id = R.raw.desert; // Trollheim
  if(curland == 59) id = R.raw.graveyard; // Halloween
  if(curland == 60) id = R.raw.palace; // Dungeon
  if(curland == 61) id = R.raw.mountain; // Lost Mountain
  if(curland == 62) id = R.raw.mirror; // Reptiles
  if(curland == 63) id = R.raw.desert; // Prairie
  if(curland == 64) id = R.raw.mirror; // Bull Dash
  if(curland == 65) id = R.raw.crossroads; // Crossroads V
  if(curland == 66) id = R.raw.laboratory; // Cellular Automaton
  if(curland == 67) id = R.raw.mirror; // Mirror Wall
  if(curland == 68) id = R.raw.mirror; // Reflection
  if(curland == 69) id = R.raw.mirror; // Reflection
  if(curland == 70) id = R.raw.mirror; // Reflection
  if(curland == 71) id = R.raw.mirror; // Mirror Land
  if(curland == 72) id = R.raw.hell; // Volcanic Wasteland
  if(curland == 73) id = R.raw.icyland; // Blizzard
  if(curland == 74) id = R.raw.hunting; // Hunting Ground
  if(curland == 75) id = R.raw.hunting; // Terracotta Army
  if(curland == 76) id = R.raw.hunting; // Terracotta Army
  if(curland == 77) id = R.raw.palace; // Crystal World
  if(curland == 78) id = R.raw.desert; // Snake Nest
  if(curland == 79) id = R.raw.caribbean; // Docks
  if(curland == 80) id = R.raw.mountain; // Ruined City
  if(curland == 81) id = R.raw.caves; // Magnetosphere
  if(curland == 82) id = R.raw.laboratory; // Jelly Kingdom
  if(curland == 84) id = R.raw.caribbean; // Brown Island
  if(curland == 85) id = R.raw.ivory; // Free Fall
  if(curland == 86) id = R.raw.palace; // Irradiated Field
  if(curland == 87) id = R.raw.crossroads; // Space Rocks
  if(curland == 88) id = R.raw.hunting; // Wetland
  if(curland == 89) id = R.raw.laboratory; // Frog Park
  if(curland == 90) id = R.raw.mountain; // Eclectic City
  if(curland == 91) id = R.raw.graveyard; // Cursed Canyon
  if(curland == 92) id = R.raw.laboratory; // Dice Reserve

          if(id > 0) backgroundmusic = MediaPlayer.create(this, id);
    	  if(backgroundmusic != null) {
    		  backgroundmusic.setLooping(true);
    		  curvol = 0;
    		  backgroundmusic.setVolume(curvol, curvol);
    		  if(curpos != null)
    		    backgroundmusic.seekTo(curpos[curland]);
    		  backgroundmusic.start();
    		  lastland = curland;
    	      }
    	  else curland = 0;
    	  }
    	else if(curvol <= delta) {
    	  if(curpos == null) curpos = new int[128];
    	  curpos[lastland] = backgroundmusic.getCurrentPosition();
    	  if(backgroundmusic != null) backgroundmusic.stop();
    	  backgroundmusic = null;
    	  }
    	else {
    	  curvol -= delta;
    	  backgroundmusic.setVolume(curvol * musicvolume / 128, curvol * musicvolume / 128);
    	  }
    	}    

     public int getTextWidth(String s, int size) {
       Rect bounds = new Rect();
       Paint pon = new Paint();
       pon.setAntiAlias(true);
       pon.setTextSize(size);
       pon.setTextAlign(Paint.Align.LEFT);
       pon.setTypeface(Typeface.DEFAULT_BOLD);
       pon.getTextBounds(s,0,s.length(),bounds);
       return bounds.width();
       }
 
     public byte[] getAsset(String s) {
        try {
            InputStream is = getAssets().open(s);
            int length = is.available();
            byte[] data = new byte[length];
            is.read(data);
            return data;
        } catch (IOException e) {
            return null;
        }
       }

  private SensorManager mSensorManager;
  private Sensor mSensor;
  // private final float[] mAccelerometerReading = new float[3];
  // private final float[] mMagnetometerReading = new float[3];

  private final float[] mRotationMatrix = {1,0,0, 0,1,0, 0,0,1};
  
  boolean listening;

     Sensor mRotation; // mAccelerometer, mMagneticField;

     final int[] msgn = {1, 1, 1};
     final int[] msgnl = {1, 1, -1};
     final int[] lsc = {1, 0, 2};

  double getOrientation(int i, int j) {
    if(!listening) {
        final HyperRogue act = this;
        runOnUiThread(new Runnable() { public void run() {
            listening = true;
            /*act.mAccelerometer =  act.mSensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
            act.mMagneticField =  act.mSensorManager.getDefaultSensor(Sensor.TYPE_GAME_ROTATION_VECTOR);
            act.mSensorManager.registerListener(act, act.mAccelerometer,
                    SensorManager.SENSOR_DELAY_NORMAL, SensorManager.SENSOR_DELAY_UI);
            act.mSensorManager.registerListener(act, act.mMagneticField,
                    SensorManager.SENSOR_DELAY_NORMAL, SensorManager.SENSOR_DELAY_UI);*/
            act.mRotation = act.mSensorManager.getDefaultSensor(Sensor.TYPE_GAME_ROTATION_VECTOR);
            act.mSensorManager.registerListener(act, act.mRotation,
                    SensorManager.SENSOR_DELAY_NORMAL, SensorManager.SENSOR_DELAY_UI);
        }});
      }
      int rotation =  getWindowManager().getDefaultDisplay().getRotation();

      switch(rotation) {
        case android.view.Surface.ROTATION_0:
          return mRotationMatrix[j+3*i];

        case android.view.Surface.ROTATION_90:
          return mRotationMatrix[lsc[j]+3*i] * (j==0?-1:1);

        case android.view.Surface.ROTATION_270:
          return mRotationMatrix[lsc[j]+3*i]*(j==1?-1:1);

        case android.view.Surface.ROTATION_180:
          return mRotationMatrix[j+3*i]*(j<2?-1:1);

        default:
          return mRotationMatrix[j+3*i];
        }
    }

 @Override
  public void onAccuracyChanged(Sensor sensor, int accuracy) {
    // Do something here if sensor accuracy changes.
    // You must implement this callback in your code.
  }

  public void onSensorChanged(SensorEvent event) {
      /*if (event.sensor == mAccelerometer) {
          System.arraycopy(event.values, 0, mAccelerometerReading,
                  0, mAccelerometerReading.length);
          mSensorManager.getRotationMatrix(mRotationMatrix, null,
                  mAccelerometerReading, mMagnetometerReading);
      } else if (event.sensor == mMagneticField) {
          System.arraycopy(event.values, 0, mMagnetometerReading,
                  0, mMagnetometerReading.length);
          mSensorManager.getRotationMatrix(mRotationMatrix, null,
                  mAccelerometerReading, mMagnetometerReading);
      }*/
      if(event.sensor.getType() == Sensor.TYPE_GAME_ROTATION_VECTOR)
          mSensorManager.getRotationMatrixFromVector(mRotationMatrix, event.values);
  }

    }

class HRConfigChooser implements EGLConfigChooser {

    private int getValue(EGL10 egl, EGLDisplay display, EGLConfig config, int attr) {
        int[] mValue = new int[1];
        if (egl.eglGetConfigAttrib(display, config, attr, mValue))
            return mValue[0];
        return 0;
    }

    public EGLConfig chooseConfig(EGL10 egl, EGLDisplay display) {

        int[] num_config = new int[1];

        int[] basic = new int[]{
                EGL10.EGL_RED_SIZE, 5,
                EGL10.EGL_GREEN_SIZE, 6,
                EGL10.EGL_BLUE_SIZE, 5,
                EGL10.EGL_STENCIL_SIZE, 1,
                EGL10.EGL_DEPTH_SIZE, 8,
                EGL10.EGL_NONE
         };

        egl.eglChooseConfig(display, basic, null, 0, num_config);
        if (num_config[0] <= 0) {
            basic = new int[]{EGL10.EGL_NONE};
            egl.eglChooseConfig(display, basic, null, 0, num_config);
        }

        if (num_config[0] <= 0) throw new IllegalArgumentException("no config available");

        EGLConfig[] configs = new EGLConfig[num_config[0]];
        egl.eglChooseConfig(display, basic, configs, num_config[0], num_config);

        EGLConfig best = null;
        int bestscore = 0;

        for (EGLConfig config : configs) {
            int d = getValue(egl, display, config, EGL10.EGL_DEPTH_SIZE);
            int s = getValue(egl, display, config, EGL10.EGL_STENCIL_SIZE);
            int r = getValue(egl, display, config, EGL10.EGL_RED_SIZE);
            int g = getValue(egl, display, config, EGL10.EGL_GREEN_SIZE);
            int b = getValue(egl, display, config, EGL10.EGL_BLUE_SIZE);
            int a = getValue(egl, display, config, EGL10.EGL_ALPHA_SIZE);
            int score = 10000;
            if (s == 0) score -= 2000;
            score -= s;
            if (d < 8) score -= 1000;
            score += d;
            score -= a;
            int all = r + g + b;
            if (all < 24) score -= 10 * (24 - all);
            score -= 10 * (all - 24);
            if (score > bestscore) {
                bestscore = score;
                best = config;
            }
        }

        return best;
    }

}