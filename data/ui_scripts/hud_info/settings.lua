local pcoptions = require("LUI.PCOptions")

game:addlocalizedstring("LUA_MENU_FPS", "Contador de FPS")
game:addlocalizedstring("LUA_MENU_FPS_DESC", "Mostrar el contador de FPS")

game:addlocalizedstring("LUA_MENU_LATENCY", "Latencia")
game:addlocalizedstring("LUA_MENU_LATENCY_DESC", "Muestra la latencia del servidor")

game:addlocalizedstring("LUA_MENU_FOV_ADS_AFFECTED", "Zoom afectado por el FOV")
game:addlocalizedstring("LUA_MENU_FOV_ADS_AFFECTED_DESC", "Con FOV alto, el zoom al apuntar se parece al de FOV 65")

pcall(function()
	if SliderBounds and SliderBounds.FOV then
		SliderBounds.FOV.Max = 120
	end
end)

fov_toggle_in_advanced_video = false
pcall(function()
	local advanced_video = require("LUI.AdvancedVideo")
	if not advanced_video or not advanced_video.AdvancedVideoOptionsFeeder or not advanced_video.VideoOptionFactory then
		return
	end

	local original_feeder = advanced_video.AdvancedVideoOptionsFeeder
	advanced_video.AdvancedVideoOptionsFeeder = function(...)
		local items = original_feeder(...)
		if not Engine.IsMultiplayer() or type(items) ~= "table" then
			return items
		end

		for index, item in ipairs(items) do
			if item.id == "option_@PLATFORM_FOV" then
				table.insert(items, index + 1, advanced_video.VideoOptionFactory({
					title = "@LUA_MENU_FOV_ADS_AFFECTED",
					desc = "@LUA_MENU_FOV_ADS_AFFECTED_DESC",
					dvar = "cg_fovAdsAffected",
					style = "toggle",
					optionList = {
						{
							text = "@MENU_ON",
							value = true
						},
						{
							text = "@MENU_OFF",
							value = false
						}
					},
					updateValueFunc = function()
					end
				}))
				break
			end
		end

		return items
	end

	fov_toggle_in_advanced_video = true
end)

pcoptions.VideoOptionsFeeder = function()
	local items = {
		pcoptions.OptionFactory(
			"ui_r_displayMode",
			"@LUA_MENU_DISPLAY_MODE",
			nil,
			{
				{
					text = "@LUA_MENU_MODE_FULLSCREEN",
					value = "fullscreen"
				},
				{
					text = "@LUA_MENU_MODE_WINDOWED_NO_BORDER",
					value = "windowed_no_border"
				},
				{
					text = "@LUA_MENU_MODE_WINDOWED",
					value = "windowed"
				}
			},
			nil,
			true
		),
		pcoptions.SliderOptionFactory(
			"profileMenuOption_blacklevel",
			"@MENU_BRIGHTNESS",
			"@MENU_BRIGHTNESS_DESC1",
			SliderBounds.PCBrightness.Min,
			SliderBounds.PCBrightness.Max,
			SliderBounds.PCBrightness.Step,
			function(element)
				element:processEvent({
					name = "brightness_over",
					immediate = true
				})
			end,
			function(element)
				element:processEvent({
					name = "brightness_up",
					immediate = true
				})
			end,
			true,
			nil,
			"brightness_updated"
		),
		pcoptions.OptionFactoryProfileData(
			"renderColorBlind",
			"profile_toggleRenderColorBlind",
			"@LUA_MENU_COLORBLIND_FILTER",
			"@LUA_MENU_COLOR_BLIND_DESC",
			{
				{
					text = "@LUA_MENU_ENABLED",
					value = true
				},
				{
					text = "@LUA_MENU_DISABLED",
					value = false
				}
			},
			nil,
			false
		)
	}

	if Engine.IsMultiplayer() and not Engine.IsZombiesMode() then
		table.insert(items, pcoptions.OptionFactory(
			"cg_paintballFx",
			"@LUA_MENU_PAINTBALL",
			"@LUA_MENU_PAINTBALL_DESC",
			{
				{
					text = "@LUA_MENU_ENABLED",
					value = true
				},
				{
					text = "@LUA_MENU_DISABLED",
					value = false
				}
			},
			nil,
			false,
			false
		))
	end

	if Engine.IsMultiplayer() and not fov_toggle_in_advanced_video then
		table.insert(items, pcoptions.OptionFactory(
			"cg_fovAdsAffected",
			"@LUA_MENU_FOV_ADS_AFFECTED",
			"@LUA_MENU_FOV_ADS_AFFECTED_DESC",
			{
				{
					text = "@LUA_MENU_ENABLED",
					value = true
				},
				{
					text = "@LUA_MENU_DISABLED",
					value = false
				}
			},
			nil,
			false,
			false
		))
	end

	table.insert(items, {
		type = "UIGenericButton",
		id = "option_advanced_video",
		properties = {
			style = GenericButtonSettings.Styles.GlassButton,
			button_text = Engine.Localize("@LUA_MENU_ADVANCED_VIDEO"),
			desc_text = "",
			button_action_func = pcoptions.ButtonMenuAction,
			text_align_without_content = LUI.Alignment.Left,
			menu = "advanced_video"
		}
	})

	return items
end
