package com.samp.mobile.game.ui;

import android.app.Activity;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.constraintlayout.widget.ConstraintLayout;

import com.nvidia.devtech.NvEventQueueActivity;
import com.samp.mobile.R;
import com.samp.mobile.launcher.other.Util;

import java.text.DecimalFormat;
import java.text.DecimalFormatSymbols;

public class HudManager {
    public Activity activity;
    public ConstraintLayout hud_layout;
    private final NvEventQueueActivity mContext = null;


    public TextView hud_money, hud_ammo;
    public ImageView hud_weapon;

    public TextView textHP, textArmour, textPlID, textWanted;

    boolean init = false;

    public HudManager(Activity aactivity) {
        activity = aactivity;
        hud_layout = aactivity.findViewById(R.id.hud_main);

        textHP = aactivity.findViewById(R.id.healthtext);
        textArmour = aactivity.findViewById(R.id.armour_text);
        //textPlID = aactivity.findViewById(R.id.textPlayerID);

        hud_money = aactivity.findViewById(R.id.money_text);
        hud_weapon = aactivity.findViewById(R.id.weapon_ammo_image);
        hud_ammo = aactivity.findViewById(R.id.weapon_ammo_text);
        textWanted = aactivity.findViewById(R.id.wantedtext);

        Util.HideLayout(hud_layout, false);
    }

    public void UpdateHudInfo(int health, int armour, int weaponid, int ammo, int ammoinclip, int money, int wanted)
    {
        textHP.setText(String.valueOf(health));
        textArmour.setText(String.valueOf(armour));
        //textPlID.setText(String.format("%s", playerid));

        DecimalFormat formatter=new DecimalFormat();
        DecimalFormatSymbols symbols= DecimalFormatSymbols.getInstance();
        symbols.setGroupingSeparator(' ');
        formatter.setDecimalFormatSymbols(symbols);
        String s= formatter.format(money);
        hud_money.setText(String.format("$ %s", s));

        textWanted.setText(String.valueOf(wanted));


        hud_weapon.setImageResource(weaponIcon(weaponid));
        hud_ammo.setText(String.format("%s / %s", ammo, ammoinclip));
        //hud_weapon.setOnClickListener(v -> NvEventQueueActivity.getInstance().onWeaponChanged());
    }

    // GTA weapon id -> our own category icon.
    static int weaponIcon(int weaponid) {
        if (weaponid <= 1) return R.drawable.km_weapon_fist;           // fist, brass knuckles
        if (weaponid <= 9) return R.drawable.km_weapon_melee;          // clubs, knife, bat, katana, chainsaw
        if (weaponid <= 15) return R.drawable.km_weapon_melee;         // gifts and cane
        if (weaponid <= 18) return R.drawable.km_weapon_thrown;        // grenade, tear gas, molotov
        if (weaponid <= 24) return R.drawable.km_weapon_pistol;
        if (weaponid <= 27) return R.drawable.km_weapon_shotgun;
        if (weaponid == 28 || weaponid == 29 || weaponid == 32) return R.drawable.km_weapon_smg;
        if (weaponid <= 31) return R.drawable.km_weapon_rifle;
        if (weaponid <= 34) return R.drawable.km_weapon_sniper;
        if (weaponid <= 38) return R.drawable.km_weapon_heavy;         // rockets, flamethrower, minigun
        if (weaponid == 39) return R.drawable.km_weapon_thrown;        // satchel charge
        if (weaponid == 46) return R.drawable.km_weapon_parachute;
        return R.drawable.km_weapon_tool;                              // detonator, spray, extinguisher, camera, goggles
    }

    public void ShowHud()
    {
        Util.ShowLayout(hud_layout, true);
    }

    public void HideHud()
    {
        Util.HideLayout(hud_layout, true);
    }
}

