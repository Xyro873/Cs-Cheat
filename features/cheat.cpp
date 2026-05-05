#include "pch.h"
#include "cheat.h"

#include <Windows.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>

#include "imgui.h"
#include "../sdk/offsets.h"

namespace cheat {

Settings g_Settings;

static constexpr float PI = 3.14159265358979323846f;
static constexpr std::ptrdiff_t kEntityListStride = 0x70;
static constexpr std::ptrdiff_t kEntityListFirstEntry = 0x10;
static constexpr std::ptrdiff_t kGameSceneNodeOffset = 0x330;
static constexpr std::ptrdiff_t kSceneNodeAbsOrigin = 0xC8;
static constexpr std::ptrdiff_t kSceneNodeDormant = 0x103;
static constexpr std::ptrdiff_t kPawnViewOffset = 0xE70;
static constexpr std::ptrdiff_t kControllerPlayerName = 0x6F0;
static constexpr std::ptrdiff_t kControllerPlayerPawn = 0x904;
static constexpr std::ptrdiff_t kControllerPlayerTeam = 0x3EB;
static constexpr std::ptrdiff_t kControllerHealth = 0x34C;

struct Entity {
    uintptr_t controller = 0;
    uintptr_t pawn = 0;
    int health = 0;
    int team = 0;
    bool valid = false;
    Vector3 origin{};
    Vector3 head{};
    Vector3 chest{};
    Vector3 screenHead{};
    Vector3 screenOrigin{};
    char name[128] = "";
};

static uintptr_t GetClientBase() {
    static uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleA("client.dll"));
    return base;
}

static bool WorldToScreen(const Vector3& world, Vector3& screen, const view_matrix_t& vm, int width, int height) {
    float clipX = world.x * vm.m[0][0] + world.y * vm.m[1][0] + world.z * vm.m[2][0] + vm.m[3][0];
    float clipY = world.x * vm.m[0][1] + world.y * vm.m[1][1] + world.z * vm.m[2][1] + vm.m[3][1];
    float clipW = world.x * vm.m[0][3] + world.y * vm.m[1][3] + world.z * vm.m[2][3] + vm.m[3][3];

    if (clipW < 0.001f)
        return false;

    float invW = 1.0f / clipW;
    screen.x = (width * 0.5f) + (clipX * invW) * (width * 0.5f);
    screen.y = (height * 0.5f) - (clipY * invW) * (height * 0.5f);
    screen.z = clipW;
    return true;
}

static Vector3 CalcAngle(const Vector3& src, const Vector3& dst) {
    Vector3 delta = { dst.x - src.x, dst.y - src.y, dst.z - src.z };
    float hyp = std::sqrtf(delta.x * delta.x + delta.y * delta.y);
    Vector3 angles;
    angles.x = std::atan2f(-delta.z, hyp) * (180.0f / PI);
    angles.y = std::atan2f(delta.y, delta.x) * (180.0f / PI);
    angles.z = 0.0f;
    return angles;
}

static float AngleDistance(const Vector3& a, const Vector3& b) {
    Vector3 delta = { NormalizePitch(a.x - b.x), NormalizeYaw(a.y - b.y), 0.0f };
    return std::sqrtf(delta.x * delta.x + delta.y * delta.y);
}

static bool GetSceneNodeOrigin(uintptr_t entity, Vector3& outOrigin) {
    if (!entity)
        return false;

    uintptr_t node = *reinterpret_cast<uintptr_t*>(entity + kGameSceneNodeOffset);
    if (!node)
        return false;

    if (*reinterpret_cast<uint8_t*>(node + kSceneNodeDormant))
        return false;

    outOrigin = *reinterpret_cast<Vector3*>(node + kSceneNodeAbsOrigin);
    return true;
}

static bool GetLocalPawn(uintptr_t& pawn) {
    pawn = *reinterpret_cast<uintptr_t*>(GetClientBase() + cs2_dumper::offsets::client_dll::dwLocalPlayerPawn);
    return pawn != 0;
}

static bool GetLocalController(uintptr_t& controller) {
    controller = *reinterpret_cast<uintptr_t*>(GetClientBase() + cs2_dumper::offsets::client_dll::dwLocalPlayerController);
    return controller != 0;
}

static bool GetPawnFromController(uintptr_t controller, uintptr_t& pawn) {
    if (!controller)
        return false;

    uint32_t pawnHandle = *reinterpret_cast<uint32_t*>(controller + kControllerPlayerPawn);
    if (!pawnHandle)
        return false;

    uintptr_t entityList = *reinterpret_cast<uintptr_t*>(GetClientBase() + cs2_dumper::offsets::client_dll::dwEntityList);
    if (!entityList)
        return false;

    uintptr_t pawnEntry = *reinterpret_cast<uintptr_t*>(entityList + kEntityListFirstEntry + 8 * ((pawnHandle & 0x7FFF) >> 9));
    if (!pawnEntry)
        return false;

    pawn = *reinterpret_cast<uintptr_t*>(pawnEntry + kEntityListStride * (pawnHandle & 0x1FF));
    return pawn != 0;
}

static bool IsValidEnemy(uintptr_t pawn, int localTeam, Entity& outEntity) {
    if (!pawn)
        return false;

    int health = *reinterpret_cast<int*>(pawn + kControllerHealth);
    if (health <= 0)
        return false;

    int team = *reinterpret_cast<uint8_t*>(pawn + kControllerPlayerTeam);
    if (team == 0 || team == localTeam)
        return false;

    Vector3 origin;
    if (!GetSceneNodeOrigin(pawn, origin))
        return false;

    outEntity.origin = origin;
    outEntity.health = health;
    outEntity.team = team;
    outEntity.valid = true;
    return true;
}

static std::string ReadPlayerName(uintptr_t controller) {
    std::string name;
    if (!controller)
        return name;

    char buffer[128] = { 0 };
    std::memcpy(buffer, reinterpret_cast<void*>(controller + kControllerPlayerName), sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    name = buffer;
    return name;
}

static Entity BuildEntity(uintptr_t controller, uintptr_t pawn, int localTeam) {
    Entity entity;
    entity.controller = controller;
    entity.pawn = pawn;
    entity.valid = false;
    if (!IsValidEnemy(pawn, localTeam, entity))
        return entity;

    std::string playerName = ReadPlayerName(controller);
    std::memcpy(entity.name, playerName.c_str(), std::min<size_t>(playerName.size(), sizeof(entity.name) - 1));

    Vector3 head = entity.origin;
    Vector3 chest = entity.origin;
    head.z += 64.0f;
    chest.z += 36.0f;
    entity.head = head;
    entity.chest = chest;
    return entity;
}

static bool FindBestTarget(const std::vector<Entity>& entities, const Vector3& eyePos, const Vector3& currentAngles, Entity& bestTarget) {
    float bestFov = g_Settings.aimbotFOV;
    bool found = false;

    for (const Entity& enemy : entities) {
        if (!enemy.valid)
            continue;

        Vector3 targetPos = g_Settings.targetHead ? enemy.head : enemy.chest;
        Vector3 targetAngles = CalcAngle(eyePos, targetPos);
        float fov = AngleDistance(currentAngles, targetAngles);
        if (fov < bestFov) {
            bestFov = fov;
            bestTarget = enemy;
            found = true;
        }
    }

    return found;
}

static void AimAtTarget(const Entity& target, const Vector3& eyePos, Vector3& currentAngles) {
    Vector3 targetPos = g_Settings.targetHead ? target.head : target.chest;
    Vector3 desiredAngles = CalcAngle(eyePos, targetPos);
    Vector3 delta = { NormalizePitch(desiredAngles.x - currentAngles.x), NormalizeYaw(desiredAngles.y - currentAngles.y), 0.0f };
    if (g_Settings.aimbotSmooth > 1.0f) {
        delta.x /= g_Settings.aimbotSmooth;
        delta.y /= g_Settings.aimbotSmooth;
    }
    Vector3 nextAngles = { currentAngles.x + delta.x, currentAngles.y + delta.y, 0.0f };
    nextAngles.x = NormalizePitch(nextAngles.x);
    nextAngles.y = NormalizeYaw(nextAngles.y);

    float* viewAngles = reinterpret_cast<float*>(GetClientBase() + cs2_dumper::offsets::client_dll::dwViewAngles);
    if (viewAngles) {
        viewAngles[0] = nextAngles.x;
        viewAngles[1] = nextAngles.y;
    }
}

static bool IsCrosshairOnTarget(const Entity& target, const view_matrix_t& viewMatrix, int width, int height) {
    Vector3 screenPos;
    Vector3 targetPos = g_Settings.targetHead ? target.head : target.chest;
    if (!WorldToScreen(targetPos, screenPos, viewMatrix, width, height))
        return false;

    float centerX = width * 0.5f;
    float centerY = height * 0.5f;
    float dx = screenPos.x - centerX;
    float dy = screenPos.y - centerY;
    float distance = std::sqrtf(dx * dx + dy * dy);
    return distance < 25.0f;
}

static void DrawAllEntities(const std::vector<Entity>& entities, const view_matrix_t& viewMatrix, int width, int height) {
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    for (const auto& entity : entities) {
        if (!entity.valid)
            continue;

        Vector3 screenHead;
        Vector3 screenOrigin;
        if (!WorldToScreen(entity.head, screenHead, viewMatrix, width, height) || !WorldToScreen(entity.origin, screenOrigin, viewMatrix, width, height))
            continue;

        float heightPx = std::fabs(screenOrigin.y - screenHead.y);
        float widthPx = heightPx * 0.4f;
        float left = screenHead.x - widthPx * 0.5f;
        float right = screenHead.x + widthPx * 0.5f;
        float bottomY = screenHead.y + heightPx;

        ImU32 color = IM_COL32(255, 128, 32, 220);
        drawList->AddRect(ImVec2(left, screenHead.y), ImVec2(right, bottomY), color, 0.0f, 0, 1.5f);

        char label[128];
        std::snprintf(label, sizeof(label), "%s [%d]", entity.name[0] ? entity.name : "Enemy", entity.health);
        drawList->AddText(ImVec2(left, screenHead.y - 16.0f), IM_COL32(255, 255, 255, 230), label);

        float barHeight = heightPx;
        float barWidth = 5.0f;
        float healthFraction = std::clamp(entity.health / 100.0f, 0.0f, 1.0f);
        float barTop = screenHead.y;
        float barBottom = screenHead.y + barHeight;
        float filledTop = barBottom - barHeight * healthFraction;
        drawList->AddRectFilled(ImVec2(left - 10.0f, barTop), ImVec2(left - 5.0f, barBottom), IM_COL32(60, 60, 60, 180));
        drawList->AddRectFilled(ImVec2(left - 10.0f, filledTop), ImVec2(left - 5.0f, barBottom), IM_COL32(0, 255, 0, 220));
    }
}

void RunFrame(int width, int height) {
    view_matrix_t viewMatrix = {};
    uintptr_t matrixPtr = GetClientBase() + cs2_dumper::offsets::client_dll::dwViewMatrix;
    if (!matrixPtr)
        return;
    viewMatrix = *reinterpret_cast<view_matrix_t*>(matrixPtr);

    uintptr_t localPawn = 0;
    if (!GetLocalPawn(localPawn) || !localPawn)
        return;

    Vector3 localOrigin;
    if (!GetSceneNodeOrigin(localPawn, localOrigin))
        return;

    Vector3 viewOffset = *reinterpret_cast<Vector3*>(localPawn + kPawnViewOffset);
    Vector3 localEye = { localOrigin.x + viewOffset.x, localOrigin.y + viewOffset.y, localOrigin.z + viewOffset.z };

    uintptr_t localController = 0;
    GetLocalController(localController);
    int localTeam = 0;
    if (localPawn)
        localTeam = *reinterpret_cast<uint8_t*>(localPawn + kControllerPlayerTeam);

    uintptr_t entityList = *reinterpret_cast<uintptr_t*>(GetClientBase() + cs2_dumper::offsets::client_dll::dwEntityList);
    if (!entityList)
        return;

    std::vector<Entity> enemies;
    enemies.reserve(64);

    for (int i = 1; i <= 64; ++i) {
        uintptr_t listEntry = *reinterpret_cast<uintptr_t*>(entityList + kEntityListFirstEntry + 8 * (i >> 9));
        if (!listEntry)
            continue;

        uintptr_t controller = *reinterpret_cast<uintptr_t*>(listEntry + kEntityListStride * (i & 0x1FF));
        if (!controller || controller == localController)
            continue;

        uintptr_t pawn = 0;
        if (!GetPawnFromController(controller, pawn) || !pawn)
            continue;

        Entity entity = BuildEntity(controller, pawn, localTeam);
        if (!entity.valid)
            continue;

        enemies.push_back(entity);
    }

    if (g_Settings.espEnabled) {
        DrawAllEntities(enemies, viewMatrix, width, height);
    }

    float* viewAngles = reinterpret_cast<float*>(GetClientBase() + cs2_dumper::offsets::client_dll::dwViewAngles);
    if (!viewAngles)
        return;

    Vector3 currentAngles = { viewAngles[0], viewAngles[1], 0.0f };
    Entity bestTarget;

    if (g_Settings.aimbotEnabled && FindBestTarget(enemies, localEye, currentAngles, bestTarget)) {
        AimAtTarget(bestTarget, localEye, currentAngles);
    }

    if (g_Settings.triggerbotEnabled && FindBestTarget(enemies, localEye, currentAngles, bestTarget)) {
        if (IsCrosshairOnTarget(bestTarget, viewMatrix, width, height)) {
            int32_t* attack = reinterpret_cast<int32_t*>(GetClientBase() + cs2_dumper::buttons::attack);
            if (attack) {
                *attack = 5;
            }
        }
    }
}
} // namespace cheat
