#include "playercoords.hpp"
#include "modules/ModuleRegistry.hpp"
#include <bedrocktools/events/EventBus.hpp>
#include <bedrocktools/sdk/world/Actor.hpp>
#include <bedrocktools/sdk/client/ClientInstance.hpp>
#include <pl/Logger.hpp>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>

static PlayerCoordsModule* g_coordsMod = nullptr;

// ---- dimension detection (0 unknown, 1 overworld, 2 nether, 3 end) ----
static int g_dim = 0;

static int detectDimension(bedrocktools::sdk::Player* player) {
    auto* dim = player->dimension();
    if (!dim) return 0;
    void** vtable = *reinterpret_cast<void***>(dim);
    if (!vtable) return 0;
    const char* typeinfo = reinterpret_cast<const char*>(vtable[-1]);
    if (!typeinfo) return 0;
    const char* name = *reinterpret_cast<const char* const*>(typeinfo + 8);
    if (!name) return 0;
    if (std::strstr(name, "Nether")) return 2;
    if (std::strstr(name, "TheEnd")) return 3;
    if (std::strstr(name, "Overworld")) return 1;
    return 0;
}

// ---- test: watch 5 candidate "isInServer" flags in MinecraftGame ----
struct GameFlag { const char* name; size_t off; int last; };
static GameFlag g_flags[] = {
    {"s50",  0x505,  -1}, {"s152", 0x502, -1}, {"s155", 0x358, -1},
    {"s267", 0x1ca8, -1}, {"s378", 0x1c68, -1},
};
static std::string g_flagText;

static void sampleGameFlags() {
    auto* ci = bedrocktools::sdk::ClientInstance::current();
    if (!ci) return;
    auto* game = ci->minecraftGame();
    if (!game) return;
    static auto& log = pl::log::Logger::getOrCreate("NetherCoords");
    std::string text;
    for (auto& f : g_flags) {
        int v = *reinterpret_cast<uint8_t*>(
            reinterpret_cast<uintptr_t>(game) + f.off);
        if (v != f.last) {
            log.info("{} (+{:#x}) = {}", f.name, f.off, v);
            f.last = v;
        }
        text += std::string(f.name) + ":" + std::to_string(v) + " ";
    }
    g_flagText = text;
}

static void s_coordsCallback(bedrocktools::sdk::Player* player) {
    if (!g_coordsMod || !g_coordsMod->enabled || !player) return;
    g_dim = detectDimension(player);
    sampleGameFlags();
    g_coordsMod->updateCoords(player->position());
}

static float calcTextWidth(const std::string& text, float size) {
    float width = 0;
    for (char c : text) {
        if (c == 'i' || c == 'l' || c == '1' || c == ':' || c == '.' || c == ' ') width += size * 0.3f;
        else if (c == 'm' || c == 'w' || c == 'M' || c == 'W') width += size * 0.8f;
        else width += size * 0.58f;
    }
    return width;
}


PlayerCoordsModule::PlayerCoordsModule()
    : Module("Player Coords", "Displays your XYZ coordinates on screen.") {
    m_currentPos = {0.f, 0.f, 0.f};
    g_coordsMod = this;
}

PlayerCoordsModule::~PlayerCoordsModule() {
    if (g_coordsMod == this) g_coordsMod = nullptr;
}


void PlayerCoordsModule::updateCoords(const bedrocktools::sdk::Vec3& pos) {
    m_currentPos = pos;
}

void PlayerCoordsModule::onInit() {
    bedrocktools::events::bus().subscribe<bedrocktools::events::LocalPlayerTickEvent>([](auto& event) { s_coordsCallback(event.player); });
}

void PlayerCoordsModule::onEnable() {
}

void PlayerCoordsModule::onDisable() {
    m_currentPos = {0.f, 0.f, 0.f};
}

void PlayerCoordsModule::onFrame() {
    if (!enabled) return;

    auto fl = [](float v) { return (long long)std::floor(v); };
    float px = m_currentPos.x, pz = m_currentPos.z;

    std::vector<std::string> lines;
    lines.push_back("X: " + std::to_string(fl(px)) +
                    " Y: " + std::to_string(fl(m_currentPos.y - 1.62f)) +
                    " Z: " + std::to_string(fl(pz)));
    if (m_showConversion) {
        if (g_dim == 2) {
            lines.push_back("Overworld: X " + std::to_string(fl(px * 8.0f)) +
                            " Z " + std::to_string(fl(pz * 8.0f)));
        } else if (g_dim == 1) {
            lines.push_back("Nether: X " + std::to_string(fl(px / 8.0f)) +
                            " Z " + std::to_string(fl(pz / 8.0f)));
        } else if (g_dim == 3) {
            lines.push_back("The End: no conversion");
        } else {
            lines.push_back("Dimension: unknown");
        }
    }
    if (!g_flagText.empty()) lines.push_back(g_flagText);

    float lineH = m_size + 8.0f;
    float boxW = 0.f;
    for (auto& l : lines) boxW = std::max(boxW, calcTextWidth(l, m_size));
    boxW += 12.0f;
    float boxH = lineH * lines.size();

    std::vector<PLModMenu_DrawCommand> cmds;
    if (m_background) {
        PLModMenu_DrawCommand bg = {};
        bg.type = PL_DRAW_RECT_FILLED;
        bg.x = hudPosX; bg.y = hudPosY; bg.w = boxW; bg.h = boxH;
        bg.color = ((int)(m_backgroundOpacity * 255.0f) << 24);
        cmds.push_back(bg);
    }
    for (size_t i = 0; i < lines.size(); ++i) {
        PLModMenu_DrawCommand t = {};
        t.type = PL_DRAW_TEXT;
        t.x = hudPosX; t.y = hudPosY + lineH * i;
        t.w = boxW; t.h = lineH;
        t.color = 0xFFFFFFFF;
        t.size = m_size;
        t.text = lines[i].c_str();
        cmds.push_back(t);
    }
    submitDrawCommands(moduleId, cmds);
}

void PlayerCoordsModule::loadConfig(const nlohmann::json& j) {
    Module::loadConfig(j);
    if (j.contains("hudPosX")) hudPosX = j["hudPosX"].get<float>();
    if (j.contains("hudPosY")) hudPosY = j["hudPosY"].get<float>();
    if (j.contains("isHudModule")) isHudModule = j["isHudModule"].get<bool>();
    if (j.contains("m_size")) m_size = j["m_size"].get<float>();
    if (j.contains("m_background")) m_background = j["m_background"].get<bool>();
    if (j.contains("m_backgroundOpacity")) m_backgroundOpacity = j["m_backgroundOpacity"].get<float>();
    if (j.contains("m_showConversion")) m_showConversion = j["m_showConversion"].get<bool>();
}

void PlayerCoordsModule::saveConfig(nlohmann::json& j) {
    Module::saveConfig(j);
    j["hudPosX"] = hudPosX;
    j["hudPosY"] = hudPosY;
    j["isHudModule"] = isHudModule;
    j["m_size"] = m_size;
    j["m_background"] = m_background;
    j["m_backgroundOpacity"] = m_backgroundOpacity;
    j["m_showConversion"] = m_showConversion;
}
