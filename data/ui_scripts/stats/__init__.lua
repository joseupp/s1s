if (game:issingleplayer() or not Engine.InFrontend()) then
	return
end

game:addlocalizedstring("LUA_MENU_STATS", "Estadisticas")
game:addlocalizedstring("LUA_MENU_STATS_DESC", "Edita las estadisticas del jugador.")

game:addlocalizedstring("LUA_MENU_UNLOCKALL_ITEMS", "Desbloquea las estadísticas")
game:addlocalizedstring("LUA_MENU_UNLOCKALL_ITEMS_DESC",
	"Si los elementos deben bloquearse en función de las estadísticas del jugador o siempre desbloquearse.")

game:addlocalizedstring("LUA_MENU_UNLOCKALL_ITEMS2", "Desbloquea el botin de suministros")
game:addlocalizedstring("LUA_MENU_UNLOCKALL_ITEMS2_DESC",
	"Desbloquea todo el botin de sumunistros.")

game:addlocalizedstring("LUA_MENU_UNLOCKALL_CLASSES", "Desbloquea todas las clases")
game:addlocalizedstring("LUA_MENU_UNLOCKALL_CLASSES_DESC",
	"Si las clases deben bloquearse en función de las estadísticas del jugador o siempre desbloquearse.")

game:addlocalizedstring("LUA_MENU_PRESTIGE", "Prestigio")
game:addlocalizedstring("LUA_MENU_PRESTIGE_DESC", "Edita el nivel de prestigio")
game:addlocalizedstring("LUA_MENU_RANK", "Nivel")
game:addlocalizedstring("LUA_MENU_RANK_DESC", "Edita el nivel")
local operatormenu = LUI.MenuBuilder.m_types_build["OperatorMenu"]
if operatormenu then
	LUI.MenuBuilder.m_types_build["OperatorMenu"] = function(a1, a2)
		local menu = operatormenu(a1, a2)
		menu:AddButton("@LUA_MENU_STATS", function(b1, b2)
			LUI.FlowManager.RequestAddMenu(b1, "menu_stats", true, b2.controller)
		end)
		return menu
	end
end
-- button stuff for configuring
function IsEnabled(dvar)
	local enabled = Engine.GetDvarBool(dvar)
	if enabled then
		return Engine.Localize("@LUA_MENU_ENABLED")
	end

	return Engine.Localize("@LUA_MENU_DISABLED")
end

function ToggleEnable(dvar)
	local enabled = Engine.GetDvarBool(dvar)
	if enabled then
		Engine.SetDvarBool(dvar, false)
	else
		Engine.SetDvarBool(dvar, true)
	end
end

function GoDirection(dvar, direction, callback)
	local value = Engine.GetDvarString(dvar)
	value = tonumber(value)

	-- get rank data
	local max = nil
	if (dvar == "ui_rank_level_") then
		max = Rank.GetMaxRank(CoD.PlayMode.Core)
	elseif (dvar == "ui_prestige_level") then
		max = Lobby.GetMaxPrestigeLevel()
	end

	local new_value = nil
	if (direction == "down") then
		new_value = value - 1
	elseif (direction == "up") then
		new_value = value + 1
	end

	-- checking to make sure its < 0 or > max
	if (new_value < 0) then
		new_value = max
	elseif (new_value > max) then
		new_value = 0
	end

	callback(new_value)

	Engine.SetDvarFromString(dvar, new_value .. "")
end

LUI.MenuBuilder.registerType("menu_stats", function(a1, a2)
	local menu = LUI.MenuTemplate.new(a1, {
		menu_title = Engine.ToUpperCase(Engine.Localize("@LUA_MENU_STATS")),
		menu_width = GenericMenuDims.menu_right_wide - GenericMenuDims.menu_left,
		menu_height = 548
	})

	menu:setClass(LUI.Options)
	menu.controller = a2.exclusiveController

	local itemsbutton = menu:AddButtonVariant(GenericButtonSettings.Variants.Select,
		"@LUA_MENU_UNLOCKALL_ITEMS", "@LUA_MENU_UNLOCKALL_ITEMS_DESC", function()
			return IsEnabled("cg_unlockall_items")
		end, function()
			ToggleEnable("cg_unlockall_items")
		end, function()
			ToggleEnable("cg_unlockall_items")
		end)
	
		local itemsbutton = menu:AddButtonVariant(GenericButtonSettings.Variants.Select,
		"@LUA_MENU_UNLOCKALL_ITEMS2", "@LUA_MENU_UNLOCKALL_ITEMS2_DESC", function()
			return IsEnabled("cg_unlockall_items2")
		end, function()
			ToggleEnable("cg_unlockall_items2")
		end, function()
			ToggleEnable("cg_unlockall_items2")
		end)
	

	local classesbutton = menu:AddButtonVariant(GenericButtonSettings.Variants.Select,
		"@LUA_MENU_UNLOCKALL_CLASSES", "@LUA_MENU_UNLOCKALL_CLASSES_DESC", function()
			return IsEnabled("cg_unlockall_classes")
		end, function()
			ToggleEnable("cg_unlockall_classes")
		end, function()
			ToggleEnable("cg_unlockall_classes")
		end)

	local prestige = Engine.GetPlayerDataEx(0, CoD.StatsGroup.Ranked, "prestige") or 0
	local experience = Engine.GetPlayerDataEx(0, CoD.StatsGroup.Ranked, "experience") or 0
	local rank = AAR.GetRankForXP(experience, prestige)

	local prestigevalue = prestige
	local rankvalue = rank

	-- save changes made
	local save_changes = function()
		Engine.SetPlayerDataEx(0, CoD.StatsGroup.Ranked, "prestige", tonumber(prestigevalue))

		local rank = tonumber(rankvalue)
		local prestige = Engine.GetPlayerDataEx(0, CoD.StatsGroup.Ranked, "prestige") or 0
		local experience = rank == 0 and 0 or Rank.GetRankMaxXP(tonumber(rankvalue) - 1, prestige)

		Engine.SetPlayerDataEx(0, CoD.StatsGroup.Ranked, "experience", experience)
	end

	-- back callback
	local back = function()
		save_changes()
		LUI.common_menus.Options.HideOptionsBackground()
		LUI.FlowManager.RequestLeaveMenu(menu)
	end

	-- create buttons and create callbacks
	CreateEditButton(menu, "ui_prestige_level", "@LUA_MENU_PRESTIGE", "@LUA_MENU_PRESTIGE_DESC", function(value)
		prestigevalue = value
	end)
	CreateEditButton(menu, "ui_rank_level_", "@LUA_MENU_RANK", "@LUA_MENU_RANK_DESC", function(value)
		rankvalue = value
	end)

	menu:AddBottomDescription(menu:InitScrolling())
	menu:AddBackButton(back)

	LUI.common_menus.Options.ShowOptionsBackground()

	return menu
end)

function CreateEditButton(menu, dvar, name, desc, callback)
	local prestige = Engine.GetPlayerDataEx(0, CoD.StatsGroup.Ranked, "prestige") or 0
	local experience = Engine.GetPlayerDataEx(0, CoD.StatsGroup.Ranked, "experience") or 0

	local dvarValue = nil
	local max = nil
	if (dvar == "ui_rank_level_") then
		dvarValue = AAR.GetRankForXP(experience, prestige)
		max = Rank.GetMaxRank(CoD.PlayMode.Core)
	elseif (dvar == "ui_prestige_level") then
		dvarValue = prestige
		max = Lobby.GetMaxPrestigeLevel()
	end

	Engine.SetDvarFromString(dvar, dvarValue .. "")

	menu:AddButtonVariant(GenericButtonSettings.Variants.Select, name, desc, function()
		local dvarString = Engine.GetDvarString(dvar)

		if (dvar == "ui_rank_level_") then
			dvarString = tonumber(dvarString) + 1
		end

		return tostring(dvarString)
	end, function()
		GoDirection(dvar, "down", callback)
	end, function()
		GoDirection(dvar, "up", callback)
	end)
end

local isclasslocked = Cac.IsCustomClassLocked
Cac.IsCustomClassLocked = function(...)
	if (Engine.GetDvarBool("cg_unlockall_classes")) then
		return false
	end

	return isclasslocked(table.unpack({...}))
end

local isdlcclasslocked = Cac.IsCustomClassDlcLocked
Cac.IsCustomClassDlcLocked = function(...)
	if (Engine.GetDvarBool("cg_unlockall_classes")) then
		return false
	end

	return isdlcclasslocked(table.unpack({...}))
end

local function get_required_items(ref)
	return Engine.TableLookup(UnlockTable.File, UnlockTable.Cols.ItemId, ref, UnlockTable.Cols.RequiredItems) or ""
end

local variant_cache = {}

local function find_weapon_entry(node, ref, depth)
	if type(node) ~= "table" or depth > 6 then
		return nil
	end

	if node[1] == ref and type(node[2]) == "number" then
		return node
	end

	for _, child in pairs(node) do
		local entry = find_weapon_entry(child, ref, depth + 1)
		if entry then
			return entry
		end
	end

	return nil
end

local function get_weapon_variants(ref)
	if variant_cache[ref] then
		return variant_cache[ref]
	end

	local variants = {}
	local entry = Cac.Weapons and find_weapon_entry(Cac.Weapons, ref, 0)
	if entry then
		for index = 0, entry[2] - 1 do
			variants[#variants + 1] = ref .. "loot" .. index
		end

		local index = 3
		while entry[index] do
			variants[#variants + 1] = entry[index]
			index = index + 1
		end
	end

	variant_cache[ref] = variants
	return variants
end

local function owns_weapon_variant(controller, ref)
	if type(ref) ~= "string" or ref == "" or get_required_items(ref) == "" then
		return false
	end

	for _, variant in ipairs(get_weapon_variants(ref)) do
		if Engine.Inventory_IsItemUsableForPlayer(controller, variant) then
			return true
		end
	end

	return false
end

local getitemlockstate = Cac.GetItemLockState
Cac.GetItemLockState = function(controller, guid, ref, ...)
	local state, status, text = getitemlockstate(controller, guid, ref, ...)

	if state ~= "Unlocked" and status == Cac.ItemLockStatus.DlcRequired then
		return "Unlocked", Cac.ItemLockStatus.Unlocked or 0, ""
	end

	if state ~= "Unlocked" then
		local item_ref = ref
		if not item_ref and guid then
			item_ref = Engine.LookupStatsTableColumnForGUID(guid, StatsTable.Cols.Ref)
		end

		local player = controller or Engine.GetFirstActiveController()
		if owns_weapon_variant(player, item_ref) then
			return "Unlocked", Cac.ItemLockStatus.Unlocked or 0, ""
		end
	end

	return state, status, text
end
