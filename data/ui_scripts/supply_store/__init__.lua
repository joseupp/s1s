if (game:issingleplayer()) then
	return
end

S1SStore = {
	open = false,
	menu = nil
}

local category_id_base = 9000
local sku_base = 90000
local supply_types = {
	{
		key = "s1s_supply_drop_mtx", type = 4, name = "@LUA_MENU_SUPPLY_DROP_MTX", image = "s1_supply_drop_mtx",
		prices = { { 1, 300 }, { 3, 850 }, { 5, 1350 }, { 10, 2500 }, { 20, 4800 } }
	}
}

local showcase_items = {
	0x1000074, 0x1000093, 0x10000B0, 0x1000340, 0x100034E, 0x1000354, 0x100035D, 0x1000363, 0x1000368,
	0x1000370, 0x1000376, 0x1000381, 0x1000386, 0x100038A, 0x1000391, 0x10003DF, 0x620000A, 0x620C00A,
	0x6202004, 0x6202006, 0x6204006, 0x6204007, 0x6202008, 0x620A008, 0x640C001, 0x640200C, 0x640A00C,
	0x6404003, 0x6406009, 0x690000C, 0x690A00E, 0x6900014, 0x6A0000D, 0x6A0E002, 0x6A02003, 0x6A08004,
	0x6A02007, 0x6A0A008, 0x6A02009, 0x6802001, 0x680200C, 0x680200F, 0x6804006, 0x680C006, 0x670200D,
	0x670A00D, 0x6702011, 0x6702003, 0x6700004, 0x6604001, 0x660A001, 0x6600002, 0x6602003, 0x6602004,
	0x6600006, 0x6B16001, 0x6B2E001, 0x6B08002, 0x6B08003, 0x6B08006, 0x6B00008, 0x620A00D, 0x6800010,
	0x6900016, 0x640A010, 0x690001A, 0x6800015, 0x660E021, 0x6B0400B, 0x6B0600B, 0x6A0400F, 0x690601B,
	0x640C012, 0x640E012, 0x6202010, 0x6204010, 0x6600031, 0x6602033, 0x6700017, 0x6702018, 0x6B02017,
	0x670201B, 0x670401B, 0x660003C, 0x6400015, 0x670401C, 0x660003F, 0x6A06013, 0x6600040, 0x6700022,
	0x6408018, 0x620A014, 0x6B0201A, 0x6600042, 0x6400019, 0x6B0001C, 0x6600046, 0x640001D, 0x6700028,
	0x6B00022
}

game:addlocalizedstring("S1S_STORE_SUPPLY_DROP_MTX_DESC",
	"Cada suministro avanzado contiene 3 objetos de botín y el primero es como mínimo raro.\n\n" ..
	"Más probabilidad de objetos élite que en los suministros normales. Los objetos repetidos se cambian por CoD Points.")

local function shuffled_showcase()
	local items = {}
	for _, guid in ipairs(showcase_items) do
		local ok, data = pcall(LUI.InventoryUtils.GetLootData, guid)
		if ok and data then
			table.insert(items, math.random(#items + 1), { guid = guid, quantity = 1 })
		end
	end
	return items
end

local function get_codpoints()
	return Engine.GetDvarInt("loot_codPoints") or 0
end

local categories = {}
local products = {}
local products_by_sku = {}

for index, supply in ipairs(supply_types) do
	local category_id = category_id_base + index
	table.insert(categories, {
		key = supply.key,
		title = supply.name,
		id = category_id,
		visibility = 0
	})

	for _, price in ipairs(supply.prices) do
		local quantity, cost = price[1], price[2]
		local sku = sku_base + index * 100 + quantity
		local name = "S1S_STORE_" .. string.upper(supply.key) .. "_" .. quantity

		game:addlocalizedstring(name, Engine.Localize(supply.name) .. (quantity > 1 and (" x" .. quantity) or ""))

		local product = {
			key = supply.key .. "_" .. quantity,
			skuId = sku,
			title = "@" .. name,
			description = "@S1S_STORE_SUPPLY_DROP_MTX_DESC",
			image = supply.image,
			categoryId = category_id,
			visibility = 0,
			displayAsFirstGuid = false,
			supplyType = supply.type,
			quantity = quantity,
			cost = cost
		}
		table.insert(products, product)
		products_by_sku[sku] = product
	end
end

S1SStore.categories = categories
S1SStore.products = products
S1SStore.products_by_sku = products_by_sku

function S1SStore.send_inventory_event(controller, task_type)
	if not S1SStore.menu then
		return
	end

	local timer = LUI.UITimer.new(50, {
		name = "inventory",
		inventoryEventType = LUI.InventoryEventType.TaskCompleted,
		inventoryTaskType = task_type,
		success = true,
		controller = controller
	}, nil, true)
	S1SStore.menu:addElement(timer)
end

S1SStoreOriginals = S1SStoreOriginals or {}

local function wrap(table_, name, func)
	local key = tostring(table_) .. "." .. name
	if S1SStoreOriginals[key] == nil then
		S1SStoreOriginals[key] = table_[name] or false
	end

	local original = S1SStoreOriginals[key] or nil
	table_[name] = function(...)
		return func(original, ...)
	end
end

local function patch_store()
	if CoDPoints then
		wrap(CoDPoints, "Enabled", function(original, ...)
			if S1SStore.open then
				return true
			end
			return original and original(...)
		end)
	end

	if not CoDPointStore then
		CoDPointStore = {}
	end

	do
		wrap(CoDPointStore, "HasConfig", function(original, ...)
			if S1SStore.open then
				return true
			end
			return original and original(...)
		end)
		wrap(CoDPointStore, "RequestConfig", function(original, ...)
			if not S1SStore.open and original then
				return original(...)
			end
		end)
		wrap(CoDPointStore, "GetCategoryList", function(original, ...)
			if S1SStore.open then
				return S1SStore.categories
			end
			return original and original(...) or {}
		end)
		wrap(CoDPointStore, "GetProductList", function(original, ...)
			if S1SStore.open then
				return S1SStore.products
			end
			return original and original(...) or {}
		end)
		wrap(CoDPointStore, "GetIGSProductList", function(original, ...)
			if S1SStore.open then
				return {}
			end
			return original and original(...) or {}
		end)
	end

	do
		wrap(Engine, "Inventory_FetchAllSKUs", function(original, controller, category_id, ...)
			if S1SStore.open then
				S1SStore.send_inventory_event(controller, LUI.InventoryTaskType.GetSkus)
				return true
			end
			return original(controller, category_id, ...)
		end)

		wrap(Engine, "Inventory_GetAllSKUIDs", function(original, category_id, ...)
			if S1SStore.open then
				local skus = {}
				for _, product in ipairs(S1SStore.products) do
					if product.categoryId == category_id then
						table.insert(skus, product.skuId)
					end
				end
				return skus
			end
			return original(category_id, ...)
		end)

		wrap(Engine, "Inventory_GetSKUInfo", function(original, sku, ...)
			local product = S1SStore.open and S1SStore.products_by_sku[sku]
			if product then
				return {
					soldOut = false,
					prices = {
						{
							currency = LUI.InventoryCurrency.CoDPoints,
							value = product.cost
						}
					},
					items = shuffled_showcase(),
					maxQuantity = "0xffffffff"
				}
			end
			return original(sku, ...)
		end)

		wrap(Engine, "Inventory_PurchaseSKU", function(original, controller, sku, count, ...)
			local product = S1SStore.open and S1SStore.products_by_sku[sku]
			if product then
				local units = count or 1
				Engine.Exec("buysupplydrop " .. product.supplyType .. " " .. product.quantity * units .. " " .. product.cost * units)
				S1SStore.send_inventory_event(controller, LUI.InventoryTaskType.PurchaseSkus)
				return 1
			end
			return original(controller, sku, count, ...)
		end)

		wrap(Engine, "Inventory_GetCoDPointsBalance", function(original, ...)
			if S1SStore.open then
				return get_codpoints()
			end
			return original(...)
		end)

	end
end

local function patch_store_menu()
	if not LUI.MenuBuilder.m_types_build["CoDPointStore"] or not LUI.CoDPointStore then
		return false
	end

	do
		wrap(LUI.CoDPointStore, "ProductChildBuilder", function(original_child_builder, list, ...)
			local def = original_child_builder(list, ...)
			local font = list and list.menuProps and list.menuProps.product_title_font or CoD.TextSettings.TitleFontSmall
			if def and def.children and font then
				table.insert(def.children, {
					type = "UIText",
					id = (def.id or "cp_product") .. "_s1s_title",
					states = {
						default = {
							leftAnchor = true,
							topAnchor = false,
							rightAnchor = true,
							bottomAnchor = true,
							left = 0,
							right = 0,
							bottom = -4,
							height = font.Height,
							font = font.Font,
							alignment = LUI.Alignment.Center
						}
					},
					handlers = {
						update_product = function(element, event)
							element:setText(event.title or "")
						end
					}
				})
			end
			return def
		end)

		wrap(LUI.MenuBuilder.m_types_build, "CoDPointStore", function(original_build, ...)
			local menu = original_build(...)
			S1SStore.menu = menu
			return menu
		end)

		LUI.FlowManager.RegisterStackPopBehaviour("CoDPointStore", function()
			CoDPoints.DisposeCache()
			LUI.FlowManager.RequestLeaveMenuByName("CoDPointStoreBusySpinner")
			S1SStore.open = false
			S1SStore.menu = nil
		end)

	end

	return true
end

local function open_store(controller)
	if not patch_store_menu() then
		return false
	end

	patch_store()
	S1SStore.open = true
	LUI.FlowManager.RequestAddMenu(nil, "CoDPointStore", true, controller, false, {
		exclusiveController = controller
	})
	return true
end

wrap(Engine, "UserCanAccessStore", function(original, ...)
	return true
end)

if Sns then
	wrap(Sns, "OpenStoreMenu", function(original, controller, ...)
		if Engine.IsMultiplayer() and not Engine.IsZombiesMode() and open_store(controller) then
			Engine.SetDvarBool("ui_onlineRequired", false)
			return true
		end
		return original(controller, ...)
	end)
end
