package com.rockstargames.oswrapper;

import android.app.UiModeManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.os.Build;
import android.os.Vibrator;
import android.telephony.TelephonyManager;

/** Device facts the 2.11 engine reads by field name (implOnInitialSetup). */
public final class DeviceInfo {
    public final int cpuFrequency;
    public final String hardware;
    public final boolean hasTouchScreen;
    public final boolean hasVibrator;
    public final boolean isPhone;
    public final boolean isTvDevice;
    public final String manufacturer;
    public final String model;
    public final int osVersion;
    public final String product;

    public DeviceInfo(Context context) {
        PackageManager pm = context.getPackageManager();
        cpuFrequency = 0;
        hardware = Build.HARDWARE;
        hasTouchScreen = pm.hasSystemFeature(PackageManager.FEATURE_TOUCHSCREEN);
        Vibrator vibrator = (Vibrator) context.getSystemService(Context.VIBRATOR_SERVICE);
        hasVibrator = vibrator != null && vibrator.hasVibrator();
        TelephonyManager tm = (TelephonyManager) context.getSystemService(Context.TELEPHONY_SERVICE);
        isPhone = tm != null && tm.getPhoneType() != TelephonyManager.PHONE_TYPE_NONE;
        UiModeManager ui = (UiModeManager) context.getSystemService(Context.UI_MODE_SERVICE);
        isTvDevice = ui != null && ui.getCurrentModeType() == Configuration.UI_MODE_TYPE_TELEVISION;
        manufacturer = Build.MANUFACTURER;
        model = Build.MODEL;
        osVersion = Build.VERSION.SDK_INT;
        product = Build.PRODUCT;
    }
}
