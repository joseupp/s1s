if (game:issingleplayer()) then
	return
end

game:addlocalizedstring("LUA_MENU_MEDAL_SOUND", "Sonido de medallas")
game:addlocalizedstring("LUA_MENU_MEDAL_SOUND_DESC", "Activa o desactiva el sonido de las medallas en la partida")

local pcoptions = require("LUI.PCOptions")
local original_feeder = pcoptions.AudioOptionsFeeder

pcoptions.AudioOptionsFeeder = function(...)
	local items = original_feeder(...)
	if type(items) ~= "table" or not Engine.IsMultiplayer() or Engine.IsZombiesMode() then
		return items
	end

	table.insert(items, pcoptions.OptionFactory(
		"snd_medalSoundDisabled",
		"@LUA_MENU_MEDAL_SOUND",
		"@LUA_MENU_MEDAL_SOUND_DESC",
		{
			{
				text = "@LUA_MENU_ENABLED",
				value = false
			},
			{
				text = "@LUA_MENU_DISABLED",
				value = true
			}
		}
	))

	return items
end
