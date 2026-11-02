//
// Created by x1y2z on 15.11.2023.
//

#include "IplStore.h"
#include "../vendor/armhook/patch.h"

void CIplStore::LoadIpls(CVector posn, bool bAvoidLoadInPlayerVehicleMovingDirection) {
    CHook::CallFunction<void>(g_libGTASA + (VER_x32 ? 0x28195C + 1 : 0x3AC5D8), posn, bAvoidLoadInPlayerVehicleMovingDirection);
}

void CIplStore::EnsureIplsAreInMemory(const CVector *posn) {
    CHook::CallFunction<void>(g_libGTASA + (VER_x32 ? 0x281BB8 + 1 : 0x3AC8E0), posn);
}

void CIplStore::AddIplsNeededAtPosn(const CVector *posn) {
    CHook::CallFunction<void>(g_libGTASA + (VER_x32 ? 0x2811F4 + 1 : 0x3ABC54), posn);
}

void CIplStore::RemoveIpl(int32 iplSlotIndex) {
    CHook::CallFunction<void>(g_libGTASA + (VER_x32 ? 0x280758 + 1 : 0x3AADE4), iplSlotIndex);
}
