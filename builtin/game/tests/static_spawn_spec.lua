local function load_static_spawn(wait_for_map)
	local callbacks = {}
	local emerge_callback
	local player

	_G.core = {
		settings = {
			get = function(_, name)
				if name == "static_spawnpoint" then
					return "(10,20,30)"
				end
			end,
			get_bool = function(_, name, default)
				if name == "static_spawnpoint_wait_for_map" then
					return wait_for_map
				end
				return default
			end,
			get_pos = function(_, name)
				if name == "static_spawnpoint" then
					return {x = 10, y = 20, z = 30}
				end
			end,
		},
		setting_get_pos = function(name)
			if name == "static_spawnpoint" then
				return {x = 10, y = 20, z = 30}
			end
		end,
		register_on_newplayer = function(callback)
			callbacks.newplayer = callback
		end,
		register_on_respawnplayer = function(callback)
			callbacks.respawnplayer = callback
		end,
		emerge_area = function(pos1, pos2, callback)
			assert.same({x = 10, y = 20, z = 30}, pos1)
			assert.same({x = 10, y = 20, z = 30}, pos2)
			emerge_callback = callback
		end,
		get_player_by_name = function(name)
			return name == "alice" and player or nil
		end,
		log = function() end,
		pos_to_string = function() return "(10,20,30)" end,
	}

	player = {
		get_player_name = function() return "alice" end,
		set_pos = function(_, pos)
			player.position = pos
		end,
	}

	assert(loadfile("builtin/game/static_spawn.lua"))()
	return callbacks, function() return emerge_callback end, player
end

describe("static spawn point", function()
	it("waits for the spawn mapblock before positioning when enabled", function()
		local callbacks, get_emerge_callback, player = load_static_spawn(true)

		assert.is_true(callbacks.newplayer(player))
		assert.is_nil(player.position)
		assert.is_function(get_emerge_callback())

		get_emerge_callback()({x = 0, y = 1, z = 1}, core.EMERGE_FROM_MEMORY, 0)
		assert.same({x = 10, y = 20, z = 30}, player.position)
	end)
end)
