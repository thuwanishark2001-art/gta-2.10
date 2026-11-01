//
// Created by x1y2z on 07.03.2023.
//

#pragma once

#include "PedModelInfo.h"
#include "AtomicModelInfo.h"
#include "VehicleModelInfo.h"
#include "game/Core/Store.h"
#include "game/constants.h"


class CModelInfo {
public:
    static constexpr int32 NUM_MODEL_INFOS = TOTAL_DFF_MODEL_IDS;
#if KM_GTA211
    // 2.11 is built with LTO and reads its model table directly (adrp), not
    // through the GOT slot 2.10 used, so swapping that slot left the client
    // with a second, mostly empty table. Use the engine's own table instead
    // (same size, 20000 entries); set in injectHooks().
    static inline CBaseModelInfo** ms_modelInfoPtrs = nullptr;
#else
    static inline CBaseModelInfo* ms_modelInfoPtrs[NUM_MODEL_INFOS];
#endif

    static void injectHooks();

    static constexpr int32_t NUM_PED_MODEL_INFOS = 315;
    static inline CStore<CPedModelInfo, NUM_PED_MODEL_INFOS> ms_pedModelInfoStore;

    static inline CStore<CAtomicModelInfo, 20000> ms_atomicModelInfoStore;

    static constexpr int32 NUM_VEHICLE_MODEL_INFOS = 270;
    static inline CStore<CVehicleModelInfo, NUM_VEHICLE_MODEL_INFOS> ms_vehicleModelInfoStore;

    static CPedModelInfo *AddPedModel(int index);

    static CAtomicModelInfo *AddAtomicModel(int index);
    static CDamageAtomicModelInfo* AddDamageAtomicModel(int32 index);
    static CVehicleModelInfo *AddVehicleModel(int index);

    static CBaseModelInfo* GetModelInfo(int index) { return ms_modelInfoPtrs[index]; }
    static auto GetPedModelInfo(int32_t index) { return GetModelInfo(index)->AsPedModelInfoPtr(); }
    static auto GetVehicleModelInfo(int32_t index) { return GetModelInfo(index)->AsVehicleModelInfoPtr(); }
    static void SetModelInfo(int index, CBaseModelInfo* pInfo) { ms_modelInfoPtrs[index] = pInfo; }

    static void Initialise();
};

