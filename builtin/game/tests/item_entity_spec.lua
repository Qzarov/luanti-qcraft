local function load_item_entity(auto_pickup_radius)
	local entity_definition
	local callback_picker
	local player = {is_player = function() return true end}

	_G.core = {
		settings = {
			get = function(_, name)
				if name == "item_auto_pickup_radius" then
					return auto_pickup_radius
				end
			end,
		},
		registered_items = {
			["test:item"] = {
				on_pickup = function(_, picker)
					callback_picker = picker
					return ItemStack()
				end,
			},
		},
		register_entity = function(_, definition)
			entity_definition = definition
		end,
		get_node_or_nil = function() return {name = "air"} end,
		get_node = function() return {name = "air"} end,
		registered_nodes = {air = {walkable = false}},
		get_objects_inside_radius = function(_, radius)
			assert.equal(2, radius)
			return {player}
		end,
	}

	_G.ItemStack = function(value)
		if type(value) == "table" then
			return value
		end
		local is_empty = value == nil or value == ""
		return {
			is_empty = function() return is_empty end,
			get_definition = function() return core.registered_items["test:item"] end,
		}
	end

	assert(loadfile("builtin/game/item_entity.lua"))()
	return entity_definition, player, function() return callback_picker end
end

describe("item entity automatic pickup", function()
	it("uses the regular pickup callback for a nearby player while stationary", function()
		local definition, player, get_callback_picker = load_item_entity("2")
		local removed = false
		local entity = {
			itemstring = "test:item",
			age = 0,
			moving_state = false,
			physical_state = true,
			_collisionbox = {-0.3, -0.3, -0.3, 0.3, 0.3, 0.3},
			object = {
				get_pos = function() return {x = 0, y = 0, z = 0} end,
				get_attach = function() return nil end,
				remove = function() removed = true end,
			},
		}
		entity.on_punch = definition.on_punch

		definition.on_step(entity, 0.1, {collides = false})

		assert.equal(player, get_callback_picker())
		assert.is_true(removed)
	end)
end)
