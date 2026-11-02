#include "jniutil.h"
#include "game/game.h"
extern CGame *pGame;

#include <sys/prctl.h>

// The calling thread's JNIEnv, attaching the thread to the VM if it isn't
// yet. GetEnv alone leaves the pointer unset on a thread the VM doesn't
// know (an engine or network thread on 2.11), and a JNI call through it
// corrupts the runtime.
JNIEnv* JavaEnv()
{
    JNIEnv* env = nullptr;
    if (javaVM->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_OK && env) return env;
    char name[17] = "native";
    prctl(PR_GET_NAME, name, 0, 0, 0);
    JavaVMAttachArgs args = { JNI_VERSION_1_6, name, nullptr };
    if (javaVM->AttachCurrentThread(&env, &args) != JNI_OK) {
        FLog("JavaEnv: can't attach thread '%s'", name);
        return nullptr;
    }
    FLog("JavaEnv: attached thread '%s' to call Java", name);
    return env;
}

CJavaWrapper::CJavaWrapper(JNIEnv *env, jobject activity)
{
    this->activity = env->NewGlobalRef(activity);

    jclass clas = env->GetObjectClass(activity);
    if(!clas)
    {
        FLog("no clas");
        return;
    }

    s_showTab = env->GetMethodID(clas, "showTab", "()V");
    s_hideTab = env->GetMethodID(clas, "hideTab", "()V");

    s_clearTab = env->GetMethodID(clas, "clearTab", "()V");
    s_setTab = env->GetMethodID(clas, "setTab", "(ILjava/lang/String;II)V");

    s_showLoadingScreen = env->GetMethodID(clas, "showLoadingScreen", "()V");
    s_hideLoadingScreen = env->GetMethodID(clas, "hideLoadingScreen", "()V");

    s_setPauseState = env->GetMethodID(clas, "setPauseState", "(Z)V");
    
    s_ShowDialog = env->GetMethodID(clas, "showDialog", "(II[B[B[B[B)V");

	s_showInputLayout = env->GetMethodID(clas, "showKeyboard", "()V");
    s_hideInputLayout = env->GetMethodID(clas, "hideKeyboard", "()V");

    s_exitGame = env->GetMethodID(clas, "exitGame", "()V");

    s_showEditObject = env->GetMethodID(clas, "showEditObject", "()V");
    s_hideEditObject = env->GetMethodID(clas, "hideEditObject", "()V");

    s_updateHudInfo = env->GetMethodID(clas, "updateHudInfo", "(IIIIIII)V");
    s_showHud = env->GetMethodID(clas, "showHud", "()V");
    s_hideHud = env->GetMethodID(clas, "hideHud", "()V");

    s_kmHud = env->GetMethodID(clas, "kmHud", "(IIIII[BII)V");
    s_kmToast = env->GetMethodID(clas, "kmToast", "([B)V");
    s_kmSms = env->GetMethodID(clas, "kmSms", "(I[B)V");
    s_kmPhone = env->GetMethodID(clas, "kmPhone", "(III)V");
    s_kmSpeed = env->GetMethodID(clas, "kmSpeed", "(II)V");
    if (env->ExceptionCheck())
    {
        // An activity without the Kingmakers UI: leave those calls disabled.
        env->ExceptionClear();
        FLog("Kingmakers UI methods missing");
        s_kmHud = s_kmToast = s_kmSms = s_kmPhone = s_kmSpeed = nullptr;
    }

    env->DeleteLocalRef(clas);
}

void CJavaWrapper::ShowKeyboard()
{
    JNIEnv* p;
    p = JavaEnv();
    if (!p) return;
    p->CallVoidMethod(activity, s_showInputLayout);
    EXCEPTION_CHECK(p);
}

void CJavaWrapper::HideKeyboard()
{
    JNIEnv* p;
    p = JavaEnv();
    if (!p) return;
    p->CallVoidMethod(activity, s_hideInputLayout);
    EXCEPTION_CHECK(p);
}

void CJavaWrapper::ShowLoadingScreen()
{
    JNIEnv* p;
    p = JavaEnv();
    if (!p) return;
    p->CallVoidMethod(activity, s_showLoadingScreen);
    EXCEPTION_CHECK(p);
}

void CJavaWrapper::HideLoadingScreen()
{
    JNIEnv* p;
    p = JavaEnv();
    if (!p) return;
    p->CallVoidMethod(activity, s_hideLoadingScreen);
    EXCEPTION_CHECK(p);
}

void CJavaWrapper::SetPauseState(bool pause)
{
    JNIEnv* p;
    p = JavaEnv();
    if (!p) return;
    p->CallVoidMethod(activity, s_setPauseState, pause);
    EXCEPTION_CHECK(p);
}

void CJavaWrapper::SetTab(int id, char* names, int score, int pings)
{
    JNIEnv* global_env;
    global_env = JavaEnv();

	if (!global_env)
	{
		LOGI("No env");
		return;
	}

	jclass strClass = global_env->FindClass("java/lang/String"); 
	jmethodID ctorID = global_env->GetMethodID(strClass, "<init>", "([BLjava/lang/String;)V"); 
	jstring encoding = global_env->NewStringUTF("UTF-8"); 

	jbyteArray bytes = global_env->NewByteArray(strlen(names)); 
	global_env->SetByteArrayRegion(bytes, 0, strlen(names), (jbyte*)names); 
	jstring str1 = (jstring)global_env->NewObject(strClass, ctorID, bytes, encoding);

    global_env->CallVoidMethod(activity, s_setTab, id, str1, score, pings);

    EXCEPTION_CHECK(global_env);
    global_env->DeleteLocalRef(str1);
    global_env->DeleteLocalRef(bytes);
    global_env->DeleteLocalRef(encoding);
    global_env->DeleteLocalRef(strClass);
}

void CJavaWrapper::ShowTab()
{
    JNIEnv* p;
    p = JavaEnv();
    if (!p) return;
    p->CallVoidMethod(activity, s_showTab);
    EXCEPTION_CHECK(p);
}

void CJavaWrapper::HideTab()
{
    JNIEnv* p;
    p = JavaEnv();
    if (!p) return;
    p->CallVoidMethod(activity, s_hideTab);
    EXCEPTION_CHECK(p);
}

void CJavaWrapper::ClearTab()
{
    JNIEnv* p;
    p = JavaEnv();
    if (!p) return;
    p->CallVoidMethod(activity, s_clearTab);
    EXCEPTION_CHECK(p);
}

void CJavaWrapper::ShowDialog(int dialogStyle, int dialogID, char* title, char* text, char* button1, char* button2)
{
	JNIEnv* env;
    env = JavaEnv();

	if (!env)
	{
		FLog("No env");
		return;
	}

	std::string sTitle(title);
	std::string sText(text);
	std::string sButton1(button1);
	std::string sButton2(button2);

	jbyteArray jstrTitle = as_byte_array((unsigned char*)sTitle.c_str(), sTitle.length());
	jbyteArray jstrText = as_byte_array((unsigned char*)sText.c_str(), sText.length());
	jbyteArray jstrButton1 = as_byte_array((unsigned char*)sButton1.c_str(), sButton1.length());
	jbyteArray jstrButton2 = as_byte_array((unsigned char*)sButton2.c_str(), sButton2.length());

	env->CallVoidMethod(activity, s_ShowDialog, dialogID, dialogStyle, jstrTitle, jstrText, jstrButton1, jstrButton2);

	EXCEPTION_CHECK(env);
}

void CJavaWrapper::UpdateHudInfo(int health, int armour, int weaponid, int ammo, int ammoinclip, int money, int wanted)
{
    JNIEnv* env;
    env = JavaEnv();

    if (!env)
    {
        FLog("No env");
        return;
    }

    env->CallVoidMethod(this->activity, this->s_updateHudInfo, health, armour, weaponid, ammo, ammoinclip, money, wanted);
}

void CJavaWrapper::ShowHud()
{
    JNIEnv* env;
    env = JavaEnv();

    if (!env)
    {
        FLog("No env");
        return;
    }
    //g_pJavaWrapper->ShowNotification(4, "HUD показан.", 5, "", "Хорошо");
    env->CallVoidMethod(this->activity, this->s_showHud);
}

void CJavaWrapper::HideHud()
{
    JNIEnv* env;
    env = JavaEnv();

    if (!env)
    {
        FLog("No env");
        return;
    }
    //g_pJavaWrapper->ShowNotification(4, "HUD скрыт.", 5, "", "Хорошо");
    env->CallVoidMethod(this->activity, this->s_hideHud);
}

void CJavaWrapper::exitGame() {

    JNIEnv* env;
    env = JavaEnv();

    if (!env)
    {
        FLog("No env");
        return;
    }

    env->CallVoidMethod(this->activity, this->s_exitGame);
}

void CJavaWrapper::ShowEditObject() {

    JNIEnv* env;
    env = JavaEnv();

    if (!env)
    {
        FLog("No env");
        return;
    }

    env->CallVoidMethod(this->activity, this->s_showEditObject);
}

void CJavaWrapper::HideEditObject() {

    JNIEnv* env;
    env = JavaEnv();

    if (!env)
    {
        FLog("No env");
        return;
    }

    env->CallVoidMethod(this->activity, this->s_hideEditObject);
}
// ---- Kingmakers UI ---------------------------------------------------------

void CJavaWrapper::KmHud(int cash, int bank, int level, int xp, int xpMax, const std::string& job, int wanted, int fuel)
{
    JNIEnv* env;
    env = JavaEnv();
    if (!env) return;
    if (!env || !s_kmHud) return;
    jbyteArray jJob = as_byte_array((unsigned char*)job.c_str(), job.length());
    env->CallVoidMethod(activity, s_kmHud, cash, bank, level, xp, xpMax, jJob, wanted, fuel);
    env->DeleteLocalRef(jJob);
    EXCEPTION_CHECK(env);
}

void CJavaWrapper::KmToast(const std::string& text)
{
    JNIEnv* env;
    env = JavaEnv();
    if (!env) return;
    if (!env || !s_kmToast) return;
    jbyteArray jText = as_byte_array((unsigned char*)text.c_str(), text.length());
    env->CallVoidMethod(activity, s_kmToast, jText);
    env->DeleteLocalRef(jText);
    EXCEPTION_CHECK(env);
}

void CJavaWrapper::KmSms(int fromNumber, const std::string& text)
{
    JNIEnv* env;
    env = JavaEnv();
    if (!env) return;
    if (!env || !s_kmSms) return;
    jbyteArray jText = as_byte_array((unsigned char*)text.c_str(), text.length());
    env->CallVoidMethod(activity, s_kmSms, fromNumber, jText);
    env->DeleteLocalRef(jText);
    EXCEPTION_CHECK(env);
}

void CJavaWrapper::KmPhone(int hasPhone, int number, int bank)
{
    JNIEnv* env;
    env = JavaEnv();
    if (!env) return;
    if (!env || !s_kmPhone) return;
    env->CallVoidMethod(activity, s_kmPhone, hasPhone, number, bank);
    EXCEPTION_CHECK(env);
}

void CJavaWrapper::KmSpeed(int kmh, int inVehicle)
{
    JNIEnv* env;
    env = JavaEnv();
    if (!env) return;
    if (!env || !s_kmSpeed) return;
    env->CallVoidMethod(activity, s_kmSpeed, kmh, inVehicle);
    EXCEPTION_CHECK(env);
}
