package com.samp.mobile.game;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.util.TypedValue;
import android.view.Gravity;
import android.text.InputFilter;
import android.text.InputType;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import com.samp.mobile.R;

import java.io.File;
import java.io.IOException;

/**
 * First screen of the app. Makes sure the player's own GTA: San Andreas
 * is installed, copies its engine libraries on first launch (or after the
 * game was updated), then starts the game.
 */
public class GameFilesActivity extends Activity {
    private static final String TAG = "GameFilesActivity";
    private static final int GOLD = 0xFFD4AF37;
    private static final int REQUEST_EXPORT_SETTINGS = 7101;
    private static final int REQUEST_IMPORT_SETTINGS = 7102;

    private TextView message;
    private ProgressBar progress;
    private TextView action;
    private TextView secondary;
    /** A downloaded update waiting for "install unknown apps" to be allowed. */
    private File pendingUpdate;
    private boolean updating = false;
    private EditText nameInput;
    private ImageView logo;
    private View background;
    private boolean copying = false;
    private boolean launching = false;
    /** Server content (ContentSync) is checked once per launch. */
    private boolean contentChecked = false;
    private boolean syncing = false;

    /** Set when the last game session crashed and its log was saved. */
    private String savedLog;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        buildLayout();
        applyServerArt();
        savedLog = CrashLog.saveIfLastSessionCrashed(this);
    }

    @Override
    protected void onResume() {
        super.onResume();
        // Back from the "install unknown apps" setting, or from the installer.
        if (pendingUpdate != null && Updater.canInstall(this)) {
            File apk = pendingUpdate;
            pendingUpdate = null;
            startInstall(apk);
            return;
        }
        if (pendingUpdate != null) return;   // still waiting for the setting; keep the prompt
        // Also re-checks after the player comes back from installing the game.
        checkAndStart();
    }


    private void exportSettings() {
        Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("text/plain");
        intent.putExtra(Intent.EXTRA_TITLE, "Kingmakers-settings.ini");
        startActivityForResult(intent, REQUEST_EXPORT_SETTINGS);
    }

    private void importSettings() {
        new AlertDialog.Builder(this)
            .setTitle("Import settings backup?")
            .setMessage("The selected backup will replace your current SA-MP settings. A safety copy of the current file will be kept.")
            .setNegativeButton("Cancel", null)
            .setPositiveButton("Continue", (dialog, which) -> {
                Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                intent.addCategory(Intent.CATEGORY_OPENABLE);
                intent.setType("*/*");
                startActivityForResult(intent, REQUEST_IMPORT_SETTINGS);
            })
            .show();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode != RESULT_OK || data == null || data.getData() == null) return;
        Uri uri = data.getData();
        try {
            if (requestCode == REQUEST_EXPORT_SETTINGS) {
                SettingsTransfer.exportSettings(this, uri);
                Toast.makeText(this, "Settings backup exported.", Toast.LENGTH_LONG).show();
            } else if (requestCode == REQUEST_IMPORT_SETTINGS) {
                SettingsTransfer.importSettings(this, uri);
                Toast.makeText(this, "Settings imported. Your previous settings were backed up.", Toast.LENGTH_LONG).show();
            }
        } catch (IOException e) {
            Log.e(TAG, "settings transfer failed", e);
            Toast.makeText(this, "Settings transfer failed: " + e.getMessage(), Toast.LENGTH_LONG).show();
        }
    }

    private TextView transferButton(String text, View.OnClickListener listener) {
        TextView button = new TextView(this);
        button.setText(text);
        button.setTextColor(Color.WHITE);
        button.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        button.setGravity(Gravity.CENTER);
        button.setPadding(dp(8), dp(12), dp(8), dp(12));
        GradientDrawable shape = new GradientDrawable();
        shape.setColor(0xFF2A2A30);
        shape.setCornerRadius(dp(8));
        shape.setStroke(dp(1), GOLD);
        button.setBackground(shape);
        button.setOnClickListener(listener);
        return button;
    }

    private void checkAndStart() {
        if (copying || syncing || updating) return;
        if (!Updater.checkedThisProcess && Updater.enabled()) {
            checkForUpdate();
            return;
        }
        if (savedLog != null) {
            Toast.makeText(this,
                "Previous crash log saved: " + savedLog,
                Toast.LENGTH_LONG).show();
            savedLog = null;
        }
        GameFiles.Status status = GameFiles.check(this);
        switch (status.state) {
            case READY:
                startGame(status.engine);
                break;
            case NEEDS_COPY:
                copyThenStart();
                break;
            case NOT_INSTALLED:
                showProblem("Kingmakers RP runs on your own copy of GTA: San Andreas.\n\n"
                    + "Install GTA: San Andreas (version " + GameFiles.requiredVersion() + ") from Google Play, then come back.",
                    "Open Google Play", v -> openStore());
                break;
            case WRONG_VERSION:
                showProblem("Kingmakers RP needs GTA: San Andreas version " + GameFiles.requiredVersion()
                    + ".\nYou have version " + status.installedVersion + ".",
                    "Check again", v -> checkAndStart());
                break;
            case MISSING_DATA:
                showProblem("Your GTA: San Andreas " + status.installedVersion + " is missing its game data.\n\n"
                    + "Open GTA: San Andreas once so it finishes installing, then come back.",
                    "Check again", v -> checkAndStart());
                break;
        }
    }

    private void copyThenStart() {
        message.setText("Setting up from your GTA: San Andreas.\nThis only happens once.");
        progress.setVisibility(View.VISIBLE);
        action.setVisibility(View.GONE);
        copying = true;
        new Thread(() -> {
            try {
                boolean known = GameFiles.copyLibraries(this);
                if (!known) Log.w(TAG, "Copied libraries aren't a known build; continuing");
                runOnUiThread(() -> {
                    copying = false;
                    startGame(GameFiles.check(this).engine);
                });
            } catch (Exception e) {
                Log.e(TAG, "copy failed", e);
                runOnUiThread(() -> copying = false);
                runOnUiThread(() -> showProblem("Setting up failed: " + e.getMessage(), "Try again", v -> checkAndStart()));
            }
        }, "GameFilesCopy").start();
    }

    private void startGame(GameFiles.Engine engine) {
        // Ask for a player name before allowing the first login.
        if (ClientSettings.needsName(this)) {
            askName(engine);
            return;
        }

        // Complete server-content setup before showing the launch screen.
        if (!contentChecked) {
            syncContentThenStart(engine);
            return;
        }

        launchGame(engine);
    }

    private void launchGame(GameFiles.Engine engine) {
        if (launching || isFinishing()) return;

        launching = true;
        message.setText("KINGMAKERS RP is ready.\nLaunching game...");
        progress.setIndeterminate(true);
        progress.setVisibility(View.VISIBLE);
        action.setVisibility(View.GONE);
        secondary.setVisibility(View.GONE);
        nameInput.setVisibility(View.GONE);

        new android.os.Handler(getMainLooper()).postDelayed(() -> {
            if (isFinishing()) return;

            Class<?> activity = engine == GameFiles.Engine.GTA_211
                ? SAMP211.class : SAMP.class;

            startActivity(new Intent(this, activity));
            finish();
        }, 350);
    }

    /** Asks GitHub for a newer release of the app; offers it if there is one. */
    private void checkForUpdate() {
        updating = true;
        message.setText("Checking for app updates…");
        progress.setIndeterminate(true);
        progress.setVisibility(View.VISIBLE);
        action.setVisibility(View.GONE);
        secondary.setVisibility(View.GONE);
        new Thread(() -> {
            Updater.Release release = Updater.check();
            runOnUiThread(() -> {
                updating = false;
                Updater.checkedThisProcess = true;
                if (release == null) { checkAndStart(); return; }
                offerUpdate(release);
            });
        }, "UpdateCheck").start();
    }

    private void offerUpdate(Updater.Release release) {
        String text = "Kingmakers RP " + release.version + " is available (you have " + BuildConfigVersion() + ").\n"
            + String.format(java.util.Locale.US, "%.1f MB", release.apkSize / 1048576.0);
        if (!release.notes.isEmpty()) text += "\n\n" + release.notes;
        message.setText(text);
        progress.setVisibility(View.GONE);
        action.setText("Update now");
        action.setOnClickListener(v -> downloadUpdate(release));
        action.setVisibility(View.VISIBLE);
        secondary.setText("Later");
        secondary.setOnClickListener(v -> { secondary.setVisibility(View.GONE); checkAndStart(); });
        secondary.setVisibility(View.VISIBLE);
    }

    private static String BuildConfigVersion() {
        return com.samp.mobile.BuildConfig.VERSION_NAME;
    }

    private void downloadUpdate(Updater.Release release) {
        updating = true;
        action.setVisibility(View.GONE);
        secondary.setVisibility(View.GONE);
        message.setText("Downloading " + release.version + "…");
        progress.setIndeterminate(false);
        progress.setMax(1000);
        progress.setProgress(0);
        progress.setVisibility(View.VISIBLE);
        new Thread(() -> {
            try {
                File apk = Updater.download(this, release, (done, total) -> runOnUiThread(() -> {
                    progress.setProgress((int) (done * 1000 / Math.max(1, total)));
                    message.setText(String.format(java.util.Locale.US, "Downloading %s…\n%.1f of %.1f MB",
                        release.version, done / 1048576.0, total / 1048576.0));
                }));
                runOnUiThread(() -> {
                    updating = false;
                    progress.setIndeterminate(true);
                    if (Updater.canInstall(this)) {
                        startInstall(apk);
                    } else {
                        pendingUpdate = apk;
                        showProblem("To install updates, allow Kingmakers RP to install apps.\n"
                            + "Turn on \"Allow from this source\", then come back.",
                            "Open settings", v -> startActivity(Updater.allowInstallsIntent(this)));
                        secondary.setText("Later");
                        secondary.setOnClickListener(v -> { pendingUpdate = null; secondary.setVisibility(View.GONE); checkAndStart(); });
                        secondary.setVisibility(View.VISIBLE);
                    }
                });
            } catch (IOException e) {
                Log.w(TAG, "update download failed", e);
                runOnUiThread(() -> {
                    updating = false;
                    progress.setIndeterminate(true);
                    showProblem("The update couldn't be downloaded: " + e.getMessage(), "Play without updating", v -> checkAndStart());
                });
            }
        }, "UpdateDownload").start();
    }

    private void startInstall(File apk) {
        message.setText("Installing the update…\nIf nothing appears, tap Install below.");
        progress.setVisibility(View.GONE);
        action.setText("Install");
        action.setOnClickListener(v -> startInstall(apk));
        action.setVisibility(View.VISIBLE);
        secondary.setText("Play without updating");
        secondary.setOnClickListener(v -> { secondary.setVisibility(View.GONE); checkAndStart(); });
        secondary.setVisibility(View.VISIBLE);
        try {
            startActivity(Updater.installIntent(this, apk));
        } catch (Exception e) {
            Log.w(TAG, "can't open the installer", e);
            message.setText("Couldn't open the installer: " + e.getMessage());
        }
    }

    /** Downloads the server's content (logo, background, sounds); a failure isn't fatal. */
    private void syncContentThenStart(GameFiles.Engine engine) {
        syncing = true;
        message.setText("Checking for server updates…");
        progress.setIndeterminate(true);
        progress.setVisibility(View.VISIBLE);
        action.setVisibility(View.GONE);
        new Thread(() -> {
            ContentSync.Result result = ContentSync.sync(this, (done, total, files, totalFiles) -> runOnUiThread(() -> {
                message.setText(String.format(java.util.Locale.US, "Downloading server content…\n%d of %d files (%.1f of %.1f MB)",
                    files, totalFiles, done / 1048576.0, total / 1048576.0));
                if (total > 0) {
                    progress.setIndeterminate(false);
                    progress.setMax(1000);
                    progress.setProgress((int) (done * 1000 / total));
                }
            }));
            if (!result.reachedServer) Log.w(TAG, "server content not updated: " + result.error);
            runOnUiThread(() -> {
                syncing = false;
                contentChecked = true;
                ContentSync.checkedThisProcess = true;
                progress.setIndeterminate(true);
                applyServerArt();
                startGame(engine);
            });
        }, "ContentSync").start();
    }

    /** The server's logo and background from its content, if it has them. */
    private void applyServerArt() {
        Bitmap logoArt = loadBitmap(ContentSync.file(this, "logo.png"), 512);
        if (logoArt != null) logo.setImageBitmap(logoArt);
        File bg = ContentSync.file(this, "background.jpg");
        if (bg == null) bg = ContentSync.file(this, "background.png");
        int longSide = Math.max(getResources().getDisplayMetrics().widthPixels, getResources().getDisplayMetrics().heightPixels);
        Bitmap bgArt = loadBitmap(bg, Math.max(longSide, 640));
        if (bgArt != null) background.setBackground(new BitmapDrawable(getResources(), bgArt));
    }

    /** Decodes an image no larger than about maxSide pixels; null if it isn't one. */
    private static Bitmap loadBitmap(File file, int maxSide) {
        if (file == null) return null;
        BitmapFactory.Options bounds = new BitmapFactory.Options();
        bounds.inJustDecodeBounds = true;
        BitmapFactory.decodeFile(file.getPath(), bounds);
        if (bounds.outWidth <= 0 || bounds.outHeight <= 0) return null;
        BitmapFactory.Options opts = new BitmapFactory.Options();
        opts.inSampleSize = 1;
        while (Math.max(bounds.outWidth, bounds.outHeight) / (opts.inSampleSize * 2) >= maxSide) opts.inSampleSize *= 2;
        try {
            return BitmapFactory.decodeFile(file.getPath(), opts);
        } catch (OutOfMemoryError e) {
            Log.w(TAG, "image too large: " + file);
            return null;
        }
    }

    private void askName(GameFiles.Engine engine) {
        message.setText("Choose your account name.\n3-20 letters, numbers or _ (for example King_Maker).\n"
            + "You'll use it with your password every time you log in.");
        progress.setVisibility(View.GONE);
        String current = ClientSettings.getName(this);
        if (!current.equals(ClientSettings.DEFAULT_NAME) && nameInput.getText().length() == 0) nameInput.setText(current);
        nameInput.setVisibility(View.VISIBLE);
        nameInput.requestFocus();
        View.OnClickListener play = v -> {
            String name = nameInput.getText().toString().trim();
            if (!ClientSettings.isValidName(name) || name.equals(ClientSettings.DEFAULT_NAME)) {
                message.setText("That name can't be used.\nUse 3-20 letters, numbers or _ (no spaces).");
                return;
            }
            if (!ClientSettings.setName(this, name)) {
                message.setText("Couldn't save the name. Check the app's storage and try again.");
                return;
            }
            nameInput.setVisibility(View.GONE);
            startGame(engine);
        };
        action.setText("Play");
        action.setOnClickListener(play);
        action.setVisibility(View.VISIBLE);
        nameInput.setOnEditorActionListener((v, actionId, event) -> {
            if (actionId == EditorInfo.IME_ACTION_DONE) { play.onClick(v); return true; }
            return false;
        });
    }

    private void openStore() {
        try {
            startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse("market://details?id=" + GameFiles.GTA_PACKAGE)));
        } catch (Exception e) {
            startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse("https://play.google.com/store/apps/details?id=" + GameFiles.GTA_PACKAGE)));
        }
    }

    private void showProblem(String text, String button, View.OnClickListener listener) {
        message.setText(text);
        progress.setVisibility(View.GONE);
        action.setText(button);
        action.setOnClickListener(listener);
        action.setVisibility(View.VISIBLE);
        secondary.setVisibility(View.GONE);
    }

    private void buildLayout() {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER);
        root.setBackgroundResource(R.drawable.km_loading_bg);
        root.setPadding(dp(32), dp(24), dp(32), dp(24));

        logo = new ImageView(this);
        logo.setImageResource(R.drawable.km_logo);
        logo.setScaleType(ImageView.ScaleType.FIT_CENTER);
        root.addView(logo, new LinearLayout.LayoutParams(dp(110), dp(110)));

        TextView title = new TextView(this);
        title.setText(R.string.app_name);
        title.setTextColor(GOLD);
        title.setTextSize(TypedValue.COMPLEX_UNIT_SP, 22);
        title.setGravity(Gravity.CENTER);
        root.addView(title);

        message = new TextView(this);
        message.setTextColor(Color.WHITE);
        message.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
        message.setGravity(Gravity.CENTER);
        message.setPadding(0, dp(16), 0, dp(16));
        message.setText("Checking your game files…");
        root.addView(message, new LinearLayout.LayoutParams(dp(420), LinearLayout.LayoutParams.WRAP_CONTENT));

        nameInput = new EditText(this);
        nameInput.setSingleLine(true);
        nameInput.setHint("Account name");
        nameInput.setTextColor(Color.WHITE);
        nameInput.setHintTextColor(0xFF9A9A9A);
        nameInput.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS | InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD);
        nameInput.setImeOptions(EditorInfo.IME_ACTION_DONE);
        nameInput.setFilters(new InputFilter[] { new InputFilter.LengthFilter(20) });
        nameInput.setGravity(Gravity.CENTER);
        nameInput.setVisibility(View.GONE);
        root.addView(nameInput, new LinearLayout.LayoutParams(dp(260), LinearLayout.LayoutParams.WRAP_CONTENT));

        progress = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        progress.setIndeterminate(true);
        root.addView(progress, new LinearLayout.LayoutParams(dp(240), LinearLayout.LayoutParams.WRAP_CONTENT));

        action = new TextView(this);
        action.setTextColor(Color.WHITE);
        action.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
        action.setGravity(Gravity.CENTER);
        action.setPadding(dp(20), dp(10), dp(20), dp(10));
        GradientDrawable shape = new GradientDrawable();
        shape.setColor(0xFF2A2A30);
        shape.setCornerRadius(dp(8));
        shape.setStroke(dp(1), GOLD);
        action.setBackground(shape);
        action.setVisibility(View.GONE);
        root.addView(action);

        secondary = new TextView(this);
        secondary.setTextColor(0xFFBDBDBD);
        secondary.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        secondary.setGravity(Gravity.CENTER);
        secondary.setPadding(dp(20), dp(12), dp(20), dp(8));
        secondary.setVisibility(View.GONE);
        root.addView(secondary);

        // Long-press the crown logo for optional settings backup/restore.
        logo.setOnLongClickListener(v -> {
            new AlertDialog.Builder(this)
                .setTitle("SA-MP settings")
                .setItems(new String[] {
                    "Export settings backup",
                    "Import settings backup"
                }, (dialog, which) -> {
                    if (which == 0) exportSettings();
                    else importSettings();
                })
                .show();
            return true;
        });

        // Scrolls on short screens, so the button is never cut off.
        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        scroll.setBackgroundResource(R.drawable.km_loading_bg);
        root.setBackground(null);
        scroll.addView(root, new ScrollView.LayoutParams(ScrollView.LayoutParams.MATCH_PARENT, ScrollView.LayoutParams.MATCH_PARENT));
        background = scroll;
        setContentView(scroll);
    }

    private int dp(int value) {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, value, getResources().getDisplayMetrics());
    }
}
