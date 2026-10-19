package org.openxray;

import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;

import org.libsdl.app.SDL;
import org.libsdl.app.SDLActivity;

import java.io.File;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

/**
 * Main Activity for OpenXRay on Android.
 * Extends SDLActivity to host the SDL2 surface and forward game execution.
 */
public class XRayActivity extends SDLActivity
{
    private static final String TAG = "OpenXRay";

    public static final String EXTRA_COMMAND_LINE = "command_line";
    public static final String EXTRA_WORKING_DIR = "working_dir";
    public static final String EXTRA_GAME_MODE = "game_mode";

    private String[] mLaunchArguments = null;

    public static File findFsGameLtx(File dir)
    {
        if (dir == null || !dir.isDirectory())
        {
            return null;
        }

        File direct = new File(dir, "fsgame.ltx");
        if (direct.exists())
        {
            return direct;
        }

        File[] files = dir.listFiles();
        if (files != null)
        {
            for (File file : files)
            {
                if ("fsgame.ltx".equalsIgnoreCase(file.getName()))
                {
                    return file;
                }
            }
        }

        return direct;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        applyImmersiveStickyMode();

        Intent intent = getIntent();
        if (intent != null)
        {
            String workingDir = intent.getStringExtra(EXTRA_WORKING_DIR);
            if (workingDir != null && !workingDir.trim().isEmpty())
            {
                workingDir = workingDir.trim();
                System.setProperty("user.dir", workingDir);

                try
                {
                    Class<?> libcoreClass = Class.forName("libcore.io.Libcore");
                    Object os = libcoreClass.getField("os").get(null);
                    Method chdirMethod = os.getClass().getMethod("chdir", String.class);
                    chdirMethod.invoke(os, workingDir);
                    Log.i(TAG, "Changed process working directory to: " + workingDir);
                }
                catch (Throwable t)
                {
                    Log.w(TAG, "Internal chdir not accessible: " + t.getMessage());
                }
            }

            List<String> argsList = new ArrayList<>();

            // 1. Game mode flag (e.g. -soc, -cs, -cop, -unlock_game_mode)
            String gameMode = intent.getStringExtra(EXTRA_GAME_MODE);
            if (gameMode != null && !gameMode.trim().isEmpty())
            {
                argsList.add(gameMode.trim());
            }

            // 2. Working directory and fsgame.ltx forwarding
            if (workingDir != null && !workingDir.isEmpty())
            {
                File dir = new File(workingDir);
                File fsltx = findFsGameLtx(dir);
                if (fsltx != null && fsltx.exists())
                {
                    argsList.add("-fsltx");
                    argsList.add(fsltx.getAbsolutePath());
                }
                else
                {
                    argsList.add("-fsltx");
                    argsList.add(new File(dir, "fsgame.ltx").getAbsolutePath());
                }

                argsList.add("-dir");
                argsList.add(workingDir);
            }

            // 3. User-defined additional command-line parameters
            String commandLine = intent.getStringExtra(EXTRA_COMMAND_LINE);
            if (commandLine != null && !commandLine.trim().isEmpty())
            {
                String[] split = commandLine.trim().split("\\s+");
                for (String arg : split)
                {
                    if (!arg.isEmpty())
                    {
                        argsList.add(arg);
                    }
                }
            }

            mLaunchArguments = argsList.toArray(new String[0]);
            Log.i(TAG, "Prepared " + mLaunchArguments.length + " launch arguments for SDL_main");
            for (int i = 0; i < mLaunchArguments.length; i++)
            {
                Log.i(TAG, "  argv[" + (i + 1) + "]: " + mLaunchArguments[i]);
            }
        }

        super.onCreate(savedInstanceState);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus)
    {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus)
        {
            applyImmersiveStickyMode();
        }
    }

    private void applyImmersiveStickyMode()
    {
        View decorView = getWindow().getDecorView();
        decorView.setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_FULLSCREEN
            | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
        );
    }

    @Override
    protected String[] getArguments()
    {
        if (mLaunchArguments != null)
        {
            return mLaunchArguments;
        }
        return super.getArguments();
    }

    @Override
    public void loadLibraries()
    {
        for (String lib : getLibraries())
        {
            Log.i(TAG, "Loading native library: " + lib);
            SDL.loadLibrary(lib, this);
        }
    }

    @Override
    protected String[] getLibraries()
    {
        return new String[] {
            "SDL2",
            "openal",
            "ogg",
            "theoradec",
            "theora",
            "vorbis",
            "vorbisenc",
            "vorbisfile",
            "lzo2",
            "xrAPI",
            "xrCore",
            "xrGameSpy",
            "xrODE",
            "xrOPCODE",
            "xrCDB",
            "xrMaterialSystem",
            "xrLuaJIT",
            "xrLuabind",
            "xrScriptEngine",
            "xrNetServer",
            "xrSound",
            "xrEngine",
            "xrAICore",
            "xrParticles",
            "xrPhysics",
            "xrUICore",
            "xrRender_GL",
            "xrGame",
            "main"
        };
    }

    @Override
    protected void onDestroy()
    {
        super.onDestroy();
        SDLActivity.initialize();
    }
}

