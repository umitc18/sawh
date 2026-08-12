#include "sawh_plugin.h"
#include <fstream>
#include <cmath>

#include "schema_helper.hpp"
#include <entity2/entitysystem.h>
#include <mathlib/vector.h>
void* g_pGameResourceService = nullptr;
ISource2GameEntities *g_pServerGameEnts = nullptr;
IServerGameClients *g_pServerGameClients = nullptr;
IVEngineServer2 *g_pEngine = nullptr;

#include <tier1/convar.h>
SH_DECL_HOOK2_void(IServerGameClients, ClientCommand, SH_NOATTRIB, 0, CPlayerSlot, const CCommand&);

struct SAWHOffsets
{
    bool resolved = false;
    int controllerToPawnHandle = -1;
    int pawnBodyComponent = -1;
    int bodyComponentSceneNode = -1;
    int sceneNodeAbsOrigin = -1;
    int pawnWeaponServices = -1;
    int weaponServicesMyWeapons = -1;
};

SAWHOffsets g_Offsets;

static void SAWH_ResolveOffsetsOnce()
{
    if (g_Offsets.resolved) return;
    g_Offsets.controllerToPawnHandle = SAWH_GetSchemaOffset("CBasePlayerController", "m_hPawn");
    g_Offsets.pawnBodyComponent      = SAWH_GetSchemaOffset("CBaseEntity", "m_CBodyComponent");
    g_Offsets.bodyComponentSceneNode = SAWH_GetSchemaOffset("CBodyComponent", "m_pSceneNode");
    g_Offsets.sceneNodeAbsOrigin     = SAWH_GetSchemaOffset("CGameSceneNode", "m_vecAbsOrigin");
    g_Offsets.pawnWeaponServices     = SAWH_GetSchemaOffset("CBasePlayerPawn", "m_pWeaponServices");
    g_Offsets.weaponServicesMyWeapons = SAWH_GetSchemaOffset("CPlayer_WeaponServices", "m_hMyWeapons");
    g_Offsets.resolved = true;

    META_CONPRINTF("[SAWH] Schema Offsets -> m_hPawn: %d, body: %d, scene: %d, origin: %d, wpnsrv: %d, mywpns: %d\n",
        g_Offsets.controllerToPawnHandle, g_Offsets.pawnBodyComponent, g_Offsets.bodyComponentSceneNode, g_Offsets.sceneNodeAbsOrigin,
        g_Offsets.pawnWeaponServices, g_Offsets.weaponServicesMyWeapons);
}

inline int HandleToEntityIndex(uint32_t handleValue)
{
    return handleValue & 0x7FFF;
}

inline CEntityInstance* SAWH_GetEntityInstance(CGameEntitySystem* pSystem, int index)
{
    if (!pSystem || index < 0 || index >= (64 * 512 - 1)) return nullptr;

    CEntityIdentity* pChunk = pSystem->m_EntityList.m_pIdentityChunks[index / 512];
    if (!pChunk) return nullptr;

    CEntityIdentity* pIdentity = &pChunk[index % 512];
    return pIdentity->m_pInstance;
}

static bool SAWH_GetPawnOriginForController(void* pEntitySystem, int controllerEntIndex, Vector& outOrigin, int& outPawnEntIndex, void*& outPawn)
{
    CGameEntitySystem* pSystem = static_cast<CGameEntitySystem*>(pEntitySystem);

    if (controllerEntIndex <= 0 || controllerEntIndex >= (64 * 512 - 1)) return false;

    CEntityIdentity* pChunk = pSystem->m_EntityList.m_pIdentityChunks[controllerEntIndex / 512];
    if (!pChunk) return false;

    CEntityIdentity* pIdentity = &pChunk[controllerEntIndex % 512];
    CEntityInstance* pController = pIdentity->m_pInstance;

    if (!pController) {
        return false;
    }

    const char* pszClassName = pIdentity->m_designerName.String();
    if (!pszClassName || strstr(pszClassName, "player_controller") == nullptr) {
        return false;
    }

    if (g_Offsets.controllerToPawnHandle < 0) {
        static bool s_log1 = false; if(!s_log1){ META_CONPRINTF("[SAWH] controllerToPawnHandle is invalid!\n"); s_log1 = true; }
        return false;
    }

    uint32_t* pHandleValue = SAWH_GetFieldPtr<uint32_t>(pController, g_Offsets.controllerToPawnHandle);
    if (!pHandleValue || *pHandleValue == 0xFFFFFFFF) {
        return false;
    }

    int pawnIndex = HandleToEntityIndex(*pHandleValue);
    CEntityInstance* pPawn = SAWH_GetEntityInstance(pSystem, pawnIndex);
    if (!pPawn)
        return false;

    if (g_Offsets.pawnBodyComponent < 0) {
        static bool s_log3 = false; if(!s_log3){ META_CONPRINTF("[SAWH] pawnBodyComponent is invalid!\n"); s_log3 = true; }
        return false;
    }

    void** pBodyComponent = SAWH_GetFieldPtr<void*>(pPawn, g_Offsets.pawnBodyComponent);
    if (!pBodyComponent || !*pBodyComponent)
        return false;

    if (g_Offsets.bodyComponentSceneNode < 0 || g_Offsets.sceneNodeAbsOrigin < 0) {
        static bool s_log3 = false; if(!s_log3){ META_CONPRINTF("[SAWH] sceneNode offsets invalid!\n"); s_log3 = true; }
        return false;
    }

    void* pSceneNode = *SAWH_GetFieldPtr<void**>(*pBodyComponent, g_Offsets.bodyComponentSceneNode);
    if (!pSceneNode) {
        return false;
    }

    Vector* pOrigin = SAWH_GetFieldPtr<Vector>(pSceneNode, g_Offsets.sceneNodeAbsOrigin);
    outOrigin = *pOrigin;
    outPawnEntIndex = pawnIndex;
    outPawn = pPawn;
    return true;
}

struct CS2_CUtlVector {
    int m_Size;
    uint32_t* m_pElements;
};

static void SAWH_ClearPawnWeapons(CCheckTransmitInfo* pInfo, void* pPawn)
{
    if (g_Offsets.pawnWeaponServices < 0 || g_Offsets.weaponServicesMyWeapons < 0) return;

    void* pWeaponServices = *SAWH_GetFieldPtr<void**>(pPawn, g_Offsets.pawnWeaponServices);
    if (!pWeaponServices) return;

    void* pUtlVector = SAWH_GetFieldPtr<void*>(pWeaponServices, g_Offsets.weaponServicesMyWeapons);
    if (!pUtlVector) return;

    // layout?:
    // +0: int m_Size
    // +4: padding
    // +8: T* m_pElements

    int size = *reinterpret_cast<int*>(pUtlVector);
    uint32_t* pElements = *reinterpret_cast<uint32_t**>((uintptr_t)pUtlVector + 8);

    if (size <= 0 || size > 64 || !pElements) return;

    for (int i = 0; i < size; ++i)
    {
        uint32_t handle = pElements[i];
        if (handle != 0xFFFFFFFF)
        {
            int wpnIndex = HandleToEntityIndex(handle);
            if (wpnIndex > 0 && wpnIndex < 16384) {
                pInfo->m_pTransmitEntity->Clear(wpnIndex);
            }
        }
    }
}

SAWHPlugin g_SAWHPlugin;
PLUGIN_EXPOSE(SAWHPlugin, g_SAWHPlugin);

SH_DECL_HOOK7_void(ISource2GameEntities, CheckTransmit, SH_NOATTRIB, 0, CCheckTransmitInfo **, int, CBitVec<16384> &, CBitVec<16384> &, const Entity2Networkable_t **, const uint16 *, int);

bool SAWHPlugin::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetServerFactory, g_pServerGameEnts, ISource2GameEntities, INTERFACEVERSION_SERVERGAMEENTS);
	GET_V_IFACE_CURRENT(GetServerFactory, g_pServerGameClients, IServerGameClients, INTERFACEVERSION_SERVERGAMECLIENTS);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pEngine, IVEngineServer2, INTERFACEVERSION_VENGINESERVER);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);

	CreateInterfaceFn engineFactory = ismm->GetEngineFactory();

	if (engineFactory)
		g_pGameResourceService = engineFactory("GameResourceServiceServerV001", nullptr);

	if (g_pServerGameEnts)
	{
		META_CONPRINTF("[SAWH] g_pServerGameEnts FOUND! Hooking CheckTransmit as POST...\n");
		SH_ADD_HOOK(ISource2GameEntities, CheckTransmit, g_pServerGameEnts, SH_MEMBER(this, &SAWHPlugin::Hook_CheckTransmit), true);
	}
	else
		META_CONPRINTF("[SAWH] ERROR: g_pServerGameEnts IS NULL! CheckTransmit will NOT be hooked.\n");


	META_CONVAR_REGISTER(0);

	if (!g_pGameResourceService)
	    META_CONPRINTF("[SAWH] ERROR: GameResourceServiceServerV001 IS NULL!\n");
	else
		META_CONPRINTF("[SAWH] GameResourceServiceServerV001 FOUND! Pointer: %p\n", g_pGameResourceService);

	if (!SAWH_InitSchemaSystem())
		META_CONPRINTF("[SAWH] ERROR: Failed to initialize schema system!\n");
	else
		META_CONPRINTF("[SAWH] Schema system initialized successfully.\n");

	//
	LoadMapData("de_dust2");

	return true;
}

bool SAWHPlugin::Unload(char *error, size_t maxlen)
{
	if (g_pServerGameEnts)
	{
		SH_REMOVE_HOOK(ISource2GameEntities, CheckTransmit, g_pServerGameEnts, SH_MEMBER(this, &SAWHPlugin::Hook_CheckTransmit), false);
	}

	ConVar_Unregister();
	return true;
}

void SAWHPlugin::AllPluginsLoaded()
{
}

void SAWHPlugin::LoadMapData(const std::string& mapName)
{
	char configDir[256];
	g_SMAPI->PathFormat(configDir, sizeof(configDir), "%s/addons/sawh/configs", g_SMAPI->GetBaseDir());

	char binPath[512];
	g_SMAPI->PathFormat(binPath, sizeof(binPath), "%s/%s.bin", configDir, mapName.c_str());

	std::ifstream binFile(binPath, std::ios::binary);
	if (!binFile.is_open())
	{
		META_CONPRINTF("[SAWH] Could not open map data: %s\n", binPath);
		return;
	}

	META_CONPRINTF("[SAWH] Loading map data: %s\n", binPath);
	
	char signature[4];
	binFile.read(signature, 4);
	if (strncmp(signature, "SAWH", 4) != 0)
	{
		META_CONPRINTF("[SAWH] Invalid header! File may be corrupt.\n");
		return;
	}

	uint32_t version;
	binFile.read(reinterpret_cast<char*>(&version), sizeof(version));
	
	// for future
	if (version < 2) {
        META_CONPRINTF("[SAWH] Unsupported binary version (%d). Please regenerate map with new generator.\n", version);
        return;
    }

	uint32_t width, height;
	binFile.read(reinterpret_cast<char*>(&width), sizeof(width));
	binFile.read(reinterpret_cast<char*>(&height), sizeof(height));
	
	float rx, ry, rs;
	binFile.read(reinterpret_cast<char*>(&rx), sizeof(rx));
	binFile.read(reinterpret_cast<char*>(&ry), sizeof(ry));
	binFile.read(reinterpret_cast<char*>(&rs), sizeof(rs));

    uint32_t nodes_count;
    binFile.read(reinterpret_cast<char*>(&nodes_count), sizeof(nodes_count));

	current_map = mapName;
	map_width = width;
	map_height = height;
	world_min_x = rx;
	world_min_y = ry;
	cell_size = rs;
    num_nodes = nodes_count;

    quad_nodes.resize(num_nodes);
    binFile.read(reinterpret_cast<char*>(quad_nodes.data()), num_nodes * sizeof(QuadNode));

	int total_bits = num_nodes * num_nodes;
	int num_bytes = (total_bits + 7) / 8;
	
	std::vector<uint8_t> buffer(num_bytes);
	binFile.read(reinterpret_cast<char*>(buffer.data()), num_bytes);
	
	visibility_grid.assign(total_bits, false);
	for (int i = 0; i < total_bits; ++i) {
		visibility_grid[i] = (buffer[i / 8] & (1 << (i % 8))) != 0;
	}
	
	float memory_mb = static_cast<float>(total_bits) / 8.0f / 1024.0f / 1024.0f;
	META_CONPRINTF("[SAWH] Loaded data for %s (Nodes: %d, RAM: %.2f MB)\n", mapName.c_str(), num_nodes, memory_mb);
}

void SAWHPlugin::CalculateGrid(float x, float y, int& grid_x, int& grid_y)
{
	float radar_pos_x = -2476.0f;
	float radar_pos_y = 3239.0f;
	float radar_scale = 4.4f;

	if (world_min_x != 0.0f || world_min_y != 0.0f) {
		radar_pos_x = world_min_x;
		radar_pos_y = world_min_y;
		radar_scale = cell_size;
	}

	float image_x = (x - radar_pos_x) / radar_scale;
	float image_y = (radar_pos_y - y) / radar_scale;

	float pixels_per_cell_x = 1024.0f / static_cast<float>(map_width);
	float pixels_per_cell_y = 1024.0f / static_cast<float>(map_height);

	grid_x = static_cast<int>(std::floor(image_x / pixels_per_cell_x));
	grid_y = static_cast<int>(std::floor(image_y / pixels_per_cell_y));
}

int SAWHPlugin::GetNodeIdAt(int grid_x, int grid_y)
{
    if (grid_x < 0 || grid_x >= map_width || grid_y < 0 || grid_y >= map_height)
        return -1;
        
    for (uint32_t i = 0; i < num_nodes; ++i) {
        const QuadNode& n = quad_nodes[i];
        if (grid_x >= n.x && grid_x < n.x + n.w && grid_y >= n.y && grid_y < n.y + n.h) {
            return i;
        }
    }
    return -1;
}

bool SAWHPlugin::IsVisible(int observer_x, int observer_y, int target_x, int target_y)
{
    int obs_node = GetNodeIdAt(observer_x, observer_y);
    int tgt_node = GetNodeIdAt(target_x, target_y);
    
    if (obs_node == -1 || tgt_node == -1) return false;
    
    if (quad_nodes[obs_node].is_solid || quad_nodes[tgt_node].is_solid) {
        return false;
    }
    
	if (visibility_grid.empty())
	{
		return false;
	}

	int global_index = obs_node * num_nodes + tgt_node;

	if (global_index >= 0 && global_index < visibility_grid.size())
	{
		return visibility_grid[global_index];
	}

	return false;
}

static int g_SAWHTickCount = 0;

void SAWHPlugin::Hook_CheckTransmit(CCheckTransmitInfo **pInfoInfoList, int nInfoCount, CBitVec<16384> &unionTransmitEdicts, CBitVec<16384> &something, const Entity2Networkable_t **pNetworkables, const uint16 *pEntityIndicies, int nEntityIndices)
{
	static int s_CheckTransmitFiredCount = 0;
	if (s_CheckTransmitFiredCount < 5)
	{
		META_CONPRINTF("[SAWH] Hook_CheckTransmit IS FIRING! (Call %d)\n", s_CheckTransmitFiredCount);
		s_CheckTransmitFiredCount++;
	}

	if (!pInfoInfoList || nInfoCount <= 0 || !g_pGameResourceService)
	{
		static bool s_LoggedNoInfo = false;
		if (!s_LoggedNoInfo) {
			META_CONPRINTF("[SAWH] CheckTransmit ignored! pInfoInfoList=%p, nInfoCount=%d, g_pGameResourceService=%p\n", pInfoInfoList, nInfoCount, g_pGameResourceService);
			s_LoggedNoInfo = true;
		}
		RETURN_META(MRES_IGNORED);
	}

	void* pGameEntitySystem = *reinterpret_cast<void**>((uintptr_t)g_pGameResourceService + 0x50);
	if (!pGameEntitySystem)
	{
		pGameEntitySystem = *reinterpret_cast<void**>((uintptr_t)g_pGameResourceService + 0x58);
	}

	if (!pGameEntitySystem)
	{
		static bool s_LoggedNullEntity = false;
		if (!s_LoggedNullEntity) {
			META_CONPRINTF("[SAWH] CheckTransmit ignored because GameEntitySystem pointer is NULL!\n");
			s_LoggedNullEntity = true;
		}
		RETURN_META(MRES_IGNORED);
	}

	static bool s_ReachedResolve = false;
	if (!s_ReachedResolve) {
		META_CONPRINTF("[SAWH] Reached SAWH_ResolveOffsetsOnce! pGameEntitySystem = %p\n", pGameEntitySystem);
		s_ReachedResolve = true;
	}

	g_SAWHTickCount++;
	bool bShouldLog = (g_SAWHTickCount % 128 == 0);

	SAWH_ResolveOffsetsOnce();

	struct PlayerCache {
		bool bValid = false;
		int grid_x = 0;
		int grid_y = 0;
		Vector origin;
		int pawnIndex = -1;
		void* pPawn = nullptr;
	};
	PlayerCache tickCache[65];

	for (int i = 1; i <= 64; ++i) {
		if (SAWH_GetPawnOriginForController(pGameEntitySystem, i, tickCache[i].origin, tickCache[i].pawnIndex, tickCache[i].pPawn)) {
			tickCache[i].bValid = true;
			CalculateGrid(tickCache[i].origin.x, tickCache[i].origin.y, tickCache[i].grid_x, tickCache[i].grid_y);
		}
	}

	for (int i = 0; i < nInfoCount; i++)
	{
		CCheckTransmitInfo *pInfo = pInfoInfoList[i];
		if (!pInfo || !pInfo->m_pTransmitEntity)
			continue;

		int iPlayerSlot = (int)*((uint8*)pInfo + 576);
		int observerSlot = iPlayerSlot;
		int observerControllerIndex = observerSlot + 1;

		if (!tickCache[observerControllerIndex].bValid)
			continue;

		int obs_grid_x = tickCache[observerControllerIndex].grid_x;
		int obs_grid_y = tickCache[observerControllerIndex].grid_y;

		if (bShouldLog)
		{
			META_CONPRINTF("[SAWH] PLAYER %d: WORLD[%.1f, %.1f, %.1f] -> GRID[%d, %d]\n", 
				observerControllerIndex, tickCache[observerControllerIndex].origin.x, tickCache[observerControllerIndex].origin.y, tickCache[observerControllerIndex].origin.z, obs_grid_x, obs_grid_y);
		}

		for (int targetSlot = 0; targetSlot < 64; ++targetSlot)
		{
			if (targetSlot == observerSlot) continue;

			int targetControllerIndex = targetSlot + 1;

			if (!tickCache[targetControllerIndex].bValid)
				continue;

			int tgt_grid_x = tickCache[targetControllerIndex].grid_x;
			int tgt_grid_y = tickCache[targetControllerIndex].grid_y;
			int targetPawnIndex = tickCache[targetControllerIndex].pawnIndex;
			void* pTargetPawn = tickCache[targetControllerIndex].pPawn;

			bool isVis = IsVisible(obs_grid_x, obs_grid_y, tgt_grid_x, tgt_grid_y);

			if (!isVis)
			{
				pInfo->m_pTransmitEntity->Clear(targetPawnIndex);
				SAWH_ClearPawnWeapons(pInfo, pTargetPawn);
			}
		}
	}

	RETURN_META(MRES_IGNORED);
}


const char *SAWHPlugin::GetAuthor() { return "umitc18"; }
const char *SAWHPlugin::GetName() { return "CS2 Simple Anti Wallhack"; }
const char *SAWHPlugin::GetDescription() { return "Grid based visibility checker"; }
const char *SAWHPlugin::GetURL() { return ""; }
const char *SAWHPlugin::GetLicense() { return "MIT"; }
const char *SAWHPlugin::GetVersion() { return "0.2.0"; }
const char *SAWHPlugin::GetDate() { return __DATE__; }
const char *SAWHPlugin::GetLogTag() { return "SAWH"; }
