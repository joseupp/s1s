if (game:issingleplayer()) then
	return
end

LUI.MenuBuilder.m_types_build["host_migration_main"] = function()
	local element = LUI.UIElement.new()
	element.id = "host_migration_id"
	element:registerEventHandler("hide_host_migration", function(menu)
		LUI.FlowManager.RequestLeaveMenu(menu)
	end)
	return element
end

if Engine.ExecFirstClient and not Engine.ExecFirstClientOriginal then
	Engine.ExecFirstClientOriginal = Engine.ExecFirstClient
	Engine.ExecFirstClient = function(command, ...)
		if command == "hostmigration_start" or command == "hostmigration_start_backout" then
			local exec = Engine.ExecFirstClientOriginal
			exec("disconnect")
			exec("xblive_privatematch 0")
			exec("onlinegame 1")
			return exec("xstartprivateparty")
		end
		return Engine.ExecFirstClientOriginal(command, ...)
	end
end

function GetGameModeName()
	return Engine.Localize(Engine.TableLookup(GameTypesTable.File, GameTypesTable.Cols.Ref, GameX.GetGameMode(), GameTypesTable.Cols.Name))
end

-- Allow players to change teams in game.
function CanChangeTeam()
	local f9_local0 = GameX.GetGameMode()
	local f9_local1
	if f9_local0 ~= "aliens" and Engine.TableLookup( GameTypesTable.File, GameTypesTable.Cols.Ref, f9_local0, GameTypesTable.Cols.TeamChoice ) == "1" then
		f9_local1 = not MLG.IsMLGSpectator()
	else
		f9_local1 = false
	end
	return f9_local1
end
