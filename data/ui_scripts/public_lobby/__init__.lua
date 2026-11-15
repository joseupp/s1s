if game:issingleplayer() or not Engine.InFrontend() then
    return
end

if not Engine.GetDvarBool("public_lobby_buttons") then
    return
end

local original_localize = Engine.Localize

local localize_aliases = {
    ["@PLATFORM_UI_HEADER_PLAY_MP_CAPS"] = "@LUA_MENU_LOCAL_PLAY_CAPS",
    ["@PLATFORM_FIND_GAME"] = "@MENU_TRAINING",
    ["@PLATFORM_PLAY_ONLINE"] = "@MENU_ONLINEVAULT_PLAY",
    ["@XBOXLIVE_POPUP_CONNECTION"] = "@MENU_LOADING_DOTS"
}

Engine.Localize = function(text, ...)
    if select("#", ...) == 0 and type(text) == "string" and localize_aliases[text] then
        return original_localize(localize_aliases[text])
    end

    return original_localize(text, ...)
end

local lobby_max_players = 18
local public_max_players = 12

local function in_public_lobby()
    return not Engine.GetDvarBool("xblive_privatematch") and not Engine.GetSystemLink()
end

local function gametype_name(gametype)
    return Engine.Localize(Engine.TableLookup(GameTypesTable.File, GameTypesTable.Cols.Ref, gametype,
        GameTypesTable.Cols.Name) or "")
end

local function is_team_based(gametype)
    local team_choice = tonumber(Engine.TableLookup(GameTypesTable.File, GameTypesTable.Cols.Ref, gametype,
        GameTypesTable.Cols.TeamChoice))
    return team_choice == 1
end

if GetMaxBotLimit then
    local original_get_max_bot_limit = GetMaxBotLimit
    GetMaxBotLimit = function(...)
        if in_public_lobby() and Engine.InLobby() then
            return lobby_max_players - 1
        end

        return original_get_max_bot_limit(...)
    end
end

local original_gametype_name = Lobby.GameTypeNameAbbreviated
Lobby.GameTypeNameAbbreviated = function(...)
    if in_public_lobby() and Engine.InLobby() then
        return gametype_name(Engine.GetDvarString("ui_gametype"))
    end

    return original_gametype_name(...)
end

local original_entered_lobby = Lobby.EnteredLobby
if original_entered_lobby then
    Lobby.EnteredLobby = function(...)
        Engine.Exec("lobbybots_lobbyopened")
        return original_entered_lobby(...)
    end
end

local original_vote_gametype = Lobby.GetVoteMapGametype
Lobby.GetVoteMapGametype = function(...)
    if in_public_lobby() and Engine.InLobby() then
        return gametype_name(Engine.GetDvarString("ui_gametype"))
    end

    return original_vote_gametype(...)
end

local sent_gametype = nil
local default_gametype = "war"

local function apply_match_rules()
    if not MatchRules.IsUsingMatchRulesData() then
        MatchRules.SetUsingMatchRulesData(1)
    end

    local gametype = MatchRules.GetData("gametype")
    if not gametype or gametype == "" then
        gametype = default_gametype
        MatchRules.SetData("gametype", gametype)
    end

    if not gametype or gametype == "" then
        return
    end

    if sent_gametype ~= gametype then
        sent_gametype = gametype
        Engine.Exec("lobby_gametype " .. gametype .. " " .. gametype_name(gametype))
    end

    if Engine.GetDvarString("ui_gametype") ~= gametype then
        Engine.SetDvarString("ui_gametype", gametype)
        Engine.ExecNow("xupdatepartystate")
    end
end

local function set_default_bots()
    local friendly = Lobby.GetBotsTeamLimit(Lobby.BotTeams.Friendly)
    local enemy = Lobby.GetBotsTeamLimit(Lobby.BotTeams.Enemy)
    if friendly > 0 or enemy > 0 then
        return
    end

    local total = public_max_players - 1
    if is_team_based(Engine.GetDvarString("ui_gametype")) then
        local half = math.floor(total / 2)
        Lobby.SetBotsTeamLimit(Lobby.BotTeams.Friendly, half)
        Lobby.SetBotsTeamLimit(Lobby.BotTeams.Enemy, total - half)
    else
        Lobby.SetBotsTeamLimit(Lobby.BotTeams.FFA, total)
    end

    Lobby.SetBotsDifficulty(Lobby.BotTeams.FFA, Lobby.BotDifficulties.Mixed)
end

local function count_bots()
    local friendly = Lobby.GetBotsTeamLimit(Lobby.BotTeams.Friendly)
    if is_team_based(Engine.GetDvarString("ui_gametype")) then
        return friendly + Lobby.GetBotsTeamLimit(Lobby.BotTeams.Enemy)
    end

    return friendly
end

local chosen_map = nil
local map_from_cancel = false
local map_before_setup = nil

local function vote_count(entry)
    if MapVoting and MapVoting[entry] and MapVoting[entry].votesDvar then
        return Engine.GetDvarInt(MapVoting[entry].votesDvar) or 0
    end

    return 0
end

local function get_vote_map()
    local map_a = Engine.GetDvarString("ui_mapvote_entrya_mapname")
    local map_b = Engine.GetDvarString("ui_mapvote_entryb_mapname")
    local votes = { vote_count("MapA"), vote_count("MapB"), vote_count("Random") }

    local winner = 1
    for i = 2, #votes do
        if votes[i] > votes[winner] then
            winner = i
        end
    end

    if winner == 1 then
        return map_a
    elseif winner == 2 then
        return map_b
    end

    local excluded = {
        [map_a or ""] = true,
        [map_b or ""] = true,
        [""] = true,
        bonus_map = true,
        more_maps = true,
        mp_virtual_lobb = true,
        mp_vlobby_refra = true,
        mp_vlobby_laser = true,
        mp_vlobby_room = true
    }

    local show_dlc = Engine.GetDvarBool("ui_showDLCMaps")
    Engine.SetDvarBool("ui_showDLCMaps", true)

    local maps = {}
    for i = 0, Lobby.GetMapFeederCount() - 1 do
        local map = Lobby.GetMapLoadNameByIndex(i)
        if map and not excluded[map] then
            maps[#maps + 1] = map
        end
    end

    Engine.SetDvarBool("ui_showDLCMaps", show_dlc)

    if #maps > 0 then
        return maps[math.random(1, #maps)]
    end

    if map_a and map_a ~= "" and map_b and map_b ~= "" then
        return math.random(1, 2) == 1 and map_a or map_b
    end

    return (map_a and map_a ~= "") and map_a or map_b
end

local function apply_map()
    local map = chosen_map or get_vote_map()
    if map and map ~= "" then
        Engine.SetDvarString("ui_mapname", map)
        return map
    end

    return ""
end

local function check_chosen_map()
    if map_before_setup == nil then
        return
    end

    local map = Engine.GetDvarString("ui_mapname")
    if map ~= map_before_setup then
        chosen_map = map
        map_from_cancel = false
        map_before_setup = nil
    end
end

local start_requested = false
local started_map = ""

local function refresh_lobby(menu)
    apply_match_rules()
    check_chosen_map()

    if chosen_map and chosen_map ~= "" and map_before_setup == nil and not start_requested
        and Engine.GetDvarString("ui_mapname") ~= chosen_map then
        Engine.SetDvarString("ui_mapname", chosen_map)
    end

    local team_based = is_team_based(Engine.GetDvarString("ui_gametype"))
    if menu.sent_team_based ~= team_based then
        menu.sent_team_based = team_based
        Engine.Exec("lobbybots_teambased " .. (team_based and "1" or "0"))
    end

    if not menu.default_bots_done and Lobby.IsGameHost() then
        menu.default_bots_done = true
        set_default_bots()
    end

    local max_players = math.min(math.max(count_bots() + 1, 2), lobby_max_players)
    if Engine.GetDvarInt("party_maxplayers") ~= max_players then
        Engine.SetDvarInt("party_maxplayers", max_players)
    end

    if menu.title then
        menu.title:setText(gametype_name(Engine.GetDvarString("ui_gametype")))
        menu.title:setTextStyle(CoD.TextStyle.ForceUpperCase)
    end
end


local function start_game(element, event)
    Lobby.SetBotsConnectType(0)

    Engine.SetDvarInt("sv_maxclients", lobby_max_players)

    Engine.SetDvarInt("pt_pregameStartTimerLength", 3)
    Engine.SetDvarInt("pt_gameStartTimerLength", 5)

    apply_match_rules()
    check_chosen_map()
    map_before_setup = nil
    local gametype = MatchRules.GetData("gametype")
    if not gametype or gametype == "" then
        gametype = Engine.GetDvarString("ui_gametype")
    end

    started_map = apply_map()
    Engine.Exec("lobby_partygo " .. started_map .. " " .. gametype)
    start_requested = true
end

local function game_setup(element, event)
    map_before_setup = Engine.GetDvarString("ui_mapname")
    LUI.FlowManager.RequestAddMenu(element, "gamesetup_menu_main", true, event.controller, false)
end

local original_party_status = Lobby.GetPartyStatus
Lobby.GetPartyStatus = function(...)
    local status = original_party_status(...)
    if status ~= "" and not start_requested and in_public_lobby() and Engine.InLobby() and Lobby.IsGameHost() then
        return Engine.Localize("@MENU_READY_TO_START_GAME")
    end

    return status
end

local function handle_back(menu, element, event)
    if start_requested then
        start_requested = false
        Engine.Exec("lobby_cancelstartserver")

        if started_map ~= "" then
            if chosen_map ~= started_map then
                map_from_cancel = true
            end
            chosen_map = started_map
            Engine.SetDvarString("ui_mapname", started_map)
        end
        return
    end

    PromptToLeaveLobby(menu, event or element)
end

local function on_leave_lobby(element)
    chosen_map = nil
    map_before_setup = nil
    LUI.MarketingPanel.ClearViewedMessages({
        LUI.MarketingLocation.Lobby
    })
    LUI.FlowManager.RequestLeaveMenu(element)
end

local function menu_xboxlive_lobby(controller, args)
    local open_summary = Engine.GetDvarBool("ui_opensummary") == true

    local menu = LUI.MPLobbyBase.new(controller, {
        menu_title = "@PLATFORM_UI_HEADER_PLAY_MP_CAPS",
        has_match_summary = true,
        has_new_item_usage_widget = true
    }, true)

    menu:setClass(LUI.MPLobbyPublic)

    if Engine.IsCoreMode() then
        local function is_starting()
            return start_requested
        end

        local start_button = menu:AddButton("@LUA_MENU_START_GAME", start_game, is_starting)
        local setup_button = menu:AddButton("@LUA_MENU_GAME_SETUP", game_setup, is_starting)
        if start_button then
            start_button:setDisabledRefreshRate(100)
        end
        if setup_button then
            setup_button:setDisabledRefreshRate(100)
        end
        menu:AddNewItemsWidget()
        menu:AddCACButton()
        menu:AddCAOButton()
        menu:AddArmoryButton()
    end

    menu:AddOptionsButton()

    if not menu:CheckAddMapAndMarketingPanels(open_summary) then
        menu:registerEventHandler("CheckAddMapAndMarketingPanels", function(element, event)
            LUI.MPLobbyPublic.CheckAddMapAndMarketingPanels(element, open_summary)
        end)

        local timer = LUI.UITimer.new(100, "CheckAddMapAndMarketingPanels")
        timer.id = "MPLobbyPublic_add_map_timer"
        menu.mapTimer = timer
        menu:addElement(timer)
    end

    start_requested = false
    menu.HandleBack = handle_back

    if map_from_cancel then
        chosen_map = nil
        map_from_cancel = false
    end

    refresh_lobby(menu)
    menu:registerEventHandler("apply_match_rules", refresh_lobby)
    local rules_timer = LUI.UITimer.new(500, "apply_match_rules")
    rules_timer.id = "public_lobby_rules_timer"
    menu:addElement(rules_timer)

    menu:registerEventHandler("exit_public_lobby", on_leave_lobby)
    menu:registerEventHandler("player_joined", Cac.PlayerJoinedEvent)
    menu:registerEventHandler("loadout_request", Cac.PlayerJoinedEvent)
    Lobby.EnteredLobby()

    if open_summary then
        LUI.InventoryUtils.ProcessLootExpiration()
    end

    return menu
end

LUI.MenuBuilder.m_types_build["menu_xboxlive_lobby"] = menu_xboxlive_lobby

LUI.FlowManager.RegisterMenuStack("menu_xboxlive_lobby", function()
    return {
        "mp_main_menu",
        "menu_xboxlive"
    }
end)

local default_playlist = 1

local function on_public_match(element, event)
    if not Engine.GetOnlineGame() then
        return
    end

    Engine.Exec("xcheckezpatch")
    Engine.ExecNow("xblive_privatematch 0")
    Engine.SetDvarInt("playlist", default_playlist)

    sent_gametype = nil
    apply_match_rules()

    LUI.FlowManager.RequestAddMenu(element, "menu_xboxlive_lobby", true, event.controller, false, {})
    Engine.Exec(MPConfig.default_xboxlive, event.controller)
    Engine.Exec("xstartlobby")
end

if LUI.mp_menus then
    for name, module in pairs(LUI.mp_menus) do
        if type(name) == "string" and name:lower() == "mplobbyonline" and type(module) == "table" then
            rawset(module, "OnPublicMatch", on_public_match)
        end
    end
end

local function patch_member_screen(screen)
    local original_update = screen.UpdateMemberList
    if type(original_update) ~= "function" then
        return
    end

    local function team_cycle(setting, direction, controller)
        if start_requested then
            return
        end

        local ops = screen.GetAvailableTeamOps()
        local current = nil
        for i = 1, #ops do
            if setting.curTeamSetting == ops[i] then
                current = i
                break
            end
        end

        local index = 1
        if current then
            index = current + direction
            if index > #ops then
                index = 1
            elseif index < 1 then
                index = #ops
            end
        end

        screen.MemberListTeamSet(setting, ops[index], controller)
    end

    local function public_team_init(menu, list)
        local setting = menu:getChildById("team_setting")
        if not setting then
            return
        end

        if setting.teamSettingInited then
            screen.MemberListTeamRefresh(setting)
            return
        end

        local controller = Engine.GetFirstActiveController()
        local nav_bumpers = LUI.NavBumpers.new({
            leftAnchor = true,
            rightAnchor = true,
            topAnchor = false,
            bottomAnchor = true,
            height = CoD.TextSettings.TitleFontMedium.Height
        }, function(element, event)
            team_cycle(setting, -1, event.controller or controller)
        end, function(element, event)
            team_cycle(setting, 1, event.controller or controller)
        end)
        setting:addElement(nav_bumpers)
        setting.navBumpers = nav_bumpers

        setting.teamText = setting:getChildById("team_text")
        setting.parentMenu = menu.parentMenu
        setting.controller = controller
        setting.teamSettingActive = true
        screen.MemberListTeamSet(setting, screen.MemberListInitTeamOp(list, controller), controller)
        screen.MemberListTeamRefreshVisibility(setting)
        setting.teamSettingInited = true
    end

    rawset(screen, "UpdateMemberList", function(menu, event)
        original_update(menu, event)

        local props = menu.properties
        if not props or not props.isPublicLobby or not Engine.IsCoreMode()
            or props.memberListState == Lobby.MemberListStates.Prelobby then
            return
        end

        local root = Engine.GetLuiRoot()
        if not root or not root:IsMenuInStack("menu_xboxlive_lobby") then
            return
        end

        local holder = menu:getChildById("member_list_holder")
        local list = holder and holder:getChildById("member_screen_list_id")
        if not list then
            return
        end

        public_team_init(menu, list)

        local setting = menu:getChildById("team_setting")
        if setting and setting.navBumpers then
            local enabled = not start_requested and #screen.GetAvailableTeamOps() > 1
            LUI.NavBumpers.SetBumpersEnabled(setting.navBumpers, enabled, enabled)
        end

        local status = menu:getChildById("status_text")
        if status then
            status:setText("")
        end
    end)
end

if LUI.mp_menus then
    for name, module in pairs(LUI.mp_menus) do
        if type(name) == "string" and name:lower() == "s1memberscreen" and type(module) == "table" then
            patch_member_screen(module)
        end
    end
end
