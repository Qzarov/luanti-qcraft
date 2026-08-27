local static_spawnpoint_string = core.settings:get("static_spawnpoint")
if static_spawnpoint_string and
		static_spawnpoint_string ~= "" and
		not core.settings:get_pos("static_spawnpoint") then
	error('The static_spawnpoint setting is invalid: "' ..
			static_spawnpoint_string .. '"')
end

local wait_for_spawn_map = core.settings:get_bool("static_spawnpoint_wait_for_map", false)

local function move_player_to_spawn(player_obj, static_spawnpoint)
	core.log("action", "Moving " .. player_obj:get_player_name() ..
		" to static spawnpoint at " .. core.pos_to_string(static_spawnpoint))
	player_obj:set_pos(static_spawnpoint)
	return true
end

local function put_player_in_spawn(player_obj)
	local static_spawnpoint = core.setting_get_pos("static_spawnpoint")
	if not static_spawnpoint then
		return false
	end

	if wait_for_spawn_map then
		local player_name = player_obj:get_player_name()
		core.emerge_area(static_spawnpoint, static_spawnpoint,
				function(_, _, calls_remaining)
					if calls_remaining ~= 0 then
						return
					end
					local player = core.get_player_by_name(player_name)
					if player then
						move_player_to_spawn(player, static_spawnpoint)
					end
				end)
		return true
	end

	return move_player_to_spawn(player_obj, static_spawnpoint)
end

core.register_on_newplayer(put_player_in_spawn)
core.register_on_respawnplayer(put_player_in_spawn)
