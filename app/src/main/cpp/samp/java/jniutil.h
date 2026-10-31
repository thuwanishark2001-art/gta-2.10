#pragma once

#include "main.h"
#include <string>

#define EXCEPTION_CHECK(env) \
	if ((env)->ExceptionCheck()) \ 
	{ \
		(env)->ExceptionDescribe(); \
		(env)->ExceptionClear(); \
		return; \
	}

class CJavaWrapper
{
public:
    CJavaWrapper(JNIEnv *env, jobject activity);
    ~CJavaWrapper() {};
    
    const char* GetClipboardString();
    void SetPauseState(bool pause);
    void ShowLoadingScreen();
    void HideLoadingScreen();
    void ShowTab();
    void HideTab();

    void UpdateHudInfo(int health, int armour, int weaponid, int ammo, int ammoinclip, int money, int wanted);
    void ShowHud();
    void HideHud();

    void SetTab(int id, char* names, int score, int pings);
    void ClearTab();
    
    void ShowKeyboard();
    void HideKeyboard();

	void exitGame();

    jbyteArray as_byte_array(unsigned char* buf, int len) {
        JNIEnv* p;
        javaVM->GetEnv((void**)&p, JNI_VERSION_1_6);
		jbyteArray array = p->NewByteArray (len);
		p->SetByteArrayRegion (array, 0, len, reinterpret_cast<jbyte*>(buf));
		return array;
	}

    void ShowDialog(int dialogStyle, int dialogID, char* title, char* text, char* button1, char* button2);

	void ShowEditObject();
	void HideEditObject();

    // Kingmakers UI (kmui/kmui.cpp). Strings are UTF-8.
    void KmHud(int cash, int bank, int level, int xp, int xpMax, const std::string& job, int wanted, int fuel);
    void KmToast(const std::string& text);
    void KmSms(int fromNumber, const std::string& text);
    void KmPhone(int hasPhone, int number, int bank);
    void KmSpeed(int kmh, int inVehicle);

    jobject activity;
    jmethodID s_setPauseState;
    jmethodID s_showLoadingScreen;
    jmethodID s_hideLoadingScreen;
    jmethodID s_ShowDialog;
    jmethodID s_showTab;
    jmethodID s_setTab;
    jmethodID s_clearTab;
    jmethodID s_hideTab;
    jmethodID s_showInputLayout;
    jmethodID s_hideInputLayout;
	jmethodID s_exitGame;
	jmethodID s_showEditObject;
	jmethodID s_hideEditObject;

    jmethodID s_showHud;
    jmethodID s_hideHud;
    jmethodID s_updateHudInfo;

    jmethodID s_kmHud;
    jmethodID s_kmToast;
    jmethodID s_kmSms;
    jmethodID s_kmPhone;
    jmethodID s_kmSpeed;
};