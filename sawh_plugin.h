#ifndef _INCLUDE_SAWH_PLUGIN_H_
#define _INCLUDE_SAWH_PLUGIN_H_

#include <ISmmPlugin.h>
#include <igameevents.h>
#include <vector>
#include <unordered_map>
#include <string>

#pragma pack(push, 1)
struct QuadNode {
    uint16_t x, y, w, h;
    uint8_t is_solid;
};
#pragma pack(pop)

class SAWHPlugin : public ISmmPlugin, public IMetamodListener
{
public:
	bool Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late);
	bool Unload(char *error, size_t maxlen);
	void AllPluginsLoaded();

public:
	const char *GetAuthor();
	const char *GetName();
	const char *GetDescription();
	const char *GetURL();
	const char *GetLicense();
	const char *GetVersion();
	const char *GetDate();
	const char *GetLogTag();
	void CalculateGrid(float x, float y, int& grid_x, int& grid_y);

private:
	void LoadMapData(const std::string& mapName);
	int GetNodeIdAt(int grid_x, int grid_y);
	bool IsVisible(int observer_x, int observer_y, int target_x, int target_y);
	void Hook_CheckTransmit(CCheckTransmitInfo **pInfoInfoList, int nInfoCount, CBitVec<16384> &unionTransmitEdicts, CBitVec<16384> &something, const Entity2Networkable_t **pNetworkables, const uint16 *pEntityIndicies, int nEntityIndices);

	std::string current_map;
	float world_min_x;
	float world_min_y;
	float cell_size;
	int map_width = 32;
	int map_height = 32;

	uint32_t num_nodes = 0;
	std::vector<QuadNode> quad_nodes;
	std::vector<uint8_t> visibility_grid;
};

extern SAWHPlugin g_SAWHPlugin;
extern ISource2GameEntities *g_pServerGameEnts;
extern IServerGameClients *g_pServerGameClients;
extern IVEngineServer2 *g_pEngine;

PLUGIN_GLOBALVARS();

#endif //_INCLUDE_SAWH_PLUGIN_H_
