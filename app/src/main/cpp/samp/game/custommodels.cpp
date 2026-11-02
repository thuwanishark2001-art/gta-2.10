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
#include "pngdecode.h"
#include <dirent.h>
#include <unordered_set>

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

struct Tint { std::string key; uint8_t r, g, b; };   // key "" = every texture

struct CustomModel {
    bool loaded = false;      // files read and textures built (once)
    bool usable = false;
    std::vector<uint8_t> dff; // empty: the stock model (or the base model's)
    int base = -1;            // "base N": build this id from the game's model N
    std::vector<Tint> tints;
    bool inspect = false;     // "inspect": only log the texture names it uses
    std::unordered_map<std::string, void*> textures;  // Key(name) -> RwTexture*
    std::vector<std::string> seen;                    // texture names it asked for
    bool namesLogged = false;
};

// Bytes of stock models used as a "base", copied from the player's own game
// as they stream in (CustomModels_OnGameInit requests them).
std::unordered_map<int, std::vector<uint8_t>> g_baseBytes;
std::unordered_set<int> g_wantedBases;

void* (*kmRpClumpForAllAtomics)(void*, void* (*)(void*, void*), void*);
void* (*kmRpGeometryForAllMaterials)(void*, void* (*)(void*, void*), void*);
void* (*RpClumpStreamRead_orig)(void*);
void (*kmRequestModel)(int, int);
void (*kmLoadAllRequestedModels)(bool);
void (*kmRemoveModel)(int);
void (*kmSetModelIsDeletable)(int);

std::unordered_map<unsigned int, CustomModel> g_models;
CustomModel* g_current = nullptr;   // the model being loaded, for FindTexture
FindCB g_gameFind = nullptr;

// Texture names are matched loosely, the same way scripts/add-skin.sh names
// the files: lower case, a trailing " (n)" from repeat downloads dropped, and
// only letters and digits kept. So texture "Sneakerprowht (5)" is file
// sneakerprowht.png, and "23 1" is 231.png.
std::string Key(const char* s)
{
    std::string in(s ? s : "");
    size_t end = in.size();
    if (end && in[end - 1] == ')') {
        size_t open = in.rfind('(');
        bool digits = open != std::string::npos && open + 1 < end - 1;
        for (size_t i = open + 1; digits && i < end - 1; i++) digits = isdigit((unsigned char) in[i]) != 0;
        if (digits) {
            end = open;
            while (end && in[end - 1] == ' ') end--;
        }
    }
    std::string out;
    for (size_t i = 0; i < end; i++) {
        unsigned char c = (unsigned char) in[i];
        if (isalnum(c)) out += (char) tolower(c);
    }
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
        if (void* t = MakeTexture(name, w, h, rgba)) model.textures[Key(name.c_str())] = t;
        else FLog("CustomModels: texture %s couldn't be created", name.c_str());
    }, [](const std::string& name) {
        FLog("CustomModels: skipped texture %s (unsupported format)", name.c_str());
    });
}

// Every <name>.png in dir becomes texture <name> (if the model has no
// texture by that name yet).
int LoadPngDir(const std::string& dir, CustomModel& model)
{
    DIR* d = opendir(dir.c_str());
    if (!d) return 0;
    int made = 0;
    while (dirent* e = readdir(d)) {
        std::string file = e->d_name;
        if (file.size() < 5 || file[0] == '.' || strcasecmp(file.c_str() + file.size() - 4, ".png") != 0) continue;
        std::string name = file.substr(0, file.size() - 4);
        if (model.textures.count(Key(name.c_str()))) continue;
        std::vector<uint8_t> png, rgba;
        int w = 0, h = 0;
        if (!ReadFile(dir + "/" + file, png, 16u << 20) || !kmpng::Decode(png.data(), png.size(), w, h, rgba)) {
            FLog("CustomModels: can't read %s/%s (not a PNG this client reads)", dir.c_str(), file.c_str());
            continue;
        }
        if (void* t = MakeTexture(name, w, h, rgba)) {
            model.textures[Key(name.c_str())] = t;
            made++;
        }
    }
    closedir(d);
    return made;
}

bool IsSafeId(unsigned int id) { return id > 0 && id < 20000; }

// models/<id>.cfg, one setting per line ('#' starts a comment):
//   base 21              build this id from the game's own model 21
//   tint * C0392B        colour every texture (RRGGBB)
//   tint shirt 1E40AF    colour only the texture named "shirt"
//   inspect              change nothing; log the texture names the model uses
void ReadCfg(const std::string& path, CustomModel& m, unsigned int modelId)
{
    std::vector<uint8_t> raw;
    if (!ReadFile(path, raw, 64 * 1024)) return;
    std::string text(raw.begin(), raw.end());
    size_t pos = 0;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        std::string line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = nl == std::string::npos ? text.size() : nl + 1;
        size_t hash = line.find('#');
        if (hash != std::string::npos) line.resize(hash);
        char word[16] = {}, arg1[64] = {}, arg2[16] = {};
        int n = sscanf(line.c_str(), "%15s %63s %15s", word, arg1, arg2);
        if (n <= 0) continue;
        if (!strcasecmp(word, "base") && n >= 2) {
            int base = atoi(arg1);
            if (IsSafeId((unsigned) base) && (unsigned) base != modelId) m.base = base;
            else FLog("CustomModels: model %u: bad base \"%s\"", modelId, arg1);
        } else if (!strcasecmp(word, "inspect")) {
            m.inspect = true;
        } else if (!strcasecmp(word, "tint") && n >= 3) {
            unsigned rgb = 0;
            if (strlen(arg2) != 6 || sscanf(arg2, "%6x", &rgb) != 1) {
                FLog("CustomModels: model %u: bad tint colour \"%s\" (use RRGGBB)", modelId, arg2);
                continue;
            }
            Tint t{strcmp(arg1, "*") ? Key(arg1) : std::string(), (uint8_t) (rgb >> 16), (uint8_t) (rgb >> 8), (uint8_t) rgb};
            m.tints.push_back(t);
        } else {
            FLog("CustomModels: model %u: unknown setting \"%s\"", modelId, line.c_str());
        }
    }
}

CustomModel* Find(unsigned int modelId)
{
    if (!IsSafeId(modelId) || !g_pszStorage) return nullptr;
    auto it = g_models.find(modelId);
    if (it != g_models.end()) return it->second.usable ? &it->second : nullptr;

    CustomModel& m = g_models[modelId];
    m.loaded = true;
    std::string base = std::string(g_pszStorage) + "content/models/" + std::to_string(modelId);
    ReadCfg(base + ".cfg", m, modelId);
    ReadFile(base + ".dff", m.dff, 50u << 20);
    std::vector<uint8_t> txd;
    if (ReadFile(base + ".txd", txd, 50u << 20)) {
        int n = LoadTxd(txd, m);
        FLog("CustomModels: model %u: %d textures from %u.txd", modelId, n, modelId);
    }
    // PNG textures: the model's own folder, then (only for custom ids) the
    // folder all custom models share.
    int own = LoadPngDir(base, m);
    m.usable = !m.dff.empty() || m.base >= 0 || own > 0 || !m.textures.empty() || !m.tints.empty() || m.inspect;
    if (!m.usable) return nullptr;
    int shared = LoadPngDir(std::string(g_pszStorage) + "content/models/textures", m);
    if (own || shared) FLog("CustomModels: model %u: %d PNG textures from models/%u/, %d from models/textures/", modelId, own, modelId, shared);
    return &m;
}

void Seen(const char* name)
{
    if (!g_current || !name) return;
    for (auto& n : g_current->seen) if (!strcasecmp(n.c_str(), name)) return;
    if (g_current->seen.size() < 64) g_current->seen.push_back(name);
}

void* FindTexture(const char* name)
{
    Seen(name);
    if (g_current && name) {
        auto it = g_current->textures.find(Key(name));
        if (it != g_current->textures.end()) return it->second;
    }
    void* found = g_gameFind ? g_gameFind(name) : nullptr;
    // Tells the server owner which texture file a custom model is missing.
    if (!found && g_current && name) FLog("CustomModels: the model wants texture \"%s\", which isn't in its files", name);
    return found;
}

// Vehicles: LoadClumpFile swaps in CVehicleModelInfo::FindTextureCB, which
// only searches the vehicle texture database and doesn't chain to the
// previous callback, so a custom vehicle's own textures are served here.
void* (*VehicleFindTextureCB_orig)(const char*);
void* VehicleFindTextureCB_hook(const char* name)
{
    Seen(name);
    if (g_current && name) {
        auto it = g_current->textures.find(Key(name));
        if (it != g_current->textures.end()) return it->second;
    }
    void* found = VehicleFindTextureCB_orig(name);
    if (!found && g_current && name) FLog("CustomModels: the vehicle wants texture \"%s\", which isn't in its files", name);
    return found;
}

// Tints: RpMaterial colour (+0x8, RwRGBA) multiplies its texture once the
// geometry has rpGEOMETRYMODULATEMATERIALCOLOR (0x40) in its flags (+0x10).
// RpAtomic's geometry is at +0x30; RpMaterial's texture at +0x0, and a
// texture's name at +0x20.
constexpr size_t kAtomicGeometry = 0x30, kGeometryFlags = 0x10, kMaterialColour = 0x8, kTextureName = 0x20;
constexpr uint32_t kModulateMaterialColour = 0x40;

void* TintMaterial(void* material, void* data)
{
    auto* m = (CustomModel*) data;
    void* texture = *(void**) material;
    std::string key = texture ? Key((const char*) texture + kTextureName) : std::string();
    const Tint* use = nullptr;
    for (auto& t : m->tints) {
        if (t.key == key) { use = &t; break; }      // a named tint wins over "*"
        if (t.key.empty() && !use) use = &t;
    }
    if (use) {
        uint8_t* c = (uint8_t*) material + kMaterialColour;
        c[0] = use->r; c[1] = use->g; c[2] = use->b; c[3] = 255;
    }
    return material;
}

void* TintAtomic(void* atomic, void* data)
{
    void* geometry = *(void**) ((uint8_t*) atomic + kAtomicGeometry);
    if (geometry) {
        *(uint32_t*) ((uint8_t*) geometry + kGeometryFlags) |= kModulateMaterialColour;
        kmRpGeometryForAllMaterials(geometry, &TintMaterial, data);
    }
    return atomic;
}

void* RpClumpStreamRead_hook(void* stream)
{
    void* clump = RpClumpStreamRead_orig(stream);
    if (clump && g_current && !g_current->tints.empty()) kmRpClumpForAllAtomics(clump, &TintAtomic, g_current);
    return clump;
}

// A memory RwStream (type 3): its data is at +0x18, its size at +0x14.
void CaptureBase(void* stream, unsigned int modelId)
{
    if (!g_wantedBases.count((int) modelId) || g_baseBytes.count((int) modelId) || !stream) return;
    auto* s = (uint8_t*) stream;
    if (*(int32_t*) s != kStreamMemory) return;
    uint32_t size = *(uint32_t*) (s + 0x14);
    const uint8_t* data = *(const uint8_t**) (s + 0x18);
    if (!data || size < 12 || size > (50u << 20)) return;
    uint32_t firstChunk;
    memcpy(&firstChunk, data, 4);
    if (firstChunk != 0x10) {
        FLog("CustomModels: game model %u can't be a base (its data starts with chunk 0x%x)", modelId, firstChunk);
        return;
    }
    g_baseBytes[(int) modelId].assign(data, data + size);
    FLog("CustomModels: kept game model %u (%u bytes) as a base", modelId, size);
}

bool LoadClumpFile_hook(void* stream, unsigned int modelId)
{
    CaptureBase(stream, modelId);
    CustomModel* m = Find(modelId);
    if (!m) return LoadClumpFile_orig(stream, modelId);

    // Where the model comes from: its own .dff, the base model, or the stock one.
    const std::vector<uint8_t>* bytes = nullptr;
    if (!m->dff.empty()) bytes = &m->dff;
    else if (m->base >= 0) {
        auto it = g_baseBytes.find(m->base);
        if (it != g_baseBytes.end()) bytes = &it->second;
        else FLog("CustomModels: model %u: base model %d isn't loaded yet; using the stock model", modelId, m->base);
    }
    KmRwMemory mem{bytes ? const_cast<uint8_t*>(bytes->data()) : nullptr, bytes ? (uint32_t) bytes->size() : 0};
    void* custom = bytes ? kmRwStreamOpen(kStreamMemory, kStreamRead, &mem) : nullptr;
    if (bytes && !custom) return LoadClumpFile_orig(stream, modelId);

    g_gameFind = kmRwTextureGetFindCallBack();
    g_current = m;
    kmRwTextureSetFindCallBack(&FindTexture);
    bool ok = LoadClumpFile_orig(custom ? custom : stream, modelId);
    kmRwTextureSetFindCallBack(g_gameFind);
    g_current = nullptr;
    if (custom) kmRwStreamClose(custom, &mem);

    if (!m->namesLogged && !m->seen.empty()) {
        // So the server owner knows which texture names to draw or tint.
        std::string names;
        for (auto& n : m->seen) names += (names.empty() ? "" : ", ") + n;
        FLog("CustomModels: model %u uses textures: %s", modelId, names.c_str());
        m->namesLogged = true;
    }
    if (!custom) {
        FLog("CustomModels: model %u: game model with %zu custom textures, %zu tints", modelId, m->textures.size(), m->tints.size());
        return ok;
    }
    if (ok) {
        if (m->dff.empty()) FLog("CustomModels: model %u built from game model %d", modelId, m->base);
        else FLog("CustomModels: model %u loaded from content/models/%u.dff", modelId, modelId);
        return true;
    }
    // A broken .dff: keep the stock model, and don't try again.
    FLog("CustomModels: model %u: its custom model didn't load; using the stock model", modelId);
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
    CHook::InlineHook("_ZN17CVehicleModelInfo13FindTextureCBEPKc", &VehicleFindTextureCB_hook, &VehicleFindTextureCB_orig);
    Sym(kmRpClumpForAllAtomics, "_Z20RpClumpForAllAtomicsP7RpClumpPFP8RpAtomicS2_PvES3_");
    Sym(kmRpGeometryForAllMaterials, "_Z25RpGeometryForAllMaterialsP10RpGeometryPFP10RpMaterialS2_PvES3_");
    Sym(kmRequestModel, "_ZN10CStreaming12RequestModelEii");
    Sym(kmLoadAllRequestedModels, "_ZN10CStreaming22LoadAllRequestedModelsEb");
    Sym(kmRemoveModel, "_ZN10CStreaming11RemoveModelEi");
    Sym(kmSetModelIsDeletable, "_ZN10CStreaming19SetModelIsDeletableEi");
    CHook::InlineHook("_Z17RpClumpStreamReadP8RwStream", &RpClumpStreamRead_hook, &RpClumpStreamRead_orig);
    FLog("CustomModels: hooks installed");
}

// After CGame::Initialise: load each "base" game model once, so its data can
// be kept for the custom ids built from it.
void CustomModels_OnGameInit()
{
    if (!g_pszStorage) return;
    std::string dir = std::string(g_pszStorage) + "content/models";
    DIR* d = opendir(dir.c_str());
    if (!d) return;
    while (dirent* e = readdir(d)) {
        std::string file = e->d_name;
        if (file.size() < 5 || strcasecmp(file.c_str() + file.size() - 4, ".cfg") != 0) continue;
        unsigned id = (unsigned) atoi(file.c_str());
        if (!IsSafeId(id)) continue;
        CustomModel probe;
        ReadCfg(dir + "/" + file, probe, id);
        if (probe.base >= 0) g_wantedBases.insert(probe.base);
    }
    closedir(d);
    for (int base : g_wantedBases) {
        if (g_baseBytes.count(base)) continue;
        kmRemoveModel(base);           // so it streams in again and gets kept
        kmRequestModel(base, 2);
        kmLoadAllRequestedModels(false);
        kmSetModelIsDeletable(base);
        if (!g_baseBytes.count(base)) FLog("CustomModels: game model %d couldn't be kept as a base", base);
    }
}
