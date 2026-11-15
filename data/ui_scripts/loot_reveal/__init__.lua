if (game:issingleplayer()) then
	return
end

local function patch()
	local list = LUI.MPNewItemsList
	if not list or not list.PlayNextVideo or not list.Videos or not list.Videos.RevealPurple or not list.Videos.RevealGreen or
		list.LegendaryRevealPatched then
		return
	end

	list.LegendaryRevealPatched = true

	local purple = list.Videos.RevealPurple
	local legendary = {}
	for key, value in pairs(purple) do
		legendary[key] = value
	end
	legendary.sound = "ui_cac_reinforce_reveal_item_elite"
	legendary.voice_over = "dds_gdn_react_elite"

	local play_next_video = list.PlayNextVideo
	list.PlayNextVideo = function(element, ...)
		local sequence = element and element.RevealSequence
		local entry = sequence and sequence[1]
		if entry and entry.lootData then
			local rarity = entry.lootData.lootRarity
			if entry.info == purple then
				entry.info = legendary
			elseif not entry.info and type(rarity) == "number" and rarity >= 6 then
				entry.info = list.Videos.RevealGreen
			end
		end

		return play_next_video(element, ...)
	end
end

patch()
