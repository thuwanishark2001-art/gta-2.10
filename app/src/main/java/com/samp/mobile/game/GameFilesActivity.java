package com.samp.mobile.game;

import android.app.Activity;
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

import com.samp.mobile.R;

import java.io.File;

/**
 * First screen of the app. Makes sure the player's own GTA: San Andreas
 * is installed, copies its engine libraries on first launch (or after the
 * game was updated), then starts the game.
 */
public class GameFilesActivity extends Activity {
    private static final String TAG = "GameFilesActivity";
    private static final int GOLD = 0xFFD4AF37;

    private TextView message;
    private ProgressBar progress;
    private TextView action;
    private EditText nameInput;
    private ImageView logo;
    private View background;
    private boolean copying = false;
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
        // Also re-checks after the player comes back from installing the game.
        checkAndStart();
    }

    private void checkAndStart() {
        if (copying || syncing) return;
        if (savedLog != null) {
            String where = savedLog;
            showProblem("The game closed unexpectedly last time.\n\nA log was saved to " + where
                + ". Please send it to the Kingmakers team so we can fix it.",
                "Continue", v -> {
                    savedLog = null;
                    checkAndStart();
                });
            return;
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
        // Every install starts with the same placeholder name, which would make
        // every new player try to log into one account. Ask for a name first.
        if (ClientSettings.needsName(this)) {
            askName(engine);
            return;
        }
        if (!contentChecked) {
            syncContentThenStart(engine);
            return;
        }
        // 2.11 (arm64): the 2.11 host with the client; 2.10 (armv7): the original.
        Class<?> activity = engine == GameFiles.Engine.GTA_211 ? SAMP211.class : SAMP.class;
        startActivity(new Intent(this, activity));
        finish();
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
