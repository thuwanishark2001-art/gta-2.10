//
// Created by x1y2z on 20.09.2023.
//

#include "TimeCycle.h"
#include "../vendor/armhook/patch.h"

void CTimeCycle::InjectHooks() {
    CHook::Write(g_libGTASA + (VER_x32 ? 0x00676BB8 : 0x8397D0), &CTimeCycle::m_CurrentColours);
    CHook::Write(g_libGTASA + (VER_x32 ? 0x006770C0 : 0x83A828), &CTimeCycle::m_BelowHorizonGrey);
}
