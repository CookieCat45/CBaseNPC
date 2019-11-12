#include "extension.h"
#include "CDetour/detours.h"
#include "helpers.h"
#include "sourcesdk/nav_mesh.h"
#include "natives.h"
#include <ihandleentity.h>

CGlobalVars *gpGlobals = nullptr;
IGameConfig *g_pGameConf = nullptr;
IBinTools *g_pBinTools = nullptr;
IServerGameEnts *gameents = nullptr;
IEngineTrace *enginetrace = nullptr;
IdentityType_t g_CoreIdent;
CBaseEntityList *g_pEntityList = nullptr;

DEFINEHANDLEOBJ(SurroundingAreasCollector, CUtlVector< CNavArea* >);

ConVar NextBotPathDrawIncrement("cnb_path_draw_inc", "0", 0, "");                     
ConVar NextBotPathSegmentInfluenceRadius("cnb_path_segment_influence", "0", 0, "");

int g_iLastKnownAreaOffset = -1;
int g_iMyNextBotPointerOffset = -1;

HandleType_t g_CellArrayHandle;
HandleType_t g_KeyValueType;

CBaseNPCExt g_CBaseNPCExt;
SMEXT_LINK(&g_CBaseNPCExt);


IForward *g_pForwardSetLocalAngles = nullptr;
IForward *g_pForwardEventKilled = nullptr;

CDetour *g_pSetLocalAngles = nullptr;
CDetour* g_pNavMeshAddArea = nullptr;

CNavArea * (CNavMesh:: *CNavMesh::func_GetNearestNavArea)(const Vector &pos, bool anyZ, float maxDist, bool checkLOS, bool checkGround, int team) = nullptr;
bool (CNavMesh:: *CNavMesh::func_GetGroundHeight)(const Vector &pos, float *height, Vector *normal) = nullptr;
bool (CTraceFilterSimpleHack:: *CTraceFilterSimpleHack::func_ShouldHitEntity)(IHandleEntity *pHandleEntity, int contentsMask) = nullptr;

bool UTILMapLessFunc(const int32_t &in1, const int32_t &in2)
{
	return (in1 < in2);
}
CUtlMap<int32_t, int32_t> g_EntitiesHooks(UTILMapLessFunc);

DETOUR_DECL_MEMBER1(CBaseEntity_SetLocalAngles, void, QAngle&, angles)
{
	if (g_pForwardSetLocalAngles != nullptr)
	{
		cell_t iEntity = gamehelpers->EntityToBCompatRef(reinterpret_cast<CBaseEntity*>(this));
		
		cell_t vector[3];
		vector[0] = sp_ftoc(angles.x);
		vector[1] = sp_ftoc(angles.y);
		vector[2] = sp_ftoc(angles.z);

		cell_t result = Pl_Continue;
		g_pForwardSetLocalAngles->PushCell(iEntity);
		g_pForwardSetLocalAngles->PushArray(vector, 3, SM_PARAM_COPYBACK);
		
		g_pForwardSetLocalAngles->Execute(&result);
		if (result == Pl_Changed)
		{
			angles.x = sp_ctof(vector[0]);
			angles.y = sp_ctof(vector[1]);
			angles.z = sp_ctof(vector[2]);
		}
	}
	DETOUR_MEMBER_CALL(CBaseEntity_SetLocalAngles)(angles);
}

DETOUR_DECL_MEMBER1(CNavMesh_AddNavArea, void, CNavArea*, area)
{
	CTNavMesh::Add(area);
	DETOUR_MEMBER_CALL(CNavMesh_AddNavArea)(area);
}

bool CBaseNPCExt::SDK_OnLoad(char *error, size_t maxlength, bool late)
{
	char conf_error[255];
	if(!gameconfs->LoadGameConfigFile("nextbot_pathing", &g_pGameConf, conf_error, sizeof(conf_error)))
	{
		snprintf(error, maxlength, "FAILED TO LOAD GAMEDATA ERROR: %s", conf_error);
		return false;
	}
	
	CNavMesh::Init();
	if (TheNavMesh)
		g_pSM->LogMessage(myself, "Found TheNavMesh pointer: 0x%08x", TheNavMesh);
	
	if (g_pGameConf->GetMemSig("CNavMesh::GetNearestNavArea", reinterpret_cast<void **>(&CNavMesh::func_GetNearestNavArea))) {
		g_pSM->LogMessage(myself, "Got function: 0x%08x CNavMesh::GetNearestNavArea", *reinterpret_cast<uintptr_t *>(&CNavMesh::func_GetNearestNavArea));
	} else {
		g_pSM->LogMessage(myself, "Couldn't locate function CNavMesh::GetNearestNavArea!");
		return false;
	}
	
	if (g_pGameConf->GetMemSig("CNavMesh::GetGroundHeight", reinterpret_cast<void **>(&CNavMesh::func_GetGroundHeight))) {
		g_pSM->LogMessage(myself, "Got function: 0x%08x CNavMesh::GetGroundHeight", *reinterpret_cast<uintptr_t *>(&CNavMesh::func_GetGroundHeight));
	} else {
		g_pSM->LogMessage(myself, "Couldn't locate function CNavMesh::GetGroundHeight!");
		return false;
	}
	
	if (g_pGameConf->GetMemSig("CTraceFilterSimple::ShouldHitEntity", reinterpret_cast<void **>(&CTraceFilterSimpleHack::func_ShouldHitEntity))) {
		g_pSM->LogMessage(myself, "Got function: 0x%08x CTraceFilterSimple::ShouldHitEntity", *reinterpret_cast<uintptr_t *>(&CTraceFilterSimpleHack::func_ShouldHitEntity));
	} else {
		g_pSM->LogMessage(myself, "Couldn't locate function CTraceFilterSimple::ShouldHitEntity!");
		return false;
	}
	
	CDetourManager::Init(g_pSM->GetScriptingEngine(), g_pGameConf);

	g_pSetLocalAngles = DETOUR_CREATE_MEMBER(CBaseEntity_SetLocalAngles, "CBaseEntity::SetLocalAngles");
	if(g_pSetLocalAngles != nullptr)
	{
		g_pSetLocalAngles->EnableDetour();
		g_pSM->LogMessage(myself, "CBaseEntity::SetLocalAngles detour enabled.");
	}

	g_pNavMeshAddArea = DETOUR_CREATE_MEMBER(CNavMesh_AddNavArea, "CNavMesh::AddNavArea");
	if (g_pNavMeshAddArea != nullptr)
	{
		g_pNavMeshAddArea->EnableDetour();
		g_pSM->LogMessage(myself, "CNavMesh::AddNavArea detour enabled.");
	}

	g_pForwardSetLocalAngles = forwards->CreateForward("CBaseEntity_SetLocalAngles", ET_Event, 2, nullptr, Param_Cell, Param_Array);
	g_pForwardEventKilled = forwards->CreateForward("CBaseCombatCharacter_EventKilled", ET_Event, 9, nullptr, Param_Cell, Param_CellByRef, Param_CellByRef, Param_FloatByRef, Param_CellByRef, Param_CellByRef, Param_Array, Param_Array, Param_Cell);
	
	GETGAMEDATAOFFSET("CBaseCombatCharacter::GetLastKnownArea", g_iLastKnownAreaOffset);
	GETGAMEDATAOFFSET("CBaseEntity::MyNextBotPointer", g_iMyNextBotPointerOffset);
	int iOffset = 0;
	GETGAMEDATAOFFSET("CBaseCombatCharacter::EventKilled", iOffset);
	SH_MANUALHOOK_RECONFIGURE(MEvent_Killed, iOffset, 0, 0);
	
	CREATEHANDLETYPE(SurroundingAreasCollector);

	sharesys->AddDependency(myself, "bintools.ext", true, true);
	sharesys->AddNatives(myself, g_NativesInfo);
	sharesys->RegisterLibrary(myself, "nextbot_pathing");

	CTNavMesh::Init();

	return true;
}

bool CBaseNPCExt::SDK_OnMetamodLoad(ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	GET_V_IFACE_ANY(GetServerFactory, gameents, IServerGameEnts, INTERFACEVERSION_SERVERGAMEENTS);
	GET_V_IFACE_ANY(GetEngineFactory, enginetrace, IEngineTrace, INTERFACEVERSION_ENGINETRACE_SERVER);
	
	gpGlobals = ismm->GetCGlobals();
	return true;
}

void CBaseNPCExt::OnCoreMapStart(edict_t *pEdictList, int edictCount, int clientMax)
{
	CNavMesh::Init();
}

void CBaseNPCExt::OnCoreMapEnd()
{
	CTNavMesh::CleanUp();
}

void CBaseNPCExt::SDK_OnAllLoaded()
{
	SM_GET_LATE_IFACE(BINTOOLS, g_pBinTools);
	g_pSM->LogMessage(myself, "0x%08x bintools.", g_pBinTools);
	
	handlesys->FindHandleType("CellArray", &g_CellArrayHandle);
	handlesys->FindHandleType("KeyValues", &g_KeyValueType);
	g_CoreIdent = sharesys->FindIdentType("CORE");
	
	g_pEntityList = (CBaseEntityList *)gamehelpers->GetGlobalEntityList();
}

bool CBaseNPCExt::QueryRunning(char *error, size_t maxlength)
{
	SM_CHECK_IFACE(BINTOOLS, g_pBinTools);
	return true;
}

bool CBaseNPCExt::QueryInterfaceDrop(SMInterface *pInterface)
{
	if(pInterface == g_pBinTools)
		return false;

	return IExtensionInterface::QueryInterfaceDrop(pInterface);
}

void CBaseNPCExt::NotifyInterfaceDrop(SMInterface *pInterface)
{
	if (strcmp(pInterface->GetInterfaceName(), SMINTERFACE_BINTOOLS_NAME) == 0)
		g_pBinTools = nullptr;
}

void CBaseNPCExt::SDK_OnUnload()
{
	gameconfs->CloseGameConfigFile(g_pGameConf);

	forwards->ReleaseForward(g_pForwardSetLocalAngles);
	forwards->ReleaseForward(g_pForwardEventKilled);

	if (g_pSetLocalAngles != nullptr) g_pSetLocalAngles->Destroy();
	if (g_pNavMeshAddArea != nullptr) g_pNavMeshAddArea->Destroy();
	
	gameconfs->CloseGameConfigFile(g_pGameConf);
	
	FOR_EACH_MAP_FAST(g_EntitiesHooks, iHookID)
		SH_REMOVE_HOOK_ID(iHookID);
}

void CBaseNPCExt::OnEntityDestroyed(CBaseEntity *pEntity)
{
	if (!pEntity) return;

	auto iIndex = g_EntitiesHooks.Find(gamehelpers->EntityToReference(pEntity));
	if (g_EntitiesHooks.IsValidIndex(iIndex))
	{
		int iHookID = g_EntitiesHooks.Element(iIndex);
		SH_REMOVE_HOOK_ID(iHookID);
	}
}

//Fix external stuff error
float IntervalTimer::Now( void ) const
{
	return gpGlobals->curtime;
}

float CountdownTimer::Now( void ) const
{
	return gpGlobals->curtime;
}

bool CGameTrace::DidHitWorld() const
{
	return gamehelpers->EntityToBCompatRef(reinterpret_cast<CBaseEntity *>(m_pEnt)) == 0;
}

bool CGameTrace::DidHitNonWorldEntity() const
{
	return m_pEnt != nullptr && !DidHitWorld();
}