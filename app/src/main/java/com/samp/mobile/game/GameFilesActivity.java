package com.samp.mobile.game;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import com.samp.mobile.R;

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
    private boolean copying = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        buildLayout();
    }

    @Override
    protected void onResume() {
        super.onResume();
        // Also re-checks after the player comes back from installing the game.
        checkAndStart();
    }

    private void checkAndStart() {
        if (copying) return;
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
        // 2.11 runs as an engine boot test until the client is ported to it.
        Class<?> activity = engine == GameFiles.Engine.GTA_211 ? Engine211Activity.class : SAMP.class;
        startActivity(new Intent(this, activity));
        finish();
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

        ImageView logo = new ImageView(this);
        logo.setImageResource(R.drawable.km_logo);
        root.addView(logo, new LinearLayout.LayoutParams(dp(120), dp(120)));

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

        setContentView(root);
    }

    private int dp(int value) {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, value, getResources().getDisplayMetrics());
    }
}
