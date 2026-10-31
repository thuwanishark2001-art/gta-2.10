package com.samp.mobile.game.ui;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.text.InputType;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.core.content.res.ResourcesCompat;

import com.samp.mobile.R;

import java.text.DecimalFormat;
import java.text.DecimalFormatSymbols;
import java.util.ArrayDeque;

/**
 * Kingmakers UI, driven by the server through the KMUI protocol
 * (docs/KMUI.md): a stats panel, a speedometer with fuel, toasts and a phone.
 * All methods must be called on the UI thread.
 */
public class KmUi {
    public interface CommandSender {
        void send(String command);
    }

    private static final int GOLD = 0xFFD4AF37;
    private static final int PANEL = 0xB4101014;
    private static final int MAX_MESSAGES = 20;

    private final Activity activity;
    private final CommandSender sender;
    private final FrameLayout root;

    private final LinearLayout statsPanel;
    private final TextView levelText;
    private final ProgressBar xpBar;
    private final TextView bankText;
    private final TextView jobText;

    private final LinearLayout speedPanel;
    private final TextView speedText;
    private final ProgressBar fuelBar;
    private final TextView fuelText;

    private final TextView toast;
    private final ArrayDeque<String> toastQueue = new ArrayDeque<>();
    private boolean toastShowing = false;

    private final LinearLayout phone;
    private final LinearLayout phoneBody;
    private final TextView phoneTitle;
    private final ArrayDeque<String> messages = new ArrayDeque<>();
    private int phoneNumber = 0;
    private int phoneBank = 0;

    private boolean hudVisible = false;
    private boolean hasHud = false;
    private boolean inVehicle = false;
    private int fuel = -1;

    public KmUi(Activity activity, CommandSender sender) {
        this.activity = activity;
        this.sender = sender;

        root = new FrameLayout(activity);
        activity.addContentView(root, new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        // Stats: top left, under the radar's row of the screen.
        statsPanel = panel(LinearLayout.VERTICAL);
        levelText = text(13, GOLD, true);
        xpBar = bar(GOLD);
        bankText = text(12, Color.WHITE, false);
        jobText = text(11, 0xFFB0B0B0, false);
        TextView phoneButton = button("Phone", v -> sender.send("/phone"));
        statsPanel.addView(levelText);
        statsPanel.addView(xpBar, new LinearLayout.LayoutParams(dp(110), dp(4)));
        statsPanel.addView(bankText);
        statsPanel.addView(jobText);
        statsPanel.addView(phoneButton, marginTop(dp(4)));
        FrameLayout.LayoutParams statsParams = new FrameLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.TOP | Gravity.START);
        statsParams.setMargins(dp(12), dp(170), 0, 0);
        root.addView(statsPanel, statsParams);

        // Speedometer: right side, above the driving controls.
        speedPanel = panel(LinearLayout.VERTICAL);
        speedPanel.setGravity(Gravity.CENTER_HORIZONTAL);
        speedText = text(26, Color.WHITE, true);
        TextView unit = text(10, 0xFFB0B0B0, false);
        unit.setText("km/h");
        fuelBar = bar(0xFF7FD17F);
        fuelText = text(10, 0xFFB0B0B0, false);
        speedPanel.addView(speedText);
        speedPanel.addView(unit);
        speedPanel.addView(fuelBar, new LinearLayout.LayoutParams(dp(80), dp(5)));
        speedPanel.addView(fuelText);
        FrameLayout.LayoutParams speedParams = new FrameLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.END | Gravity.CENTER_VERTICAL);
        speedParams.setMargins(0, 0, dp(24), dp(40));
        root.addView(speedPanel, speedParams);

        // Toasts: top centre.
        toast = text(13, Color.WHITE, true);
        toast.setBackground(rounded(PANEL, GOLD));
        toast.setPadding(dp(14), dp(8), dp(14), dp(8));
        toast.setGravity(Gravity.CENTER);
        FrameLayout.LayoutParams toastParams = new FrameLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.TOP | Gravity.CENTER_HORIZONTAL);
        toastParams.setMargins(0, dp(16), 0, 0);
        root.addView(toast, toastParams);

        // Phone: centred card.
        phone = panel(LinearLayout.VERTICAL);
        phone.setPadding(dp(14), dp(12), dp(14), dp(12));
        phoneTitle = text(15, GOLD, true);
        phoneBody = new LinearLayout(activity);
        phoneBody.setOrientation(LinearLayout.VERTICAL);
        phone.addView(phoneTitle);
        phone.addView(phoneBody, marginTop(dp(8)));
        FrameLayout.LayoutParams phoneParams = new FrameLayout.LayoutParams(dp(300), ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.CENTER);
        root.addView(phone, phoneParams);

        statsPanel.setVisibility(View.GONE);
        speedPanel.setVisibility(View.GONE);
        toast.setVisibility(View.GONE);
        phone.setVisibility(View.GONE);
    }

    // ---- server events -----------------------------------------------------

    public void onHud(int cash, int bank, int level, int xp, int xpMax, String job, int wanted, int fuel) {
        hasHud = true;
        levelText.setText("Level " + level + "   " + xp + "/" + xpMax);
        xpBar.setMax(Math.max(1, xpMax));
        xpBar.setProgress(Math.max(0, Math.min(xp, xpMax)));
        bankText.setText("Bank " + money(bank));
        jobText.setText(job);
        phoneBank = bank;
        this.fuel = fuel;
        updateFuel();
        updateVisibility();
    }

    public void onSpeed(int kmh, boolean inVehicle) {
        this.inVehicle = inVehicle;
        speedText.setText(String.valueOf(kmh));
        updateVisibility();
    }

    public void onToast(String text) {
        if (text.isEmpty()) return;
        if (toastQueue.size() < 5) toastQueue.add(text);
        if (!toastShowing) nextToast();
    }

    public void onSms(int from, String text) {
        messages.addFirst(from + ": " + text);
        while (messages.size() > MAX_MESSAGES) messages.removeLast();
        onToast("SMS from " + from);
        if (phone.getVisibility() == View.VISIBLE) showMessages();
    }

    public void onPhone(boolean hasPhone, int number, int bank) {
        if (!hasPhone) {
            onToast("You don't have a phone. 24/7 shops sell them.");
            return;
        }
        phoneNumber = number;
        phoneBank = bank;
        showPhoneHome();
        phone.setVisibility(View.VISIBLE);
    }

    public void setHudVisible(boolean visible) {
        hudVisible = visible;
        updateVisibility();
    }

    /** Back button: closes the phone if it's open. */
    public boolean onBack() {
        if (phone.getVisibility() != View.VISIBLE) return false;
        phone.setVisibility(View.GONE);
        return true;
    }

    // ---- phone screens -----------------------------------------------------

    private void showPhoneHome() {
        phoneTitle.setText(phoneNumber > 0 ? "Phone  " + phoneNumber : "Phone");
        phoneBody.removeAllViews();
        TextView balance = text(12, Color.WHITE, false);
        balance.setText("Bank balance " + money(phoneBank));
        phoneBody.addView(balance);
        phoneBody.addView(button("Messages (" + messages.size() + ")", v -> showMessages()), marginTop(dp(8)));
        phoneBody.addView(button("New message", v -> showCompose()), marginTop(dp(6)));
        phoneBody.addView(button("Call a taxi", v -> { sender.send("/calltaxi"); close(); }), marginTop(dp(6)));
        phoneBody.addView(button("Emergency (911)", v -> showEmergency()), marginTop(dp(6)));
        phoneBody.addView(button("Close", v -> close()), marginTop(dp(6)));
    }

    private void showMessages() {
        phoneTitle.setText("Messages");
        phoneBody.removeAllViews();
        LinearLayout list = new LinearLayout(activity);
        list.setOrientation(LinearLayout.VERTICAL);
        if (messages.isEmpty()) {
            TextView none = text(12, 0xFFB0B0B0, false);
            none.setText("No messages yet.");
            list.addView(none);
        }
        for (String message : messages) {
            TextView line = text(12, Color.WHITE, false);
            line.setText(message);
            list.addView(line, marginTop(dp(4)));
        }
        ScrollView scroll = new ScrollView(activity);
        scroll.addView(list);
        phoneBody.addView(scroll, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(160)));
        phoneBody.addView(button("Back", v -> showPhoneHome()), marginTop(dp(8)));
    }

    private void showCompose() {
        phoneTitle.setText("New message");
        phoneBody.removeAllViews();
        EditText number = input("Number", InputType.TYPE_CLASS_NUMBER);
        EditText message = input("Message", InputType.TYPE_CLASS_TEXT);
        phoneBody.addView(number);
        phoneBody.addView(message, marginTop(dp(6)));
        phoneBody.addView(button("Send", v -> {
            String to = number.getText().toString().trim();
            String body = clean(message.getText().toString());
            if (to.isEmpty() || body.isEmpty()) return;
            sender.send("/sms " + to + " " + body);
            showPhoneHome();
        }), marginTop(dp(8)));
        phoneBody.addView(button("Back", v -> showPhoneHome()), marginTop(dp(6)));
    }

    private void showEmergency() {
        phoneTitle.setText("911");
        phoneBody.removeAllViews();
        EditText message = input("What's happening and where?", InputType.TYPE_CLASS_TEXT);
        phoneBody.addView(message);
        phoneBody.addView(button("Call", v -> {
            String body = clean(message.getText().toString());
            if (body.isEmpty()) return;
            sender.send("/911 " + body);
            close();
        }), marginTop(dp(8)));
        phoneBody.addView(button("Back", v -> showPhoneHome()), marginTop(dp(6)));
    }

    private void close() {
        phone.setVisibility(View.GONE);
    }

    // ---- helpers -----------------------------------------------------------

    private void updateVisibility() {
        statsPanel.setVisibility(hudVisible && hasHud ? View.VISIBLE : View.GONE);
        speedPanel.setVisibility(hudVisible && inVehicle ? View.VISIBLE : View.GONE);
    }

    private void updateFuel() {
        boolean show = fuel >= 0;
        fuelBar.setVisibility(show ? View.VISIBLE : View.GONE);
        fuelText.setVisibility(show ? View.VISIBLE : View.GONE);
        if (!show) return;
        fuelBar.setMax(100);
        fuelBar.setProgress(fuel);
        fuelText.setText("Fuel " + fuel + "%");
        fuelText.setTextColor(fuel < 10 ? 0xFFE05050 : 0xFFB0B0B0);
    }

    private void nextToast() {
        String next = toastQueue.poll();
        if (next == null) {
            toastShowing = false;
            toast.setVisibility(View.GONE);
            return;
        }
        toastShowing = true;
        toast.setText(next);
        toast.setAlpha(0f);
        toast.setVisibility(View.VISIBLE);
        toast.animate().alpha(1f).setDuration(200).withEndAction(() ->
            toast.animate().alpha(0f).setStartDelay(2500).setDuration(300).withEndAction(this::nextToast));
    }

    // Commands are one line; keep the user's text from adding a second command.
    private static String clean(String text) {
        return text.replace('\n', ' ').replace('\r', ' ').trim();
    }

    private static String money(int amount) {
        DecimalFormat formatter = new DecimalFormat();
        DecimalFormatSymbols symbols = DecimalFormatSymbols.getInstance();
        symbols.setGroupingSeparator(' ');
        formatter.setDecimalFormatSymbols(symbols);
        return "$" + formatter.format(amount);
    }

    private int dp(int value) {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, value, activity.getResources().getDisplayMetrics());
    }

    private LinearLayout.LayoutParams marginTop(int margin) {
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.topMargin = margin;
        return params;
    }

    private GradientDrawable rounded(int fill, int stroke) {
        GradientDrawable shape = new GradientDrawable();
        shape.setColor(fill);
        shape.setCornerRadius(dp(8));
        if (stroke != 0) shape.setStroke(dp(1), stroke);
        return shape;
    }

    private LinearLayout panel(int orientation) {
        LinearLayout layout = new LinearLayout(activity);
        layout.setOrientation(orientation);
        layout.setBackground(rounded(PANEL, 0));
        layout.setPadding(dp(10), dp(8), dp(10), dp(8));
        return layout;
    }

    private TextView text(int sp, int colour, boolean bold) {
        TextView view = new TextView(activity);
        view.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        view.setTextColor(colour);
        Typeface font = null;
        try {
            font = ResourcesCompat.getFont(activity, R.font.montserrat_regular);
        } catch (Exception ignored) {
            // fall back to the system font
        }
        view.setTypeface(font, bold ? Typeface.BOLD : Typeface.NORMAL);
        return view;
    }

    private TextView button(String label, View.OnClickListener listener) {
        TextView view = text(13, Color.WHITE, true);
        view.setText(label);
        view.setGravity(Gravity.CENTER);
        view.setBackground(rounded(0xFF2A2A30, GOLD));
        view.setPadding(dp(10), dp(8), dp(10), dp(8));
        view.setOnClickListener(listener);
        return view;
    }

    private ProgressBar bar(int colour) {
        ProgressBar bar = new ProgressBar(activity, null, android.R.attr.progressBarStyleHorizontal);
        bar.getProgressDrawable().setTint(colour);
        bar.setMax(100);
        return bar;
    }

    private EditText input(String hint, int type) {
        EditText edit = new EditText(activity);
        edit.setHint(hint);
        edit.setInputType(type);
        edit.setSingleLine(true);
        edit.setTextColor(Color.WHITE);
        edit.setHintTextColor(0xFF808080);
        edit.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        edit.setBackground(rounded(0xFF1E1E22, 0xFF505058));
        edit.setPadding(dp(10), dp(8), dp(10), dp(8));
        return edit;
    }
}
