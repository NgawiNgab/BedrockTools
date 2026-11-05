#include "playercoords.hpp"
#include "modules/ModuleRegistry.hpp"
#include <bedrocktools/events/EventBus.hpp>
#include <bedrocktools/sdk/world/Actor.hpp>
#include <cmath>
#include <algorithm>

static PlayerCoordsModule* g_coordsMod = nullptr;

static void s_coordsCallback(bedrocktools::sdk::Player* player) {
    if (!g_coordsMod || !g_coordsMod->enabled || !player) return;
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
    lines.push_back("X: " + std::to_string((int)px) +
                    " Y: " + std::to_string((int)m_currentPos.y) +
                    " Z: " + std::to_string((int)pz));
    if (m_showConversion) {
        lines.push_back("Nether: X " + std::to_string(fl(px / 8.0f)) +
                        " Z " + std::to_string(fl(pz / 8.0f)));
        lines.push_back("Overworld: X " + std::to_string(fl(px * 8.0f)) +
                        " Z " + std::to_string(fl(pz * 8.0f)));
    }

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
    PLModMenu_DrawCommand txtCmd = {};
    txtCmd.type = PL_DRAW_TEXT;
    txtCmd.x = boxX;
    txtCmd.y = boxY;
    txtCmd.w = boxW;
    txtCmd.h = boxH;
    txtCmd.color = 0xFFFFFFFF; 
    txtCmd.size = m_size;
    txtCmd.text = text.c_str();
    cmds.push_back(txtCmd);

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
}

void PlayerCoordsModule::saveConfig(nlohmann::json& j) {
    Module::saveConfig(j);
    j["hudPosX"] = hudPosX;
    j["hudPosY"] = hudPosY;
    j["isHudModule"] = isHudModule;
    j["m_size"] = m_size;
    j["m_background"] = m_background;
    j["m_backgroundOpacity"] = m_backgroundOpacity;
}
