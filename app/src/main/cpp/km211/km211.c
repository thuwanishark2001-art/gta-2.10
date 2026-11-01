// Patches the GTA:SA 2.11 engine (libGame.so) needs to run in the
// Kingmakers launcher. Plain C, like kmdiag, so it adds no C++ runtime.
//
// The audio packs (data_sfx1, data_sfx2, data_streams) are downloaded into
// the game's private storage, which this app can't read. Without them,
// ZIP_FileCreate returns null for each sound archive, and both
// CAEMP3TrackLoader::GetDataStream (music, speech) and LoadTask::TryLoadSound
// (effects) call ZIPFile::Find on that null pointer: the crash at the start
// of a new game. Each caller already handles "not found", so Find on a
// missing archive now answers that, and the game runs silent.

#include <android/log.h>
#include <jni.h>
#include <stdint.h>

#include "shadowhook.h"

#define TAG "km211"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// ZIPFile::Find(std::string) returns {ZipEntry*, bool found} in x0/x1.
typedef struct { void *entry; uint64_t found; } find_result;
typedef find_result (*find_fn)(void *self, void *name);

static find_fn g_find_orig;
static int g_missing_reported;

static find_result find_proxy(void *self, void *name) {
    if (!self) {
        if (!g_missing_reported) {
            g_missing_reported = 1;
            LOGI("sound archive missing (audio pack not available); playing without it");
        }
        find_result none = { 0, 0 };
        return none;
    }
    return g_find_orig(self, name);
}

static int hook(const char *sym, void *proxy, void **orig) {
    if (shadowhook_hook_sym_name("libGame.so", sym, proxy, orig)) {
        LOGI("patched %s", sym);
        return 1;
    }
    LOGE("can't patch %s: %s", sym, shadowhook_to_errmsg(shadowhook_get_errno()));
    return 0;
}

// Returns how many patches were applied. Call after libGame.so is loaded.
JNIEXPORT jint JNICALL Java_com_samp_mobile_game_Engine211Patches_apply(JNIEnv *env, jclass cls) {
    (void) env; (void) cls;
    // Returns the first init's result if kmdiag already initialised it.
    if (shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false) != 0) {
        LOGE("shadowhook init failed: %s", shadowhook_to_errmsg(shadowhook_get_errno()));
        return 0;
    }
    int applied = 0;
    applied += hook("_ZN7ZIPFile4FindENSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE",
                    (void *) find_proxy, (void **) &g_find_orig);
    return applied;
}
