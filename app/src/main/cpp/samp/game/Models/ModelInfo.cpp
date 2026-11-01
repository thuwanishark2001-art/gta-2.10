//
// Created by x1y2z on 07.03.2023.
//

#include "ModelInfo.h"
#include "../vendor/armhook/patch.h"
#include "log.h"

CVehicleModelInfo* CModelInfo::AddVehicleModel(int index)
{
    auto& pInfo = CModelInfo::ms_vehicleModelInfoStore.AddItem();

    ((void(*)(CVehicleModelInfo*))(g_libGTASA + (VER_x32 ? 0x386E98 + 1 : 0x533338)))(&pInfo); // CBaseModelInfo::CBaseModelInfo();

    pInfo.vtable = g_libGTASA + (VER_x32 ? 0x006676A8 : 0x822910);

    ((void(*)(CVehicleModelInfo*))(*(uintptr_t*)(pInfo.vtable + (VER_x32 ? 0x1C : 0x1C*2))))(&pInfo);

    CModelInfo::SetModelInfo(index, &pInfo);
    return &pInfo;
}

CPedModelInfo* CModelInfo::AddPedModel(int index)
{

    auto& pInfo = CModelInfo::ms_pedModelInfoStore.AddItem();

    ((void(*)(CPedModelInfo*))(g_libGTASA + (VER_x32 ? 0x00384FD8 + 1 : 0x5304DC)))(&pInfo); // CBaseModelInfo::CBaseModelInfo();

    pInfo.vtable = g_libGTASA + (VER_x32 ? 0x00667658 : 0x822858);

    ((void(*)(CPedModelInfo*))(*(uintptr_t*)(pInfo.vtable + (VER_x32 ? 0x1C : 0x1C*2))))(&pInfo);

    CModelInfo::SetModelInfo(index, &pInfo);
    return &pInfo;
}

CDamageAtomicModelInfo* CModelInfo::AddDamageAtomicModel(int32 index)
{
    return CHook::CallFunction<CDamageAtomicModelInfo*>(g_libGTASA + (VER_x32 ? 0x00385F94 + 1 : 0x531B4C), index);
}

CAtomicModelInfo* CModelInfo::AddAtomicModel(int index)
{
    auto& pInfo = ms_atomicModelInfoStore.AddItem();

    ((void(*)(CAtomicModelInfo*))(g_libGTASA + (VER_x32 ? 0x00384FD8 + 1 : 0x5304DC)))(&pInfo);

    pInfo.vtable = g_libGTASA + (VER_x32 ? 0x00667444 : 0x822390);

    ((void(*)(CAtomicModelInfo*))(*(uintptr_t*)(pInfo.vtable + (VER_x32 ? 0x1C : 0x1C*2))))(&pInfo);

    CModelInfo::SetModelInfo(index, &pInfo);
    return &pInfo;
}

void CModelInfo::Initialise() {
    memset(ms_modelInfoPtrs, 0, sizeof(ms_modelInfoPtrs));

}

void CModelInfo::injectHooks()
{
    // These three used to "redirect" the engine's model info stores, but the
    // addresses are the GOT slots of the CStore<...> *destructors*
    // (_ZN6CStoreI16CAtomicModelInfoLi14000EED2Ev and the ped/vehicle ones),
    // so they only replaced a destructor pointer with a data pointer. The
    // engine's stores are file-local statics; the client gets its own through
    // the AddPedModel/AddVehicleModel redirects below. Removed.
    CHook::Write(g_libGTASA + (VER_x32 ? 0x6796CC : 0x8373A8), &CModelInfo::ms_modelInfoPtrs);

    CHook::Redirect("_ZN10CModelInfo11AddPedModelEi", &CModelInfo::AddPedModel);
    CHook::Redirect("_ZN10CModelInfo15AddVehicleModelEi", &CModelInfo::AddVehicleModel);
    CHook::Redirect("_ZN10CModelInfo14AddAtomicModelEi", &CModelInfo::AddAtomicModel);
}
