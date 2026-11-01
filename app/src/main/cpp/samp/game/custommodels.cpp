// Custom models from the server's content (docs/CONTENT.md): a skin or vehicle
// model the launcher downloaded replaces a stock model id.
//
//   content/models/<id>.dff   the model (a PC .dff)
//   content/models/<id>.txd   its textures (a PC .txd: D3D8/D3D9, DXT1/3/5 or
//                             uncompressed); optional if the .dff only uses
//                             textures the game already has
//
// The engine streams each model through CFileLoader::LoadClumpFile(stream, id).
// The hook gives it the custom .dff instead, while a texture find callback
// serves the custom .txd's textures by name (falling back to the game's).
// The mobile engine can't read PC texture formats, so the .txd is decoded
// here to RGBA and uploaded through RwImage -> RwRaster.

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <sys/stat.h>
#include <strings.h>

#include "../main.h"
#include "../log.h"
#include "../vendor/armhook/patch.h"
#include "custommodels.h"
#include "txddecode.h"

namespace {

// RenderWare as the 2.11 engine lays it out (checked against libGame.so:
// kmRwTextureCreate/kmRwTextureSetName, kmRwImageCreate/kmRwImageAllocatePixels).
struct KmRwMemory { uint8_t* start; uint32_t length; };
struct KmRwImage { int32_t flags, width, height, depth, stride; int32_t pad; uint8_t* pixels; void* palette; };
static_assert(offsetof(KmRwImage, pixels) == 0x18, "RwImage layout");

constexpr int kStreamMemory = 3;   // rwSTREAMMEMORY
constexpr int kStreamRead = 1;     // rwSTREAMREAD
constexpr int kRasterTypeTexture = 4;
constexpr int kTextureNameOffset = 0x20;

using FindCB = void* (*)(const char*);

void* (*kmRwStreamOpen)(int, int, const void*);
int (*kmRwStreamClose)(void*, void*);
void* (*kmRwImageCreate)(int, int, int);
void* (*kmRwImageAllocatePixels)(void*);
int (*kmRwImageDestroy)(void*);
void* (*kmRwImageFindRasterFormat)(void*, int, int*, int*, int*, int*);
void* (*kmRwRasterCreate)(int, int, int, int);
void* (*kmRwRasterSetFromImage)(void*, void*);
int (*kmRwRasterDestroy)(void*);
void* (*kmRwTextureCreate)(void*);
void* (*kmRwTextureSetName)(void*, const char*);
FindCB (*kmRwTextureGetFindCallBack)();
void* (*kmRwTextureSetFindCallBack)(FindCB);

bool (*LoadClumpFile_orig)(void* stream, unsigned int modelId);

struct CustomModel {
    bool loaded = false;      // files read and textures built (once)
    bool usable = false;
    std::vector<uint8_t> dff;
    std::unordered_map<std::string, void*> textures;  // lower-case name -> RwTexture*
};

std::unordered_map<unsigned int, CustomModel> g_models;
CustomModel* g_current = nullptr;   // the model being loaded, for FindTexture
FindCB g_gameFind = nullptr;

std::string Lower(const char* s)
{
    std::string out(s);
    for (auto& c : out) c = (char) tolower((unsigned char) c);
    return out;
}

bool ReadFile(const std::string& path, std::vector<uint8_t>& out, size_t maxBytes)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0 || (size_t) st.st_size > maxBytes) return false;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    out.resize((size_t) st.st_size);
    bool ok = fread(out.data(), 1, out.size(), f) == out.size();
    fclose(f);
    return ok;
}

void* MakeTexture(const std::string& name, int w, int h, const std::vector<uint8_t>& rgba)
{
    auto* image = (KmRwImage*) kmRwImageCreate(w, h, 32);
    if (!image) return nullptr;
    if (!kmRwImageAllocatePixels(image) || !image->pixels) { kmRwImageDestroy(image); return nullptr; }
    for (int y = 0; y < h; y++) memcpy(image->pixels + (size_t) y * image->stride, &rgba[(size_t) y * w * 4], (size_t) w * 4);
    int rw, rh, rd, rf;
    kmRwImageFindRasterFormat(image, kRasterTypeTexture, &rw, &rh, &rd, &rf);
    void* raster = kmRwRasterCreate(rw, rh, rd, rf);
    void* texture = nullptr;
    if (raster && kmRwRasterSetFromImage(raster, image)) {
        texture = kmRwTextureCreate(raster);
        if (texture) kmRwTextureSetName(texture, name.c_str());
        else kmRwRasterDestroy(raster);
    } else if (raster) {
        kmRwRasterDestroy(raster);
    }
    kmRwImageDestroy(image);
    return texture;
}

int LoadTxd(const std::vector<uint8_t>& file, CustomModel& model)
{
    return kmtxd::ForEachTexture(file.data(), file.size(), [&](const std::string& name, int w, int h, const std::vector<uint8_t>& rgba) {
        if (void* t = MakeTexture(name, w, h, rgba)) model.textures[Lower(name.c_str())] = t;
        else FLog("CustomModels: texture %s couldn't be created", name.c_str());
    }, [](const std::string& name) {
        FLog("CustomModels: skipped texture %s (unsupported format)", name.c_str());
    });
}

bool IsSafeId(unsigned int id) { return id > 0 && id < 20000; }

CustomModel* Find(unsigned int modelId)
{
    if (!IsSafeId(modelId) || !g_pszStorage) return nullptr;
    auto it = g_models.find(modelId);
    if (it != g_models.end()) return it->second.usable ? &it->second : nullptr;

    CustomModel& m = g_models[modelId];
    m.loaded = true;
    std::string base = std::string(g_pszStorage) + "content/models/" + std::to_string(modelId);
    if (!ReadFile(base + ".dff", m.dff, 50u << 20)) return nullptr;
    std::vector<uint8_t> txd;
    if (ReadFile(base + ".txd", txd, 50u << 20)) {
        int n = LoadTxd(txd, m);
        FLog("CustomModels: model %u: %d textures from %u.txd", modelId, n, modelId);
    }
    m.usable = true;
    return &m;
}

void* FindTexture(const char* name)
{
    if (g_current && name) {
        auto it = g_current->textures.find(Lower(name));
        if (it != g_current->textures.end()) return it->second;
    }
    return g_gameFind ? g_gameFind(name) : nullptr;
}

bool LoadClumpFile_hook(void* stream, unsigned int modelId)
{
    CustomModel* m = Find(modelId);
    if (!m) return LoadClumpFile_orig(stream, modelId);

    KmRwMemory mem{m->dff.data(), (uint32_t) m->dff.size()};
    void* custom = kmRwStreamOpen(kStreamMemory, kStreamRead, &mem);
    if (!custom) return LoadClumpFile_orig(stream, modelId);

    g_gameFind = kmRwTextureGetFindCallBack();
    g_current = m;
    kmRwTextureSetFindCallBack(&FindTexture);
    bool ok = LoadClumpFile_orig(custom, modelId);
    kmRwTextureSetFindCallBack(g_gameFind);
    g_current = nullptr;
    kmRwStreamClose(custom, &mem);

    if (ok) {
        FLog("CustomModels: model %u loaded from content/models/%u.dff", modelId, modelId);
        return true;
    }
    // A broken .dff: keep the stock model, and don't try again.
    FLog("CustomModels: model %u: %u.dff didn't load; using the stock model", modelId, modelId);
    m->usable = false;
    return LoadClumpFile_orig(stream, modelId);
}

template <typename T>
void Sym(T& fn, const char* name) { fn = reinterpret_cast<T>(CHook::getSym(name)); }

} // namespace

void CustomModels_InstallHooks()
{
    Sym(kmRwStreamOpen, "_Z12RwStreamOpen12RwStreamType18RwStreamAccessTypePKv");
    Sym(kmRwStreamClose, "_Z13RwStreamCloseP8RwStreamPv");
    Sym(kmRwImageCreate, "_Z13RwImageCreateiii");
    Sym(kmRwImageAllocatePixels, "_Z21RwImageAllocatePixelsP7RwImage");
    Sym(kmRwImageDestroy, "_Z14RwImageDestroyP7RwImage");
    Sym(kmRwImageFindRasterFormat, "_Z23RwImageFindRasterFormatP7RwImageiPiS1_S1_S1_");
    Sym(kmRwRasterCreate, "_Z14RwRasterCreateiiii");
    Sym(kmRwRasterSetFromImage, "_Z20RwRasterSetFromImageP8RwRasterP7RwImage");
    Sym(kmRwRasterDestroy, "_Z15RwRasterDestroyP8RwRaster");
    Sym(kmRwTextureCreate, "_Z15RwTextureCreateP8RwRaster");
    Sym(kmRwTextureSetName, "_Z16RwTextureSetNameP9RwTexturePKc");
    Sym(kmRwTextureGetFindCallBack, "_Z24RwTextureGetFindCallBackv");
    Sym(kmRwTextureSetFindCallBack, "_Z24RwTextureSetFindCallBackPFP9RwTexturePKcE");
    CHook::InlineHook("_ZN11CFileLoader13LoadClumpFileEP8RwStreamj", &LoadClumpFile_hook, &LoadClumpFile_orig);
    FLog("CustomModels: hooks installed");
}
