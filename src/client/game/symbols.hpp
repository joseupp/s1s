#pragma once

#define WEAK __declspec(selectany)

namespace game
{
	/***************************************************************
	 * Functions
	 **************************************************************/

	WEAK symbol<void(int type, VariableUnion u)> AddRefToValue{0x140315830, 0x14044F300};
	WEAK symbol<void(unsigned int id)> AddRefToObject{0, 0x14044F2F0};
	WEAK symbol<unsigned int(unsigned int id)> AllocThread{0, 0x14044F650};
	WEAK symbol<void(int type, VariableUnion u)> RemoveRefToValue{0x140317340, 0x0};
	WEAK symbol<void(unsigned int id)> RemoveRefToObject{0, 0x140450B10};

	WEAK symbol<void(void*, void*)> AimAssist_AddToTargetList{0, 0x140052700};

	WEAK symbol<void(unsigned int weapon, bool isAlternate, char* output, unsigned int maxStringLen)> BG_GetWeaponNameComplete{0x0, 0x1401C0030};

	WEAK symbol<void(errorParm code, const char* message, ...)> Com_Error{0x1402F7570, 0x14042BC30};
	WEAK symbol<void()> Com_Frame_Try_Block_Function{0x1402F7E10, 0x14042C610};
	WEAK symbol<CodPlayMode()> Com_GetCurrentCoDPlayMode{0, 0x14052BBC0};
	WEAK symbol<void(float, float, int)> Com_SetSlowMotion{0, 0x14042F6E0};
	WEAK symbol<void()> Com_Quit_f{0x1402F9390, 0x14042E470};

	WEAK symbol<void(const char* cmdName, void (), cmd_function_s* allocedCmd)> Cmd_AddCommandInternal{0x1402EDDB0, 0x14040C180};
	WEAK symbol<void(int localClientNum, int controllerIndex, const char* text)> Cmd_ExecuteSingleCommand{0x1402EE350, 0x14040C700};
	WEAK symbol<void(const char*)> Cmd_RemoveCommand{0x1402EE910, 0x14040CEA0};
	WEAK symbol<void(const char* text_in)> Cmd_TokenizeString{0x1402EEA30, 0x14040CFE0};
	WEAK symbol<void()> Cmd_EndTokenizeString{0x1402EE000, 0x14040C530};

	WEAK symbol<void(const char* message)> Conbuf_AppendText{0x14038F220, 0x14053C3B0};

	WEAK symbol<char*(int start)> ConcatArgs{0x14021A7E0, 0x1403439B0};

	WEAK symbol<void(int localClientNum, void (*)(int localClientNum))> Cbuf_AddCall{0x1402ED820, 0x14040BB80};
	WEAK symbol<void(int localClientNum, const char* text)> Cbuf_AddText{0x1402ED890, 0x14040BC00};
	WEAK symbol<void(int localClientNum, int controllerIndex, const char* buffer, void (int, int, const char*))> Cbuf_ExecuteBufferInternal{0x1402ED9A0, 0x14040BEF0};

	WEAK symbol<bool()> CL_IsCgameInitialized{0x140136560, 0x1402592B0};
	WEAK symbol<void(int localClientNum, const char* string)> CL_ForwardCommandToServer{0x0, 0x140266C60};
	WEAK symbol<void(int localClientNum)> CL_WritePacket{0x0, 0x140260F00};
	WEAK symbol<void(int localClientNum)> CL_Disconnect{0x0, 0x1402655C0};

	WEAK symbol<void(int localClientNum, const char* message)> CG_GameMessage{0x1400EE500, 0x1401FEC50};
	WEAK symbol<void(int localClientNum, /*mp::cg_s**/void* cg, const char* dvar, const char* value)> CG_SetClientDvarFromServer{0, 0x14021C2B0};

	WEAK symbol<void(XAssetType type, void (*func)(XAssetHeader, void*), void* inData, bool includeOverride)> DB_EnumXAssets_FastFile{0x14017D7C0, 0x1402CA6C0};
	WEAK symbol<void(XAssetType type, void(*func)(XAssetHeader, void*), const void* inData, bool includeOverride)> DB_EnumXAssets_Internal{0x14017D830, 0x1402CA730};
	WEAK symbol<XAssetEntry(XAssetType type, const char* name)> DB_FindXAssetEntry{0x14017D830, 0x1402CA9E0};
	WEAK symbol<XAssetHeader(XAssetType type, const char *name, int allowCreateDefault)> DB_FindXAssetHeader{0x14017DCA0, 0x1402CAAB0};
	WEAK symbol<const char*(const XAsset* asset)> DB_GetXAssetName{0x140151C00, 0x14029D8D0};
	WEAK symbol<int(XAssetType type)> DB_GetXAssetTypeSize{0x140151C20, 0x14029D8F0};
	WEAK symbol<void(XZoneInfo* zoneInfo, unsigned int zoneCount, DBSyncMode syncMode)> DB_LoadXAssets{0x14017FB20, 0x1402CC9C0};
	WEAK symbol<int(XAssetType type, const char* name)> DB_XAssetExists{0x140182190, 0x1402D07D0};
	WEAK symbol<int(XAssetType type, const char* name)> DB_IsXAssetDefault{0x14017EEF0, 0x1402CBCA0};
	WEAK symbol<int(const RawFile* rawfile)> DB_GetRawFileLen{0x14017E890, 0x1402CB790};
	WEAK symbol<void(const RawFile* rawfile, char* buf, int size)> DB_GetRawBuffer{0x14017E750, 0x1402CB650};
	WEAK symbol<char*(const char* filename, char* buf, int size)> DB_ReadRawFile{0x140180E30, 0x1402CE540};

	WEAK symbol<dvar_t*(const char* name)> Dvar_FindVar{0x140370860, 0x1405225F0};
	WEAK symbol<void(const dvar_t* dvar)> Dvar_ClearModified{0x140370700, 0x140522380};
	WEAK symbol<void(char* buffer, int index)> Dvar_GetCombinedString{0x1402FB590, 0x140430960};
	WEAK symbol<bool(const char* name)> Dvar_IsValidName{0x140370CB0, 0x140522F10};
	WEAK symbol<void(dvar_t* dvar, DvarSetSource source)> Dvar_Reset{0x140372950, 0x140524670};
	WEAK symbol<void(const char* dvar, const char* buffer)> Dvar_SetCommand{0x1403730D0, 0x140524AF0};
	WEAK symbol<void(const dvar_t* dvar, const char* string)> Dvar_SetString{0x140373DE0, 0x1405254C0};
	WEAK symbol<void(const dvar_t* dvar, bool value)> Dvar_SetBool{0x0, 0x140524800};
	WEAK symbol<void(const char*, const char*, DvarSetSource)> Dvar_SetFromStringByNameFromSource{0x1403737D0, 0x140524F00};
	WEAK symbol<const char*(dvar_t* dvar, dvar_value value)> Dvar_ValueToString{0x140374E10, 0x140526190};

	WEAK symbol<dvar_t*(const char* dvarName, bool value, unsigned int flags, const char* description)>
	Dvar_RegisterBool{0x140371850, 0x1405234E0};
	WEAK symbol<dvar_t*(const char* dvarName, const char** valueList, int defaultIndex, unsigned int flags,
	                    const char* description)> Dvar_RegisterEnum{0x140371B30, 0x140523780};
	WEAK symbol<dvar_t*(const char* dvarName, float value, float min, float max, unsigned int flags,
	                    const char* description)> Dvar_RegisterFloat{0x140371C20, 0x140523860};
	WEAK symbol<dvar_t*(const char* dvarName, int value, int min, int max, unsigned int flags, const char* desc)>
	Dvar_RegisterInt{0x140371CF0, 0x140523970};
	WEAK symbol<dvar_t*(const char* dvarName, const char* value, unsigned int flags, const char* description)>
	Dvar_RegisterString{0x140372050, 0x140523D50};
	WEAK symbol<dvar_t* (const char* dvarName, float x, float y, float min, float max,
		                 unsigned int flags, const char* description)> Dvar_RegisterVec2{0x140372120, 0x140523E10};
	WEAK symbol<dvar_t* (const char* dvarName, float x, float y, float z, float min, float max,
		                 unsigned int flags, const char* description)> Dvar_RegisterVec3{0x140372230, 0x140523F10};
	WEAK symbol<dvar_t*(const char* dvarName, float x, float y, float z, float w, float min, float max,
	                    unsigned int flags, const char* description)> Dvar_RegisterVec4{0x140372430, 0x140524220};

	WEAK symbol<DWOnlineStatus()> dwGetLogOnStatus{0, 0x1405A2540};

	WEAK symbol<long long(const char* qpath, char** buffer)> FS_ReadFile{0x140362390, 0x140512440};
	WEAK symbol<void(void* buffer)> FS_FreeFile{0x140362380, 0x140512430};
	WEAK symbol<void (const char *gameName)> FS_Startup{0x140361940, 0x140511A20};
	WEAK symbol<void (const char *path, const char *dir, int bLanguageDirectory, int iLanguage)> FS_AddGameDirectory{0x14035F3F0, 0x14050FB60};
	WEAK symbol<void (const char *path, const char *dir)> FS_AddLocalizedGameDirectory{0x14035F5C0, 0x14050FE50};

	WEAK symbol<void()> GScr_LoadConsts{0x140283970, 0x1403A2E60};
	WEAK symbol<unsigned int(unsigned int parentId, unsigned int name)> FindVariable{0x1403165D0, 0x14044FFB0};
	WEAK symbol<unsigned int(int entnum, unsigned int classnum)> FindEntityId{0x1403166D0, 0x14044FEB0};
	WEAK symbol<scr_string_t(unsigned int parentId, unsigned int id)> GetVariableName{0x1403170E0, 0x1404509C0};
	WEAK symbol<void(VariableValue* result, unsigned int classnum, int entnum, int offset)> GetEntityFieldValue{0x14031AAD0, 0x140454540};
	WEAK symbol<unsigned int(unsigned int)> GetObjectType{0x140316F70, 0x1404507B0};
	WEAK symbol<unsigned int(unsigned int, unsigned int)> GetVariable{0x0, 0x140450880};

	WEAK symbol<void()> G_Glass_Update{0x14021D540, 0x1403489F0};

	WEAK symbol<int(int clientNum)> G_GetClientScore{0, 0x1403515F0};
	WEAK symbol<unsigned int(const char* name)> G_GetWeaponForName{0x140274590, 0x14039B6A0};
	WEAK symbol<int(playerState_s* ps, unsigned int weapon, int dualWield, int startInAltMode, int, int, int, char)> G_GivePlayerWeapon{0x1402749B0, 0x14039BBB0};
	WEAK symbol<void(playerState_s* ps, unsigned int weapon, int hadWeapon)> G_InitializeAmmo{0x1402217F0, 0x14034D670};
	WEAK symbol<void(int clientNum, unsigned int weapon)> G_SelectWeapon{0x140275380, 0x14039CA70};
	WEAK symbol<int(playerState_s* ps, unsigned int weapon)> G_TakePlayerWeapon{0x1402754E0, 0x14039CC30};

	WEAK symbol<char*(char* string)> I_CleanStr{0x140379010, 0x14052BEB0};

	WEAK symbol<char*(GfxImage *image, uint32_t width, uint32_t height, uint32_t depth, uint32_t mipCount, uint32_t imageFlags, DXGI_FORMAT imageFormat, const char *name, const D3D11_SUBRESOURCE_DATA *initData)> Image_Setup{0x1404858D0, 0x1406099C0};

	WEAK symbol<const char*(int, int, int)> Key_KeynumToString{0x14013F380, 0x140263350};

	WEAK symbol<unsigned int(int)> Live_SyncOnlineDataFlags{0x1404459A0, 0x1405C8520};

	WEAK symbol<void(int localClientNum, const char* menuName, int isPopup, int isModal, unsigned int isExclusive)> LUI_OpenMenu{0, 0x1404F0D00};
	WEAK symbol<void()> LUI_EnterCriticalSection{0, 0x14012B110};
	WEAK symbol<void()> LUI_LeaveCriticalSection{0, 0x140130CE0};

	WEAK symbol<bool(int localClientNum, const char* menuName)> Menu_IsMenuOpenAndVisible{0, 0x1404EB420};

	WEAK symbol<Material*(const char* material)> Material_RegisterHandle{0x1404919D0, 0x140616F00};

	WEAK symbol<void(netadr_s*, sockaddr*)> NetadrToSockadr{0, 0x14051A940};

	WEAK symbol<void(netsrc_t, netadr_s*, const char*)> NET_OutOfBandPrint{0, 0x1404382D0};
	WEAK symbol<void(netsrc_t sock, int length, const void* data, const netadr_s* to)> NET_SendLoopPacket{0, 0x140438440};
	WEAK symbol<bool(const char* s, netadr_s* a)> NET_StringToAdr{0, 0x140438570};

	WEAK symbol<void(float x, float y, float width, float height, float s0, float t0, float s1, float t1,
	                 float* color, Material* material)> R_AddCmdDrawStretchPic{0x1404A2580, 0x140628140};
	WEAK symbol<void(const char*, int, Font_s*, float, float, float, float, float, float*, int)> R_AddCmdDrawText{0x1404A2BF0, 0x1406287C0};

	WEAK symbol<void(const char*, int, Font_s*, float, float, float, float, float, const float*, int, int, char)> R_AddCmdDrawTextWithCursor{0x1404A35E0, 0x140628C60};
	WEAK symbol<Font_s*(const char* font)> R_RegisterFont{0x140481F90, 0x140606290};
	WEAK symbol<void()> R_SyncRenderThread{0x1404A4D60, 0x14062A4C0};
	WEAK symbol<int(const char* text, int maxChars, Font_s* font)> R_TextWidth{0x140482270, 0x140606570};

	WEAK symbol<ScreenPlacement*()> ScrPlace_GetViewPlacement{0x14014FA70, 0x140299900};

	WEAK symbol<unsigned int()> Scr_AllocArray{0x140317C50, 0x1404513A0};
	WEAK symbol<const float*(const float* v)> Scr_AllocVector{0x140317D10, 0x1404514F0};
	WEAK symbol<const char*(unsigned int index)> Scr_GetString{0x14031C570, 0x140456140};
	WEAK symbol<void(const char* value)> Scr_AddString{0x0, 0x140455000};
	WEAK symbol<void(int value)> Scr_AddInt{0x0, 0x140454F20};
	WEAK symbol<int(unsigned int index)> Scr_GetInt{0x14031C1F0, 0x140455DB0};
	WEAK symbol<float(unsigned int index)> Scr_GetFloat{0x14031C090, 0x140455C70};
	WEAK symbol<unsigned int()> Scr_GetNumParam{0x14031C2A0, 0x140455E60};
	WEAK symbol<void()> Scr_ClearOutParams{0x14031B7C0, 0x140455480};
	WEAK symbol<scr_entref_t(unsigned int entId)> Scr_GetEntityIdRef{0x14031A0D0, 0x140453AC0};
	WEAK symbol<int(unsigned int classnum, int entnum, int offset)> Scr_SetObjectField{0x14026B620, 0x140394570};
	WEAK symbol<void(void* ent, unsigned int stringValue, unsigned int paramcount)> Scr_Notify{0x0, 0x140394020};
	WEAK symbol<void(unsigned int id, unsigned int stringValue, unsigned int paramcount)> Scr_NotifyId{0x14031CB80, 0x1404568F0};
	WEAK symbol<bool(VariableValue* value)> Scr_CastString{0x0, 0x140451710};

	WEAK symbol<unsigned __int16(int handle, unsigned int paramcount)> Scr_ExecThread{0x0, 0x140455560};
	WEAK symbol<unsigned int(const char* name)> Scr_LoadScript{0x0, 0x14044B640};
	WEAK symbol<unsigned int(const char* script, unsigned int name)> Scr_GetFunctionHandle{0x0, 0x14044B4C0};
	WEAK symbol<unsigned int(void* func, int type, unsigned int name)> Scr_RegisterFunction{0x1403115B0, 0x14044AE70};

	WEAK symbol<unsigned int(unsigned int localId, const char* pos, unsigned int paramcount)> VM_Execute{0x0, 0x140457630};
	WEAK symbol<void()> Scr_ErrorInternal{0x0, 0x1404554E0};

	WEAK symbol<const char*(scr_string_t stringValue)> SL_ConvertToString{0x140314850, 0x14044E1C0};
	WEAK symbol<scr_string_t(const char* str)> SL_FindString{0x140314AF0, 0x14044E490};
	WEAK symbol<scr_string_t(const char* str, unsigned int user)> SL_GetString{0x140314D90, 0x14044E730};
	WEAK symbol<unsigned int(char const* str)> SL_GetCanonicalString{0x140311770, 0x14044B030};

	WEAK symbol<void(int arg, char* buffer, int bufferLength)> SV_Cmd_ArgvBuffer{0x1402EEFD0, 0x14040D620};
	WEAK symbol<void(const char* text_in)> SV_Cmd_TokenizeString{0, 0x14040D6A0};
	WEAK symbol<void()> SV_Cmd_EndTokenizedString{0, 0x14040D660};

	WEAK symbol<mp::gentity_s*(const char* name)> SV_AddBot{0, 0x1404962B0};
	WEAK symbol<bool(int clientNum)> SV_BotIsBot{0, 0x140483670};
	WEAK symbol<const char*()> SV_BotGetRandomName{0, 0x140482AB0};
	WEAK symbol<mp::gentity_s*(int)> SV_AddTestClient{0, 0x140496550};
	WEAK symbol<bool(mp::gentity_s*)> SV_CanSpawnTestClient{0, 0x140496800};
	WEAK symbol<int(mp::gentity_s* ent)> SV_SpawnTestClient{0, 0x14049A0E0};

	WEAK symbol<void(mp::gentity_s*)> SV_AddEntity{0, 0x140393570};

	WEAK symbol<void(netadr_s* from)> SV_DirectConnect{0, 0x140496F00};
	WEAK symbol<void(mp::client_t* client)> SV_DropClient{0, 0x140495F10};
	WEAK symbol<void(mp::client_t*, const char*, int)> SV_ExecuteClientCommand{0, 0x15121D8E6};
	WEAK symbol<void(int localClientNum)> SV_FastRestart{0, 0x1404948F0};
	WEAK symbol<void(int clientNum, svscmd_type type, const char* text)> SV_GameSendServerCommand{0x1403F3A70, 0x14049BAB0};
	WEAK symbol<const char*(int clientNum)> SV_GetGuid{0, 0x14049BB70};
	WEAK symbol<int(int clientNum)> SV_GetClientPing{0, 0x14049BB50};
	WEAK symbol<playerState_s*(int num)> SV_GetPlayerstateForClientNum{0x1403F3AB0, 0x14049BBF0};
	WEAK symbol<void(int clientNum, const char* reason)> SV_KickClientNum{0, 0x140494C60};
	WEAK symbol<bool()> SV_Loaded{0x1403F42C0, 0x14049D540};
	WEAK symbol<bool(const char* map)> SV_MapExists{0, 0x140494CC0};
	WEAK symbol<void(int localClientNum, const char* map, bool mapIsPreloaded)> SV_StartMap{0, 0x1404957E0};
	WEAK symbol<void(int localClientNum, const char* map, bool mapIsPreloaded, bool migrate)> SV_StartMapForParty{0, 0x140495910};

	WEAK symbol<void(int index, const char* string)> SV_SetConfigstring{0, 0x14049D810};

	WEAK symbol<void(char* path, int pathSize, Sys_Folder folder, const char* filename, const char* ext)>
	Sys_BuildAbsPath{0x14037BBE0, 0x14052F560};
	WEAK symbol<HANDLE(int folder, const char* baseFileName)> Sys_CreateFile{0x14037BCA0, 0x14052F610};
	WEAK symbol<void(const char* error, ...)> Sys_Error{0x14038C770, 0x140539670};
	WEAK symbol<bool(const char* path)> Sys_FileExists{0x14038C810, 0x140539720};
	WEAK symbol<bool()> Sys_IsDatabaseReady2{0x1402FF980, 0x14043EED0};
	WEAK symbol<int()> Sys_Milliseconds{0x14038E9F0, 0x14053BB00};
	WEAK symbol<bool(int, void const*, const netadr_s*)> Sys_SendPacket{0x14038E720, 0x14053B810};
	WEAK symbol<void(Sys_Folder, const char* path)> Sys_SetFolder{0x14037BDD0, 0x14052F780};
	WEAK symbol<void()> Sys_ShowConsole{0x14038FA90, 0x14053CC30};
	WEAK symbol<bool()> Sys_IsMainThread{0x1402FF9C0, 0x14043EF10};

	WEAK symbol<const char*(const char*)> UI_GetMapDisplayName{0, 0x14040ED80};
	WEAK symbol<const char*(const char*)> UI_GetGameTypeDisplayName{0, 0x14040E750};
	WEAK symbol<void(unsigned int localClientNum, const char** args)> UI_RunMenuScript{0, 0x1404F31D0};
	WEAK symbol<int(const char* text, int maxChars, Font_s* font, float scale)> UI_TextWidth{0, 0x1404F5980};

	WEAK symbol<const char*()> SEH_GetCurrentLanguageName{0x140339300, 0x1404D3050};

	WEAK symbol<void*(unsigned int size, unsigned int alignment, unsigned int type, int source)> PMem_AllocFromSource_NoDebug{0x1403775F0, 0x14052A050};
	WEAK symbol<void*(unsigned int size)> Hunk_AllocateTempMemoryHighInternal{0x140369D60, 0x14051A3A0};

	WEAK symbol<void*(jmp_buf* Buf, int Value)> longjmp{0x14059C5C0, 0x140732BEC};
	WEAK symbol<int(jmp_buf* Buf)> _setjmp{0x14059CD00, 0x140732C22};

	/***************************************************************
	 * Variables
	 **************************************************************/

	WEAK symbol<int> keyCatchers{0x1413D5B00, 0x141BD623C};
	WEAK symbol<PlayerKeyState> playerKeys{0x1413BC5DC, 0x141BD323C};

	WEAK symbol<CmdArgs> cmd_args{0x1492EC6F0, 0x147E35160};
	WEAK symbol<CmdArgs> sv_cmd_args{0x1492EC7A0, 0x147E35210};
	WEAK symbol<cmd_function_s*> cmd_functions{0x1492EC848, 0x147E352B8};

	WEAK symbol<int> dvarCount{0x14A7BFF34, 0x14B329B40};
	WEAK symbol<dvar_t*> sortedDvars{0x14A7BFF50, 0x14B329B50};

	WEAK symbol<unsigned int> levelEntityId{0x149AF55B0, 0x1485A64A0};
	WEAK symbol<int> g_script_error_level{0x14A1917A8, 0x148C4AA54};
	WEAK symbol<jmp_buf> g_script_error{0x14A1917B0, 0x148C4AB70};
	WEAK symbol<scr_classStruct_t> g_classMap{0x14080A840, 0x140911CB0};

	WEAK symbol<scrVarGlob_t> scr_VarGlob{0x149B1D680, 0x1485CE500};
	WEAK symbol<scrVmPub_t> scr_VmPub{0x14A1938C0, 0x148C42520};
	WEAK symbol<function_stack_t> scr_function_stack{0x14A19DE40, 0x148C4ECC0};

	WEAK symbol<const char*> command_whitelist{0x140808EF0, 0x14090CA50};

	WEAK symbol<SOCKET> query_socket{0, 0x14B5B83E0};

	WEAK symbol<int> level_time{0x0, 0x144DA4DCC};

	WEAK symbol<void*> DB_XAssetPool{0x140804690, 0x140908510};
	WEAK symbol<unsigned int> db_hashTable{0x142C3E050, 0x143B41F10};
	WEAK symbol<XAssetEntry> g_assetEntryPool{0x142CC2400, 0x143BECD00};
	WEAK symbol<int> g_poolSize{0x140804140, 0x140907DA0};
	WEAK symbol<const char*> g_assetNames{0x140803C90, 0x140907260};

	WEAK symbol<DWORD> threadIds{0x149632EC0, 0x148217000};

	WEAK symbol<GfxDrawMethod_s> gfxDrawMethod{0x14CDFAFE8, 0x14D80F0B0};

	WEAK symbol<unsigned int> tls_index{0x14F65DAF0, 0x150085C44};

	namespace mp
	{
		WEAK symbol<gentity_s> g_entities{0, 0x144BA12E0};
		WEAK symbol<client_t> svs_clients{0, 0x149B0CF90};
		WEAK symbol<int> svs_numclients{0, 0x149B0CF8C};

		WEAK symbol<int> gameTime{0, 0x144DA4DCC};
		WEAK symbol<int> serverTime{0, 0x149B0CF80};

		WEAK symbol<int> ping{0, 0x141BDB634};

		WEAK symbol<int> sv_serverId_value{0, 0x148CF1EF0};

		WEAK symbol<char> virtualLobby_loaded{0, 0x141BD61C5};
	}

	namespace sp
	{
		WEAK symbol<gentity_s> g_entities{0x143C26DC0, 0};
	}

	namespace hks
	{
		WEAK symbol<lua_State*> lua_state{0, 0x1416DBB88};
		WEAK symbol<void(lua_State* s, const char* str, unsigned int l)> hksi_lua_pushlstring{0, 0x14007B2F0};
		WEAK symbol<HksObject*(HksObject* result, lua_State* s, const HksObject* table, const HksObject* key)> hks_obj_getfield{0, 0x1400F3FF0};
		WEAK symbol<void(lua_State* s, const HksObject* tbl, const HksObject* key, const HksObject* val)> hks_obj_settable{0, 0x1400F50D0};
		WEAK symbol<HksObject* (HksObject* result, lua_State* s, const HksObject* table, const HksObject* key)> hks_obj_gettable{0, 0x1400F4500};
		WEAK symbol<void(lua_State* s, int nargs, int nresults, const unsigned int* pc)> vm_call_internal{0, 0x140121C10};
		WEAK symbol<HashTable*(lua_State* s, unsigned int arraySize, unsigned int hashSize)> Hashtable_Create{0, 0x1400E25A0};
		WEAK symbol<void(lua_State* s, lua_function function, int num_upvalues, const char* name, int internal_, int profilerTreatClosureAsFunc)> hksi_lua_pushcclosure{0, 0x1400F6620};
		WEAK symbol<int(lua_State* s, int t)> hksi_luaL_ref{0, 0x1400FF5C0};
		WEAK symbol<void(lua_State* s, int t, int ref)> hksi_luaL_unref{0, 0x1400F76F0};
		WEAK symbol<int(lua_State* s, const HksCompilerSettings* options, const char* buff, unsigned __int64 sz, const char* name)> hksi_hksL_loadbuffer{0, 0x1400F5A10};
		WEAK symbol<int(lua_State* s, const char* what, lua_Debug* ar)> hksi_lua_getinfo{0, 0x1400F7C60};
		WEAK symbol<int(lua_State* s, int level, lua_Debug* ar)> hksi_lua_getstack{0, 0x1400F85F0};
		WEAK symbol<void(lua_State* s, const char* fmt, ...)> hksi_luaL_error{0, 0x1400F73B0};
		WEAK symbol<const char*> s_compilerTypeName{0, 0x1408FFC80};
	}
}
