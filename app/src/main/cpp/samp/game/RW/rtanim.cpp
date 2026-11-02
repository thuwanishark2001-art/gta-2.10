#include "RenderWare.h"
#include "game/common.h"
#include "../vendor/armhook/patch.h"

void RtAnimAnimationFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc) {
    CHook::CallFunction<void>(g_libGTASA + (VER_x32 ? 0x1EADA4 + 1 : 0x786CCC), blockSize, numBlocksToPrealloc);
}

RwBool RtAnimInitialize() {
    return CHook::CallFunction<RwBool>(g_libGTASA + (VER_x32 ? 0x1EADBC + 1 : 0x786CE0));
}

RwBool RtAnimRegisterInterpolationScheme(RtAnimInterpolatorInfo* interpolatorInfo) {
    return CHook::CallFunction<RwBool>(g_libGTASA + (VER_x32 ? 0x1EAE68 + 1 : 0x786DB4), interpolatorInfo);
}

RtAnimInterpolatorInfo* RtAnimGetInterpolatorInfo(RwInt32 typeID) {
    return CHook::CallFunction<RtAnimInterpolatorInfo*>(g_libGTASA + (VER_x32 ? 0x1EAEF4 + 1 : 0x786E40), typeID);
}

RtAnimAnimation* RtAnimAnimationCreate(RwInt32 typeID, RwInt32 numFrames, RwInt32 flags, RwReal duration) {
    return CHook::CallFunction<RtAnimAnimation*>(g_libGTASA + (VER_x32 ? 0x1EAF2C + 1 : 0x786E80), typeID, numFrames, flags, duration);
}

RwBool RtAnimAnimationDestroy(RtAnimAnimation* animation) {
    return CHook::CallFunction<RwBool>(g_libGTASA + (VER_x32 ? 0x1EAFBC + 1 : 0x786F60), animation);
}

RtAnimAnimation* RtAnimAnimationRead(const RwChar* filename) {
    return CHook::CallFunction<RtAnimAnimation*>(g_libGTASA + (VER_x32 ? 0x1EAFD8 + 1 : 0x786F88), filename);
}

RwBool RtAnimAnimationWrite(const RtAnimAnimation* animation, const RwChar* filename) {
    return CHook::CallFunction<RwBool>(g_libGTASA + (VER_x32 ? 0x1EB130 + 1 : 0x7871C8), animation, filename);
}

RtAnimAnimation* RtAnimAnimationStreamRead(RwStream* stream) {
    return CHook::CallFunction<RtAnimAnimation*>(g_libGTASA + (VER_x32 ? 0x1EB010 + 1 : 0x786FF0), stream);
}

RwBool RtAnimAnimationStreamWrite(const RtAnimAnimation* animation, RwStream* stream) {
    return CHook::CallFunction<RwBool>(g_libGTASA + (VER_x32 ? 0x1EB15C + 1 : 0x787218), animation, stream);
}

RwInt32 RtAnimAnimationStreamGetSize(const RtAnimAnimation* animation) {
    return CHook::CallFunction<RwInt32>(g_libGTASA + (VER_x32 ? 0x1EB1E4 + 1 : 0x787314), animation);
}

RwUInt32 RtAnimAnimationGetNumNodes(const RtAnimAnimation* animation) {
    return CHook::CallFunction<RwUInt32>(g_libGTASA + (VER_x32 ? 0x1EB1F2 + 1 : 0x787334), animation);
}

RtAnimInterpolator* RtAnimInterpolatorCreate(RwInt32 numNodes, RwInt32 maxInterpKeyFrameSize) {
    return CHook::CallFunction<RtAnimInterpolator*>(g_libGTASA + (VER_x32 ? 0x1EB218 + 1 : 0x787378), numNodes, maxInterpKeyFrameSize);
}

void RtAnimInterpolatorDestroy(RtAnimInterpolator* anim) {
    CHook::CallFunction<void>(g_libGTASA + (VER_x32 ? 0x1EB270 + 1 : 0x7873EC), anim);
}

RwBool RtAnimInterpolatorSetCurrentAnim(RtAnimInterpolator* animI, RtAnimAnimation* anim) {
    return CHook::CallFunction<RwBool>(g_libGTASA + (VER_x32 ? 0x1EB284 + 1 : 0x787400), animI, anim);
}

RwBool RtAnimInterpolatorSetKeyFrameCallBacks(RtAnimInterpolator* anim, RwInt32 keyFrameTypeID) {
    return CHook::CallFunction<RwBool>(g_libGTASA + (VER_x32 ? 0x1EB370 + 1 : 0x787528), anim, keyFrameTypeID);
}

void RtAnimInterpolatorSetAnimLoopCallBack(RtAnimInterpolator* anim, RtAnimCallBack callBack, void* data) {
    CHook::CallFunction<void>(g_libGTASA + (VER_x32 ? 0x1EB3D8 + 1 : 0x787594), anim, callBack, data);
}

void RtAnimInterpolatorSetAnimCallBack(RtAnimInterpolator* anim, RtAnimCallBack callBack, RwReal time, void* data) {
    CHook::CallFunction<void>(g_libGTASA + (VER_x32 ? 0x1EB3DE + 1 : 0x78759C), anim, callBack, time, data);
}

RwBool RtAnimInterpolatorCopy(RtAnimInterpolator* outAnim, RtAnimInterpolator* inAnim) {
    return CHook::CallFunction<RwBool>(g_libGTASA + (VER_x32 ? 0x1EB3E6 + 1 : 0x7875A8), outAnim, inAnim);
}

RwBool RtAnimInterpolatorSubAnimTime(RtAnimInterpolator* anim, RwReal time) {
    return CHook::CallFunction<RwBool>(g_libGTASA + (VER_x32 ? 0x1EB3FC + 1 : 0x7875D8), anim, time);
}

RwBool RtAnimInterpolatorAddAnimTime(RtAnimInterpolator* anim, RwReal time) {
    return CHook::CallFunction<RwBool>(g_libGTASA + (VER_x32 ? 0x1EB530 + 1 : 0x787730), anim, time);
}

RwBool RtAnimInterpolatorSetCurrentTime(RtAnimInterpolator* anim, RwReal time) {
    return CHook::CallFunction<RwBool>(g_libGTASA + (VER_x32 ? 0x1EB6DC + 1 : 0x7878F8), anim, time);
}

RwBool RtAnimAnimationMakeDelta(RtAnimAnimation* animation, RwInt32 numNodes, RwReal time) {
    return CHook::CallFunction<RwBool>(g_libGTASA + (VER_x32 ? 0x1EB710 + 1 : 0x78792C), animation, numNodes, time);
}

RwBool RtAnimInterpolatorBlend(RtAnimInterpolator* outAnim, RtAnimInterpolator* inAnim1, RtAnimInterpolator* inAnim2, RwReal alpha) {
    return CHook::CallFunction<RwBool>(g_libGTASA + (VER_x32 ? 0x1EB854 + 1 : 0x787AD0), outAnim, inAnim1, inAnim2, alpha);
}

RwBool RtAnimInterpolatorAddTogether(RtAnimInterpolator* outAnim, RtAnimInterpolator* inAnim1, RtAnimInterpolator* inAnim2) {
    return CHook::CallFunction<RwBool>(g_libGTASA + (VER_x32 ? 0x1EB8B0 + 1 : 0x787B78), outAnim, inAnim1, inAnim2);
}

RtAnimInterpolator* RtAnimInterpolatorCreateSubInterpolator(RtAnimInterpolator* parentAnim, RwInt32 startNode, RwInt32 numNodes, RwInt32 maxInterpKeyFrameSize) {
    return CHook::CallFunction<RtAnimInterpolator*>(g_libGTASA + (VER_x32 ? 0x1EB900 + 1 : 0x787C10), parentAnim, startNode, numNodes, maxInterpKeyFrameSize);
}

RwBool RtAnimInterpolatorBlendSubInterpolator(RtAnimInterpolator* outAnim, RtAnimInterpolator* inAnim1, RtAnimInterpolator* inAnim2, RwReal alpha) {
    return CHook::CallFunction<RwBool>(g_libGTASA + (VER_x32 ? 0x1EB96C + 1 : 0x787CA8), outAnim, inAnim1, inAnim2, alpha);
}

RwBool RtAnimInterpolatorAddSubInterpolator(RtAnimInterpolator* outAnim, RtAnimInterpolator* mainAnim, RtAnimInterpolator* subAnim) {
    return CHook::CallFunction<RwBool>(g_libGTASA + (VER_x32 ? 0x1EBB0E + 1 : 0x787EEC), outAnim, mainAnim, subAnim);
}
