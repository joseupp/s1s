do return end

if (game:issingleplayer()) then
	return
end

local aarmain = LUI.MenuBuilder.m_definitions["aar_summary_main"]
if not aarmain then
	return
end

local function new_items_count(controller)
	local items = Engine.Inventory_GetNewItems(controller) or {}
	local packages = Engine.Loot_GetUnopenedSupplyPackages(controller) or {}
	return #items + #packages
end

LUI.MenuBuilder.m_definitions["aar_summary_main"] = function(...)
	local def = aarmain(...)
	if not def or not def.handlers then
		return def
	end

	local menu_create = def.handlers.menu_create
	def.handlers.menu_create = function(menu, event)
		if menu_create then
			menu_create(menu, event)
		end

		local controller = Engine.GetFirstActiveController()
		if new_items_count(controller) > 0 then
			return
		end

		local elapsed = 0
		local timer = LUI.UITimer.new(250, "aar_new_items_check")
		timer.id = "aar_new_items_check_timer"
		menu:addElement(timer)

		menu:registerEventHandler("aar_new_items_check", function(element)
			elapsed = elapsed + 250
			if new_items_count(controller) > 0 then
				timer:close()
				element:processEvent({
					name = "menu_refresh"
				})
			elseif elapsed >= 15000 then
				timer:close()
			end
		end)
	end

	return def
end
