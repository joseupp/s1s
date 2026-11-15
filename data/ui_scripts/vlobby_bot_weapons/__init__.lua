if not Lobby or not Lobby.GetMemberLoadout then
	return
end

S1SVLobbyOriginals = S1SVLobbyOriginals or {}
if not S1SVLobbyOriginals.GetMemberLoadout then
	S1SVLobbyOriginals.GetMemberLoadout = Lobby.GetMemberLoadout
end

local get_bot_weapon = function(state, slot, team)
	local ok, name = pcall(Lobby.GetMemberName, state, slot, team)
	if not ok or not name or name == "" then
		return nil
	end

	local ok2, value = pcall(Engine.GetDvarString, "vlbot_wpn_" .. name)
	if not ok2 or not value or value == "" then
		return nil
	end

	return tonumber(value)
end

rawset(Lobby, "GetMemberLoadout", function(state, slot, team, controller, ...)
	local loadout = S1SVLobbyOriginals.GetMemberLoadout(state, slot, team, controller, ...)
	if loadout then
		local weapon = get_bot_weapon(state, slot, team)
		if weapon then
			loadout.primary = weapon
		end
	end

	return loadout
end)
