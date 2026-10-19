package org.openxray;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.DocumentsContract;
import android.provider.Settings;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.RadioGroup;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;

/**
 * Minimal utilitarian launcher for OpenXRay.
 * Allows selecting the game mode, data directory, and passing engine parameters.
 */
public class LauncherActivity extends Activity
{
    private static final String PREFS_NAME = "OpenXRayPrefs";
    private static final String KEY_GAME_MODE = "game_mode";
    private static final String KEY_GAME_DIR = "game_dir";
    private static final String KEY_CMD_ARGS = "cmd_args";

    private static final int REQUEST_CODE_FOLDER_PICK = 1001;
    private static final int REQUEST_CODE_PERMISSIONS = 1002;

    private RadioGroup mRgGameMode;
    private EditText mEditGameDir;
    private Button mBtnBrowseDir;
    private TextView mTxtDirStatus;
    private EditText mEditCmdArgs;
    private Button mBtnLaunch;

    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_launcher);

        initViews();
        loadPreferences();
        checkAndRequestPermissions();
    }

    private void initViews()
    {
        mRgGameMode = findViewById(R.id.rgGameMode);
        mEditGameDir = findViewById(R.id.editGameDir);
        mBtnBrowseDir = findViewById(R.id.btnBrowseDir);
        mTxtDirStatus = findViewById(R.id.txtDirStatus);
        mEditCmdArgs = findViewById(R.id.editCmdArgs);
        mBtnLaunch = findViewById(R.id.btnLaunch);

        mBtnBrowseDir.setOnClickListener(new View.OnClickListener()
        {
            @Override
            public void onClick(View v)
            {
                openFolderPicker();
            }
        });

        mEditGameDir.addTextChangedListener(new TextWatcher()
        {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {}

            @Override
            public void afterTextChanged(Editable s)
            {
                validateDirectoryStatus(s.toString());
            }
        });

        mBtnLaunch.setOnClickListener(new View.OnClickListener()
        {
            @Override
            public void onClick(View v)
            {
                launchGame();
            }
        });
    }

    private void loadPreferences()
    {
        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);

        String savedMode = prefs.getString(KEY_GAME_MODE, "-cop");
        switch (savedMode)
        {
            case "-soc":
                mRgGameMode.check(R.id.rbSoc);
                break;
            case "-cs":
                mRgGameMode.check(R.id.rbCs);
                break;
            case "-unlock_game_mode":
                mRgGameMode.check(R.id.rbCustom);
                break;
            case "-cop":
            default:
                mRgGameMode.check(R.id.rbCop);
                break;
        }

        String defaultDir = "/sdcard/stalker";
        File externalFiles = getExternalFilesDir(null);
        if (externalFiles != null && externalFiles.exists())
        {
            File fallbackFs = new File(externalFiles, "fsgame.ltx");
            if (fallbackFs.exists())
            {
                defaultDir = externalFiles.getAbsolutePath();
            }
        }

        String savedDir = prefs.getString(KEY_GAME_DIR, defaultDir);
        mEditGameDir.setText(savedDir);
        validateDirectoryStatus(savedDir);

        String savedArgs = prefs.getString(KEY_CMD_ARGS, "");
        mEditCmdArgs.setText(savedArgs);
    }

    private void savePreferences(String mode, String dir, String args)
    {
        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        prefs.edit()
            .putString(KEY_GAME_MODE, mode)
            .putString(KEY_GAME_DIR, dir)
            .putString(KEY_CMD_ARGS, args)
            .apply();
    }

    private String getSelectedGameMode()
    {
        int checkedId = mRgGameMode.getCheckedRadioButtonId();
        if (checkedId == R.id.rbSoc)
        {
            return "-soc";
        }
        else if (checkedId == R.id.rbCs)
        {
            return "-cs";
        }
        else if (checkedId == R.id.rbCustom)
        {
            return "-unlock_game_mode";
        }
        else
        {
            return "-cop";
        }
    }

    private void validateDirectoryStatus(String path)
    {
        if (path == null || path.trim().isEmpty())
        {
            mTxtDirStatus.setText("Status: No directory specified");
            mTxtDirStatus.setTextColor(0xFF888888);
            return;
        }

        File dir = new File(path.trim());
        if (!dir.exists() || !dir.isDirectory())
        {
            mTxtDirStatus.setText("Status: Directory does not exist");
            mTxtDirStatus.setTextColor(0xFFFF4444);
            return;
        }

        File fsltx = new File(dir, "fsgame.ltx");
        if (fsltx.exists() && fsltx.isFile())
        {
            mTxtDirStatus.setText("Status: fsgame.ltx found");
            mTxtDirStatus.setTextColor(0xFF44BB44);
        }
        else
        {
            mTxtDirStatus.setText("Status: fsgame.ltx NOT found in directory");
            mTxtDirStatus.setTextColor(0xFFFF9800);
        }
    }

    private void openFolderPicker()
    {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        try
        {
            startActivityForResult(intent, REQUEST_CODE_FOLDER_PICK);
        }
        catch (Exception e)
        {
            Toast.makeText(this, "No file manager found for folder selection", Toast.LENGTH_SHORT).show();
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data)
    {
        super.onActivityResult(requestCode, resultCode, data);

        if (requestCode == REQUEST_CODE_FOLDER_PICK && resultCode == RESULT_OK && data != null)
        {
            Uri treeUri = data.getData();
            if (treeUri != null)
            {
                try
                {
                    getContentResolver().takePersistableUriPermission(
                        treeUri,
                        Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                    );
                }
                catch (Exception ignored)
                {
                }

                String resolvedPath = resolveTreeUriToPath(treeUri);
                if (resolvedPath != null && !resolvedPath.isEmpty())
                {
                    mEditGameDir.setText(resolvedPath);
                }
            }
        }
    }

    private String resolveTreeUriToPath(Uri uri)
    {
        if (uri == null)
        {
            return null;
        }

        try
        {
            String docId = DocumentsContract.getTreeDocumentId(uri);
            if (docId != null)
            {
                String[] parts = docId.split(":", 2);
                String type = parts[0];
                String relativePath = parts.length > 1 ? parts[1] : "";

                if ("primary".equalsIgnoreCase(type))
                {
                    String externalRoot = Environment.getExternalStorageDirectory().getAbsolutePath();
                    return relativePath.isEmpty() ? externalRoot : externalRoot + "/" + relativePath;
                }
                else
                {
                    File secondary = new File("/storage/" + type + "/" + relativePath);
                    if (secondary.exists())
                    {
                        return secondary.getAbsolutePath();
                    }
                    return "/storage/" + type + "/" + relativePath;
                }
            }
        }
        catch (Exception ignored)
        {
        }

        String raw = uri.getPath();
        if (raw != null && raw.contains(":"))
        {
            String[] parts = raw.split(":", 2);
            if (parts.length > 1)
            {
                return Environment.getExternalStorageDirectory().getAbsolutePath() + "/" + parts[1];
            }
        }

        return uri.getPath();
    }

    private void checkAndRequestPermissions()
    {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R)
        {
            if (!Environment.isExternalStorageManager())
            {
                try
                {
                    Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
                    intent.setData(Uri.parse("package:" + getPackageName()));
                    startActivity(intent);
                }
                catch (Exception e)
                {
                    Intent intent = new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION);
                    startActivity(intent);
                }
            }
        }
        else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M)
        {
            if (checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED ||
                checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED)
            {
                requestPermissions(new String[] {
                    Manifest.permission.READ_EXTERNAL_STORAGE,
                    Manifest.permission.WRITE_EXTERNAL_STORAGE
                }, REQUEST_CODE_PERMISSIONS);
            }
        }
    }

    private void launchGame()
    {
        final String selectedMode = getSelectedGameMode();
        final String gameDir = mEditGameDir.getText().toString().trim();
        final String customArgs = mEditCmdArgs.getText().toString().trim();

        if (gameDir.isEmpty())
        {
            Toast.makeText(this, "Please specify a game directory", Toast.LENGTH_SHORT).show();
            return;
        }

        File dir = new File(gameDir);
        File fsltx = new File(dir, "fsgame.ltx");

        if (!fsltx.exists())
        {
            new AlertDialog.Builder(this)
                .setTitle("fsgame.ltx not found")
                .setMessage("fsgame.ltx was not detected in the selected directory.\n\nPath: " + gameDir + "\n\nThe game might fail to start if assets are missing. Launch anyway?")
                .setPositiveButton("Launch Anyway", (dialog, which) -> executeLaunch(selectedMode, gameDir, customArgs))
                .setNegativeButton("Cancel", null)
                .show();
        }
        else
        {
            executeLaunch(selectedMode, gameDir, customArgs);
        }
    }

    private void executeLaunch(String mode, String dir, String args)
    {
        savePreferences(mode, dir, args);

        Intent intent = new Intent(LauncherActivity.this, XRayActivity.class);
        intent.putExtra(XRayActivity.EXTRA_GAME_MODE, mode);
        intent.putExtra(XRayActivity.EXTRA_WORKING_DIR, dir);
        intent.putExtra(XRayActivity.EXTRA_COMMAND_LINE, args);
        startActivity(intent);
    }
}
