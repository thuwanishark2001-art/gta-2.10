package com.samp.mobile.game;

import android.app.Activity;
import android.view.ViewGroup;

import com.samp.mobile.R;
import com.samp.mobile.game.ui.AttachEdit;
import com.samp.mobile.game.ui.CustomKeyboard;
import com.samp.mobile.game.ui.HudManager;
import com.samp.mobile.game.ui.KmUi;
import com.samp.mobile.game.ui.LoadingScreen;
import com.samp.mobile.game.ui.dialog.DialogManager;

import java.nio.charset.StandardCharsets;

/**
 * The client's Java UI (HUD, dialogs, keyboard, object editor, loading
 * screen, Kingmakers panels) for an engine host that isn't the 2.10
 * NvEventQueueActivity. libsamp calls the host activity by method name
 * (java/jniutil.cpp), and {@link SAMP211} forwards those calls here. Each
 * method can be called from any thread.
 */
final class ClientUi {
    private final Activity activity;
    private final HudManager hud;
    private final CustomKeyboard keyboard;
    private final DialogManager dialog;
    private final AttachEdit attachEdit;
    private final LoadingScreen loadingScreen;
    private final KmUi kmUi;

    /** Call on the UI thread, after the game view is the content view. */
    ClientUi(Activity activity, KmUi.CommandSender sender) {
        this.activity = activity;
        // HudManager finds its views in the layout, as in main_render_screen.
        activity.addContentView(activity.getLayoutInflater().inflate(R.layout.hud, null),
            new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        hud = new HudManager(activity);
        keyboard = new CustomKeyboard(activity);
        dialog = new DialogManager(activity);
        attachEdit = new AttachEdit(activity);
        loadingScreen = new LoadingScreen(activity);
        kmUi = new KmUi(activity, sender);
    }

    private void ui(Runnable r) {
        activity.runOnUiThread(r);
    }

    void showLoadingScreen() {}

    void hideLoadingScreen() { ui(loadingScreen::hide); }

    void setPauseState(boolean pause) {
        ui(() -> {
            if (pause) {
                dialog.hideWithoutReset();
                attachEdit.hideWithoutReset();
            } else {
                if (dialog.isShow) dialog.showWithOldContent();
                if (attachEdit.isShow) attachEdit.showWithoutReset();
            }
        });
    }

    void showDialog(int id, int type, byte[] caption, byte[] content, byte[] left, byte[] right) {
        String c = new String(caption, StandardCharsets.UTF_8);
        String t = new String(content, StandardCharsets.UTF_8);
        String l = new String(left, StandardCharsets.UTF_8);
        String r = new String(right, StandardCharsets.UTF_8);
        ui(() -> dialog.show(id, type, c, t, l, r));
    }

    void showKeyboard() { ui(keyboard::ShowInputLayout); }
    void hideKeyboard() { ui(keyboard::HideInputLayout); }
    void showEditObject() { ui(attachEdit::show); }
    void hideEditObject() { ui(attachEdit::hide); }

    void updateHudInfo(int health, int armour, int weapon, int ammo, int clip, int money, int wanted) {
        ui(() -> hud.UpdateHudInfo(health, armour, weapon, ammo, clip, money, wanted));
    }

    void showHud() {
        ui(() -> {
            hud.ShowHud();
            kmUi.setHudVisible(true);
        });
    }

    void hideHud() {
        ui(() -> {
            hud.HideHud();
            kmUi.setHudVisible(false);
        });
    }

    void kmHud(int cash, int bank, int level, int xp, int xpMax, byte[] job, int wanted, int fuel) {
        String jobText = new String(job, StandardCharsets.UTF_8);
        ui(() -> kmUi.onHud(cash, bank, level, xp, xpMax, jobText, wanted, fuel));
    }

    void kmToast(byte[] text) {
        String message = new String(text, StandardCharsets.UTF_8);
        ui(() -> kmUi.onToast(message));
    }

    void kmSms(int from, byte[] text) {
        String message = new String(text, StandardCharsets.UTF_8);
        ui(() -> kmUi.onSms(from, message));
    }

    void kmPhone(int hasPhone, int number, int bank) { ui(() -> kmUi.onPhone(hasPhone != 0, number, bank)); }
    void kmSpeed(int kmh, int inVehicle) { ui(() -> kmUi.onSpeed(kmh, inVehicle != 0)); }

    /** True if a Kingmakers panel took the back press. Call on the UI thread. */
    boolean onBack() { return kmUi.onBack(); }
}
