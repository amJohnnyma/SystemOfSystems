extends Node2D

@onready var world: GodotSimulation = $GodotSimulation
@onready var input_manager: InputManager = $InputManager
@onready var ui_root_scene = preload("res://demo/ui_root.tscn")



# Chunk coordinate -> { "terrain": {atlas_path: MeshInstance2D},
#                       "structures": {structure_id: MeshInstance2D} }
var chunk_nodes: Dictionary = {}
var camera: Camera2D
#var shader_mat: ShaderMaterial

# Mesh configuration
const MESH_SCALE := Vector2(4.0, 4.0)
var cell_size: float = 64.0


var world_width: int = 640
var world_height: int = 180
var threshold: float = 128.0



var structure_registry: Dictionary = {}
var entity_registry: Dictionary = {}
var item_registry: Dictionary = {}
var selected_structure: Dictionary = {}
var textured_shader: Shader
var building_materials = {}

var selected_hotbar_slot = -1

# One shared source of truth for item icons and world block rendering:
# res://game/structure_atlases.json
const BlockAtlasCatalog = preload("res://demo/bin/block_atlas_catalog.gd")
var block_atlases = BlockAtlasCatalog.new()
var terrain_blend_shader: Shader

# Avoid logging the same unmapped type hundreds of times across chunks.
var _warned_unmapped_types: Dictionary = {}

const SHOW_ENTITY_HITBOXES := true
var enemy_multimeshes : Dictionary = {}
var enemy_materials : Dictionary = {}
var debug_multimesh_node: MultiMeshInstance2D
var enemy_shader: Shader

var PLAYER_ANIMATIONS := {}

var player_multimeshes: Dictionary = {}
var player_materials: Dictionary = {}
var player_debug_multimesh_node: MultiMeshInstance2D


var player_visual_pixels_per_cell: float = 20.0 # = (Character height in pixels) / (Desired height in world cells)
var player_visual_pivot: Vector2 = Vector2(0.5, 0.0)





func _ready() -> void:
	# Create and configure Camera2D
	camera = Camera2D.new()
	add_child(camera)
	camera.make_current()

	# Cache shared shader material
	#shader_mat = ShaderMaterial.new()
	#shader_mat.shader = preload("res://shaders/flat_colour.gdshader")
	textured_shader = Shader.new()
	textured_shader.code = """
		shader_type canvas_item;
		uniform sampler2D building_tex : source_color, filter_nearest;

	void fragment() {
		COLOR = texture(building_tex, UV);
		//COLOR = vec4(UV.x, UV.y, 0.0, 1.0);
	}
		"""
		
	terrain_blend_shader = Shader.new()
	terrain_blend_shader.code = """
	shader_type canvas_item;
	render_mode unshaded;

	uniform sampler2D terrain_atlas : source_color, filter_nearest;

	void fragment() {
	    COLOR = texture(terrain_atlas, UV);
	}

		"""

		# One ShaderMaterial per atlas is created and cached by the catalog.
	if not block_atlases.load_config(terrain_blend_shader, textured_shader):
		push_error("Block atlases failed to load; world blocks may be invisible")

	
	# Connect to InputManager signals
	input_manager.setup(world, camera, MESH_SCALE)
	input_manager.terrain_modified.connect(update_mesh_display)
	input_manager.cursor_moved.connect(func(_pos): queue_redraw())
	input_manager.brush_radius_changed.connect(func(_radius): queue_redraw())
	input_manager.building_placed.connect(_place_selected_building)
	input_manager.building_cancelled.connect(_cancel_building)
	input_manager.player_data_updated.connect(_on_player_data_updated)

	# Instantiate UI CanvasLayer above the world
	var ui_root = ui_root_scene.instantiate()
	add_child(ui_root)
	UIManager.register_ui_root(ui_root)
	UIManager.connect_simulation(world)
	UIManager.slot_action_requested.connect(_on_slot_action_requested)
	
	var hotbar = get_tree().root.find_child("Hotbar", true, false)
	if hotbar:
		hotbar.hotbar_slot_selected.connect(_on_hotbar_slot_selected)

	# Connect using the standard GDScript signal syntax
	world.test_signal_event.connect(_on_custom_event)
	world.update_godot_mesh_event.connect(_on_required_mesh_update)
	world.update_entities_event.connect(_on_required_entity_update)
	
	# Call the C++ method that triggers the emission
	world.trigger_test_signal("MEOW")
	
	var defaults: Dictionary = world.getWorldDefaults()

	world_width = int(defaults["width"])
	world_height = int(defaults["height"])
	cell_size = float(defaults["chunk_size"])
	threshold = float(defaults["density_threshold"])
		# Initialize C++ world grid (world_x, world_y, g_size)
	world.createWorld(world_width, world_height, int(cell_size), threshold)
		
	debug_multimesh_node = MultiMeshInstance2D.new()
	debug_multimesh_node.name = "DebugOriginMultiMesh"
	debug_multimesh_node.scale = MESH_SCALE
	add_child(debug_multimesh_node)     
	var mmd = MultiMesh.new()
	mmd.transform_format = MultiMesh.TRANSFORM_2D
	mmd.use_custom_data = true # Enables INSTANCE_CUSTOM color
	mmd.mesh = QuadMesh.new()     
	mmd.mesh.size = Vector2(1, 1)      
	debug_multimesh_node.multimesh = mmd     
	debug_multimesh_node.z_index = 10      

	var debug_mat := ShaderMaterial.new()
	var debug_shad := Shader.new()
	debug_shad.code = """
        shader_type canvas_item;
        render_mode unshaded;

        varying vec4 inst_color;

        void vertex() {
            // INSTANCE_CUSTOM holds the Color passed from set_instance_custom_data
            inst_color = INSTANCE_CUSTOM;
        }

        void fragment() {
            COLOR = inst_color;
        }
	"""
	debug_mat.shader = debug_shad
	debug_multimesh_node.material = debug_mat
	
	_init_entity_materials()
	_init_building_materials()
	_init_create_player()
	_init_player_materials()
	_init_player_debug()
	_register_entity_render_configs()
	_print_item_registry_preview()
	
	_on_player_data_updated(world.getPlayerData())
	
	
	#print("Loaded structures into Godot: ", structure_registry)
	#print("Loaded entities into Godot:\n", JSON.stringify(entity_registry, "\t"))
	
	
	# Temporary visual test: place every registered block on a 16-column grid.
	var test_block_types: Array = block_atlases.get_block_types()
	for i in range(test_block_types.size()):
		var block_type: int = int(test_block_types[i])
		if not structure_registry.has(block_type):
			continue
		world.placeStructure(i % 16, int(i / 16), block_type)
		
		
	# Populate all chunk nodes initially
	_setup_full_mesh_display()
	update_mesh_display()
	
	
	camera.global_position = Vector2(world_width * MESH_SCALE.x, world_height * MESH_SCALE.y) / 2.0
	
	#world.placeEntity(310,10,1)
	#world.placeEntity(320,10,2)
	#world.placeEntity(330,10,3)
	#world.placeEntity(340,10,4)
	for i in range(10):
		world.placeEntity(randi_range(1, 320),10,1)

	
func _calculate_sprite_transform(
	grid_position: Vector2,
	body_size: Vector2,
	frame_size_px: Vector2,
	pixels_per_cell: float,
	pivot: Vector2,
	facing_right: bool
) -> Transform2D:

	# Convert source pixels into world cells
	var render_size: Vector2 = frame_size_px / maxf(pixels_per_cell, 0.001)

	var direction: float = 1.0 if facing_right else -1.0

	# Bottom-center of the physical body
	var anchor := Vector2(
		grid_position.x + body_size.x * 0.5,
		grid_position.y + 1.0
	)

	# Align the visual pivot to the physical anchor
	var center := Vector2(
		anchor.x + direction * (0.5 - pivot.x) * render_size.x,
		anchor.y + (pivot.y - 0.5) * render_size.y
	)

	return Transform2D(
		Vector2(render_size.x * direction, 0.0),
		Vector2(0.0, -render_size.y),
		center
	)

func _print_item_registry_preview():
	item_registry = world.getItemRegistry()
	'''
	var custom_ids : Array[int] = []
	var structure_ids : Array[int] = []
	for id in item_registry.keys():
		var item: Dictionary = item_registry[id]
		if bool(item.get("generated_from_structure", false)):
			structure_ids.append(int(id))
		else:
			custom_ids.append(int(id))
	custom_ids.sort()
	structure_ids.sort()
	print("Item registry: ", item_registry.size(), " definitions (", custom_ids.size(), " custom, ", structure_ids.size(), " structures)")
	for id in custom_ids.slice(0, 5):
		print("[Custom item ", id, "] ", item_registry[id])
	for id in structure_ids.slice(0, 5):
		print("[Structure item ", id, "] ", item_registry[id])
		'''

func _init_create_player() -> void:
	const PLAYER_JSON_PATH = "res://game/player.json"

	if not FileAccess.file_exists(PLAYER_JSON_PATH):
		push_error("Player JSON not found: " + PLAYER_JSON_PATH)
		return

	var file := FileAccess.open(PLAYER_JSON_PATH, FileAccess.READ)

	if file == null:
		push_error("Failed to open player JSON: " + PLAYER_JSON_PATH)
		return

	var json := JSON.new()
	var error := json.parse(file.get_as_text())
	file.close()

	if error != OK:
		push_error(
			"Failed to parse player JSON at line %d: %s"
			% [json.get_error_line(), json.get_error_message()]
		)
		return

	if not json.data is Dictionary:
		push_error("Player JSON must contain a Dictionary.")
		return

	var data: Dictionary = json.data

	# Store the original animation settings for Godot rendering.
	PLAYER_ANIMATIONS = data.get("sprites", {}).duplicate(true)

	# Visual scaling, independent of physical size.
	player_visual_pixels_per_cell = float(
		data.get("visualPixelsPerCell", 30.0)
	)

	# Normalized pivot, measured from bottom-left.
	player_visual_pivot = Vector2(0.5, 0.0)

	var pivot_data: Array = data.get("visualPivot", [0.5, 0.0])
	if pivot_data.size() >= 2:
		player_visual_pivot = Vector2(
			float(pivot_data[0]),
			float(pivot_data[1])
		)
	
	

	# Send the ORIGINAL dictionary to C++.
	world.createPlayer(data)

func _on_player_data_updated(data: Dictionary) -> void:
	if data.is_empty() or player_multimeshes.is_empty():
		return

	# 1. Determine animation state
	var state: String = str(data.get("state", "idle"))

	if not player_multimeshes.has(state):
		state = "idle" if player_multimeshes.has("idle") else str(player_multimeshes.keys()[0])

	# Hide inactive animation states
	for anim_state in player_multimeshes:
		if anim_state != state:
			var inactive_mm: MultiMesh = player_multimeshes[anim_state].multimesh
			if inactive_mm.instance_count != 0:
				inactive_mm.instance_count = 0

	var mm_node: MultiMeshInstance2D = player_multimeshes[state]
	var mm: MultiMesh = mm_node.multimesh

	if mm.instance_count != 1:
		mm.instance_count = 1

	# 2. Animation frame selection
	var total_cols: int = max(1, int(mm_node.get_meta("cols", 1)))
	var total_rows: int = max(1, int(mm_node.get_meta("rows", 1)))
	var total_frames: int = total_cols * total_rows

	var anim_phase: float = float(data.get("animPhase", 0.0))

	var current_frame: int = int(
		floor(anim_phase * total_frames)
	) % total_frames

	var frame_col: float = float(current_frame % total_cols)
	var frame_row: float = float(current_frame / total_cols)

	# 3. Calculate visual size from source pixels
	var frame_width_px: float = float(
		mm_node.get_meta("frame_width_px", 0.0)
	)

	var frame_height_px: float = float(
		mm_node.get_meta("frame_height_px", 0.0)
	)

	if frame_width_px <= 0.0 or frame_height_px <= 0.0:
		push_warning("Player frame dimensions not initialized: " + state)
		return

	var pixels_per_cell: float = max(
		0.001,
		player_visual_pixels_per_cell
	)

	var render_w: float = frame_width_px / pixels_per_cell
	var render_h: float = frame_height_px / pixels_per_cell

	# 4. Physical position from C++
	var grid_x: float = float(data.get("x_smooth", 0.0))
	var grid_y: float = float(data.get("y_smooth", 0.0))
	#camera.global_position = Vector2(grid_x * MESH_SCALE.x, grid_y * MESH_SCALE.y)

	var body_w: float = float(data.get("width", 2.0))
	var body_h: float = float(data.get("height", 3.0))

	var facing_right: bool = bool(data.get("facingRight", true))
	var direction: float = 1.0 if facing_right else -1.0

	# Bottom-center anchor of the physical body
	var anchor := Vector2(
		grid_x + body_w * 0.5,
		grid_y + 1.0
	)

	# 5. Position sprite relative to its visual pivot
	var pivot: Vector2 = player_visual_pivot

	var center := Vector2(
		anchor.x + direction * (0.5 - pivot.x) * render_w,
		anchor.y + (pivot.y - 0.5) * render_h
	)

	var xform := Transform2D(
		Vector2(render_w * direction, 0.0),
		Vector2(0.0, -render_h),
		center
	)

	mm.set_instance_transform_2d(0, xform)

	# 6. Pass animation frame to shader
	mm.set_instance_custom_data(
		0,
		Color(
			frame_col,
			frame_row,
			float(total_cols),
			float(total_rows)
		)
	)

	# 7. Debug physical collision box
	_update_player_debug_hitbox(data)

func _update_player_debug_hitbox(data: Dictionary) -> void:
	if not player_debug_multimesh_node:
		return

	var mm: MultiMesh = player_debug_multimesh_node.multimesh

	var grid_x: float = float(data.get("x_smooth", 0.0))
	var grid_y: float = float(data.get("y_smooth", 0.0))

	var w: float = float(data.get("width", 2.0))
	var h: float = float(data.get("height", 3.0))

	# Same origin convention as the entity renderer
	var center := Vector2(
		grid_x + w * 0.5,
		grid_y - h * 0.5 + 1.0
	)

	# Full hitbox, independent of facing direction
	var xform := Transform2D(
		Vector2(w, 0.0),
		Vector2(0.0, -h),
		center
	)

	mm.set_instance_transform_2d(0, xform)

	# Semi-transparent green hitbox
	mm.set_instance_custom_data(
		0,
		Color(0.0, 1.0, 0.0, 0.3)
	)
	
func _init_player_debug() -> void:
	player_debug_multimesh_node = MultiMeshInstance2D.new()
	player_debug_multimesh_node.name = "PlayerHitboxDebug"
	player_debug_multimesh_node.scale = MESH_SCALE
	player_debug_multimesh_node.z_index = 20

	# Reuse the existing entity debug material
	player_debug_multimesh_node.material = debug_multimesh_node.material

	var mm := MultiMesh.new()
	mm.transform_format = MultiMesh.TRANSFORM_2D
	mm.use_custom_data = true

	var quad := QuadMesh.new()
	quad.size = Vector2(1, 1)

	mm.mesh = quad
	mm.instance_count = 1

	player_debug_multimesh_node.multimesh = mm

	add_child(player_debug_multimesh_node)
func _init_player_materials() -> void:
	for anim_state in PLAYER_ANIMATIONS:
		var anim_data: Dictionary = PLAYER_ANIMATIONS[anim_state]
		var sprite_name: String = anim_data.get("filename", "")

		if sprite_name.is_empty():
			continue

		var tex_path := "res://game/atlas/" + sprite_name

		if not ResourceLoader.exists(tex_path):
			push_warning("Missing player texture: " + tex_path)
			continue

		var tex: Texture2D = load(tex_path)

		# Reuse the entity shader
		var mat := ShaderMaterial.new()
		mat.shader = enemy_shader
		mat.set_shader_parameter("enemy_tex", tex)

		player_materials[anim_state] = mat
		
		var cols: int = max(1, int(anim_data.get("columns", 1)))
		var rows: int = max(1, int(anim_data.get("rows", 1)))

		var frame_width_px: float = float(tex.get_width()) / cols
		var frame_height_px: float = float(tex.get_height()) / rows

		# One MultiMeshInstance2D per animation state
		var mm_node := MultiMeshInstance2D.new()
		mm_node.name = "PlayerMultiMesh_%s" % anim_state
		mm_node.scale = MESH_SCALE
		mm_node.material = mat

		var mm := MultiMesh.new()
		mm.transform_format = MultiMesh.TRANSFORM_2D
		mm.use_custom_data = true
		mm.mesh = QuadMesh.new()
		mm.instance_count = 0

		mm_node.multimesh = mm

		mm_node.set_meta("cols", max(1, int(anim_data.get("columns", 1))))
		mm_node.set_meta("rows", max(1, int(anim_data.get("rows", 1))))
		#mm_node.set_meta("cols", cols)
		#mm_node.set_meta("rows", rows)

		mm_node.set_meta("frame_width_px", frame_width_px)
		mm_node.set_meta("frame_height_px", frame_height_px)

		add_child(mm_node)

		player_multimeshes[anim_state] = mm_node
		
		
func _init_entity_materials() -> void:
	# 1. Fetch base entity registry from C++
	entity_registry = world.getEntityRegistry()
	
	# 2. Load entity.json and merge/append animation/sprite data into entity_registry
	var file = FileAccess.open("res://game/entity.json", FileAccess.READ)
	if file:
		var json = JSON.new()
		var error = json.parse(file.get_as_text())
		if error == OK:
			var arr = json.get_data()
			for entry in arr:
				var type_id = int(entry.get("entityTypeID", 0))
				if entity_registry.has(type_id):
					var existing_entry: Dictionary = entity_registry[type_id]
					for key in entry:
						existing_entry[key] = entry[key]

	# 3. Enemy shader setup
	enemy_shader = Shader.new()
	enemy_shader.code = """
        shader_type canvas_item;

        uniform sampler2D enemy_tex : source_color, filter_nearest;

        void vertex() {
            // INSTANCE_CUSTOM passed from set_instance_custom_data:
            // r = col, g = row, b = total_columns, a = total_rows
            float col = INSTANCE_CUSTOM.r;
            float row = INSTANCE_CUSTOM.g;
            float total_columns = max(1.0, INSTANCE_CUSTOM.b);
            float total_rows = max(1.0, INSTANCE_CUSTOM.a);

            // Scale UVs down to fit a single grid frame
            UV /= vec2(total_columns, total_rows);

            // Offset UV to point to the correct frame tile
            UV += vec2(col / total_columns, row / total_rows);
        }

        void fragment() {
            COLOR = texture(enemy_tex, UV);
        }
		"""

	# 4. Initialize materials and MultiMeshInstance2D nodes per state for each enemy type
	for type_id_key in entity_registry:
		var type_id: int = int(type_id_key)
		var entity_data: Dictionary = entity_registry[type_id_key]
		
		var animations: Dictionary = entity_data.get("animations", {})
		
			# Visual rendering settings for this entity type
		var visual_pixels_per_cell: float = maxf(
			0.001,
			float(entity_data.get("visualPixelsPerCell", 30.0))
		)

		var visual_pivot := Vector2(0.5, 0.0)

		var pivot_data: Array = entity_data.get(
			"visualPivot", [0.5, 0.0]
		)

		if pivot_data.size() >= 2:
			visual_pivot = Vector2(
				float(pivot_data[0]),
				float(pivot_data[1])
			)
		

		
		# Loop through every defined animation state (idle, walk, attack, etc.)
		for anim_state in animations:
			var anim_data: Dictionary = animations[anim_state]
			var sprite_name: String = anim_data.get("sprite", "")
			
			if sprite_name.is_empty():
				continue

			var tex_path := "res://game/atlas/" + sprite_name
			if ResourceLoader.exists(tex_path):
				var tex: Texture2D = load(tex_path)
				
				# Create unique material for this specific entity type + state combo
				var mat := ShaderMaterial.new()
				mat.shader = enemy_shader
				mat.set_shader_parameter("enemy_tex", tex)
				
				# Use a composite key for materials: "typeID_state"
				var material_key := "%d_%s" % [type_id, anim_state]
				enemy_materials[material_key] = mat

				# Create dedicated MultiMeshInstance2D for this state
				var mm_node := MultiMeshInstance2D.new()
				mm_node.name = "EnemyMultiMesh_%d_%s" % [type_id, anim_state]
				mm_node.scale = MESH_SCALE
				mm_node.material = mat

				var mm := MultiMesh.new()
				mm.transform_format = MultiMesh.TRANSFORM_2D
				mm.use_custom_data = true
				mm.mesh = QuadMesh.new()
				mm_node.multimesh = mm
				
				# Read grid columns and rows directly from this animation block
				var cols: float = float(anim_data.get("cols", 1))
				var rows: float = float(anim_data.get("rows", 1))
				mm_node.set_meta("cols", cols)
				mm_node.set_meta("rows", rows)
				
				var frame_width_px: float = float(tex.get_width()) / cols
				var frame_height_px: float = float(tex.get_height()) / rows

				mm_node.set_meta("frame_width_px", frame_width_px)
				mm_node.set_meta("frame_height_px", frame_height_px)
				mm_node.set_meta("visual_pixels_per_cell", visual_pixels_per_cell)
				mm_node.set_meta("visual_pivot", visual_pivot)

				add_child(mm_node)
				
				# Store in dictionary using composite key
				enemy_multimeshes[material_key] = mm_node
			else:
				push_warning("Missing enemy texture path: " + tex_path)




func get_selected_hotbar_slot():
	return selected_hotbar_slot

func _on_custom_event(message: String) -> void:
	print("Received signal from C++: - ", message)
	
func _on_required_mesh_update():
	# We need a mesh update
	update_mesh_display()

func _init_building_materials() -> void:
	structure_registry = world.getStructureRegistry()

	# Standalone building sprites stay separate from block atlases.
	for id in structure_registry:
		var st: Dictionary = structure_registry[id]
		if not st.get("isBuilding", false):
			continue

		var sprite_filename: String = st.get("sprite", "")
		if sprite_filename.is_empty():
			continue

		var tex_path := "res://game/sprite/" + sprite_filename
		if ResourceLoader.exists(tex_path):
			var tex: Texture2D = load(tex_path)
			var mat := ShaderMaterial.new()
			mat.shader = textured_shader
			mat.set_shader_parameter("building_tex", tex)
			building_materials[id] = mat
		else:
			push_warning("Missing building texture: " + tex_path)


# Type-specific UVs are now resolved from the corresponding atlas rule.
func get_block_atlas_uv(type_id: int, uv: Vector2) -> Vector2:
	return block_atlases.get_uv(type_id, uv)

# Helper to get the full texture path for a structure sprite
func get_structure_sprite_path(structure_data: Dictionary) -> String:
	var sprite_filename = structure_data.get("sprite", "")
	if sprite_filename.is_empty():
		return ""
	# Adjust path according to where your sprites are stored
	return "res://game/sprite" + sprite_filename


func _register_entity_render_configs() -> void:
	var configs: Dictionary = {}

	for group_key in enemy_multimeshes:
		var mm_node: MultiMeshInstance2D = enemy_multimeshes[group_key]

		configs[group_key] = {
			"cols": int(mm_node.get_meta("cols", 1)),
			"rows": int(mm_node.get_meta("rows", 1)),
			"frame_width_px": float(
				mm_node.get_meta("frame_width_px", 1.0)
			),
			"frame_height_px": float(
				mm_node.get_meta("frame_height_px", 1.0)
			),
			"visual_pixels_per_cell": float(
				mm_node.get_meta("visual_pixels_per_cell", 30.0)
			),
			"visual_pivot": mm_node.get_meta(
				"visual_pivot",
				Vector2(0.5, 0.0)
			)
		}

		var mm: MultiMesh = mm_node.multimesh

		# Configure these BEFORE populating instances.
		# If instances already exist, reset instance_count first.
		mm.instance_count = 0
		mm.transform_format = MultiMesh.TRANSFORM_2D
		mm.use_colors = false
		mm.use_custom_data = true

	configs["__debug_enabled"] = SHOW_ENTITY_HITBOXES
	debug_multimesh_node.visible = SHOW_ENTITY_HITBOXES
	world.set_entity_render_configs(configs)



var entity_render_debug_printed := false


func _on_required_entity_update(
	render_buffers: Dictionary
) -> void:

	for group_key in enemy_multimeshes:
		var mm_node: MultiMeshInstance2D = enemy_multimeshes[group_key]
		var mm: MultiMesh = mm_node.multimesh

		if not render_buffers.has(group_key):
			if mm.instance_count != 0:
				mm.instance_count = 0
			continue

		var group_data: Dictionary = render_buffers[group_key]

		var buffer: PackedFloat32Array = group_data["buffer"]
		var bounds: AABB = group_data["aabb"]

		var count: int = int(buffer.size() / 12)

		if count == 0:
			if mm.instance_count != 0:
				mm.instance_count = 0
			continue

		# Update the actual spatial bounds of this group.
		if mm.custom_aabb != bounds:
			mm.custom_aabb = bounds

		if mm.instance_count != count:
			mm.instance_count = count

		mm.visible_instance_count = -1

		# Upload every entity transform in one operation.
		mm.buffer = buffer
		# Debug hitbox rendering
	if not SHOW_ENTITY_HITBOXES:
		return

	var debug_mm: MultiMesh = debug_multimesh_node.multimesh

	var debug_data: Dictionary = render_buffers.get("__debug", {})

	if debug_data.is_empty():
		debug_mm.instance_count = 0
		return

	var debug_buffer: PackedFloat32Array = debug_data["buffer"]
	var debug_count: int = debug_buffer.size() / 12

	if debug_count == 0:
		debug_mm.instance_count = 0
		return

	debug_mm.custom_aabb = debug_data["aabb"]

	if debug_mm.instance_count != debug_count:
		debug_mm.instance_count = debug_count

	debug_mm.visible_instance_count = -1
	debug_mm.buffer = debug_buffer



		
func _on_slot_action_requested(slot_number: int) -> void:
	#print("Main script heard that slot was clicked: ", slot_number)

	UIManager.close_top_window()
	selected_structure = structure_registry.get(slot_number, {})
	input_manager.set_input_mode(3)

func _on_hotbar_slot_selected(slot_number: int) -> void:
	#print("Main script received hotbar selection: Slot ", slot_number)
	input_manager.setSelectedHotbarSlot(slot_number)
	
	if slot_number == 3:
		selected_structure = structure_registry.get(1, {})
		input_manager.set_input_mode(InputManager.InputMode.BUILDING_PLACEMENT)
	elif slot_number == 4:
		selected_structure = structure_registry.get(2, {})
		input_manager.set_input_mode(InputManager.InputMode.BUILDING_PLACEMENT)
	elif slot_number == 5:
		selected_structure = structure_registry.get(3, {})
		input_manager.set_input_mode(InputManager.InputMode.BUILDING_PLACEMENT)
	elif slot_number == 6:
		selected_structure = structure_registry.get(4, {})
		input_manager.set_input_mode(InputManager.InputMode.BUILDING_PLACEMENT)
	elif slot_number == 7:
		selected_structure = structure_registry.get(257, {})
		input_manager.set_input_mode(InputManager.InputMode.BUILDING_PLACEMENT)
	elif slot_number == 8:
		selected_structure = structure_registry.get(258, {})
		input_manager.set_input_mode(InputManager.InputMode.BUILDING_PLACEMENT)
	else:
		selected_structure = {}
		input_manager.set_input_mode(InputManager.InputMode.FULL_GAMEPLAY)
		
func _draw_building_overlay():
	if selected_structure.is_empty():
		return
	var color := Color(0.325, 0.282, 0.29, 0.6)
	var w = selected_structure.get("width", 1)
	var h = selected_structure.get("height", 1)

	var size = Vector2(w, -h) * MESH_SCALE.x 

	var cell_x = int(input_manager.current_mouse_local.x)
	var cell_y = int(input_manager.current_mouse_local.y)
	var pos = Vector2(cell_x, cell_y+1) * MESH_SCALE.x

	var rect = Rect2(pos, size)

	draw_rect(rect, color, true)

func _place_selected_building():
	#print("Placing: ", selected_structure)
	var cell_x := int(input_manager.getCurMouseX())
	var cell_y := int(input_manager.getCurMouseY())
	var structureID = selected_structure.get("id")
	world.placeStructure(cell_x, cell_y, structureID)
	selected_structure = {}
	input_manager.set_input_mode(0)
	input_manager.setSelectedHotbarSlot(-1)
	update_mesh_display()

func _cancel_building():
	#print("Building cancelled")
	selected_structure = {}
	input_manager.set_input_mode(0)

func _draw() -> void:
	var grid_color := Color(1, 1, 1, 0.15)
	var line_width := 0.5
		# Mouse Radius Circle
	var circle_center := input_manager.current_mouse_local * MESH_SCALE
	var pixel_radius := input_manager.brush_radius_cells * MESH_SCALE.x
	draw_arc(circle_center, pixel_radius, 0, TAU, 32, Color(1, 1, 0, 0.8), 0.5)
	
	if not selected_structure.is_empty():
		_draw_building_overlay()

'''
	# Vertical grid lines
	for x in range(world_width):
		var start := Vector2(x, 0) * MESH_SCALE
		var end := Vector2(x, world_height) * MESH_SCALE
		draw_line(start, end, grid_color, line_width)

	# Horizontal grid lines
	for y in range(world_height):
		var start := Vector2(0, y) * MESH_SCALE
		var end := Vector2(world_width, y) * MESH_SCALE
		draw_line(start, end, grid_color, line_width)
'''


# Full rebuild (called once at start)
func _setup_full_mesh_display() -> void:
	var all_chunks: Dictionary = world.getMeshData() # Vector2i -> Chunk Dict
	for chunk_pos in all_chunks:
		_update_chunk_node(chunk_pos, all_chunks[chunk_pos])
		

# Delta update (called during editing)
func update_mesh_display() -> void:
	var updated_chunks: Array = world.updateMeshData() # Array of modified Chunk Dicts
	#print("Updated chunk count: ", updated_chunks.size()) # Should be > 0 on start & click
	for chunk_data in updated_chunks:
		#print("Chunk vertices: ", chunk_data["vertices"].size())
		var chunk_pos: Vector2i = chunk_data["chunk_pos"]
		_update_chunk_node(chunk_pos, chunk_data)

# The C++ marching-squares generator currently outputs a single material ID
# per shape, so triangles can be batched by atlas path without changing C++.
# Once the generator creates actual cross-atlas blends, this renderer will
# need paired atlas samplers / a different blend representation.
func _update_terrain_atlases(
	chunk_pos: Vector2i,
	terrain_data: Dictionary,
	terrain_nodes: Dictionary
) -> void:
	var verts: PackedVector2Array = terrain_data.get("vertices", PackedVector2Array())
	var inds: PackedInt32Array = terrain_data.get("indices", PackedInt32Array())
	var uvs: PackedVector2Array = terrain_data.get("uvs", PackedVector2Array())
	var colors: PackedColorArray = terrain_data.get("colors", PackedColorArray())

	# path -> packed mesh geometry for this atlas in this chunk.
	var groups: Dictionary = {}
	if uvs.size() != verts.size() or colors.size() != verts.size() or inds.size() % 3 != 0:
		push_warning("Malformed terrain mesh for chunk " + str(chunk_pos))
	else:
		for tri in range(0, inds.size(), 3):
			var ia: int = inds[tri]
			var ib: int = inds[tri + 1]
			var ic: int = inds[tri + 2]
			if min(ia, ib, ic) < 0 or max(ia, ib, ic) >= verts.size():
				continue

			# C++ packs primary type ID into color.b, secondary into color.a.
			var type_a: int = int(round(colors[ia].b))
			var atlas_info: Dictionary = block_atlases.get_mapping(type_a)
			if atlas_info.is_empty():
				_warn_unmapped_block(type_a)
				continue

			var path: String = atlas_info["path"]
			var type_b: int = int(round(colors[ib].b))
			var type_c: int = int(round(colors[ic].b))
			var atlas_b: Dictionary = block_atlases.get_mapping(type_b)
			var atlas_c: Dictionary = block_atlases.get_mapping(type_c)
			if atlas_b.is_empty() or atlas_c.is_empty() or atlas_b.get("path", "") != path or atlas_c.get("path", "") != path:
				push_warning("A terrain triangle spans multiple atlases: " + str(chunk_pos))
				continue

			if not groups.has(path):
				groups[path] = {
					"vertices": PackedVector2Array(),
					"uvs": PackedVector2Array(),
					"indices": PackedInt32Array()
				}

			var g: Dictionary = groups[path]
			var gv: PackedVector2Array = g["vertices"]
			var gu: PackedVector2Array = g["uvs"]
			var gi: PackedInt32Array = g["indices"]
			var base_index: int = gv.size()
			for idx in [ia, ib, ic]:
				var type_id: int = int(round(colors[idx].b))
				gv.append(verts[idx])
				gu.append(get_block_atlas_uv(type_id, uvs[idx]))
			gi.append(base_index)
			gi.append(base_index + 1)
			gi.append(base_index + 2)
			# Store the updated packed arrays explicitly (copy-on-write safe).
			g["vertices"] = gv
			g["uvs"] = gu
			g["indices"] = gi

	# One MeshInstance2D per atlas per chunk; materials shared across chunks.
	for path in groups:
		var terrain_node: MeshInstance2D = terrain_nodes.get(path, null)
		if terrain_node == null:
			terrain_node = MeshInstance2D.new()
			terrain_node.name = "Terrain_%d_%d_%s" % [
				chunk_pos.x, chunk_pos.y, str(path).get_file().get_basename()
			]
			terrain_node.scale = MESH_SCALE
			terrain_node.show_behind_parent = true
			terrain_node.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
			add_child(terrain_node)
			terrain_nodes[path] = terrain_node

		var g: Dictionary = groups[path]
		var arrays: Array = []
		arrays.resize(Mesh.ARRAY_MAX)
		arrays[Mesh.ARRAY_VERTEX] = g["vertices"]
		arrays[Mesh.ARRAY_TEX_UV] = g["uvs"]
		arrays[Mesh.ARRAY_INDEX] = g["indices"]

		var mesh := ArrayMesh.new()
		mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
		terrain_node.material = block_atlases.get_terrain_material(path)
		terrain_node.mesh = mesh

	# Empty or moved atlas groups must not leave stale geometry on screen.
	for path in terrain_nodes:
		if not groups.has(path):
			var stale: MeshInstance2D = terrain_nodes[path]
			stale.mesh = null


func _warn_unmapped_block(block_type: int) -> void:
	if _warned_unmapped_types.has(block_type):
		return
	_warned_unmapped_types[block_type] = true
	push_warning("No atlas mapping for block type %d" % block_type)

func _update_chunk_node(chunk_pos: Vector2i, chunk_data: Dictionary) -> void:
	var chunk_entry: Dictionary = chunk_nodes.get(chunk_pos, {})
	if chunk_entry.is_empty():
		chunk_entry = {
			"terrain": {},  # atlas path -> MeshInstance2D
			"structures": {}  # structure ID -> MeshInstance2D
		}
		chunk_nodes[chunk_pos] = chunk_entry

	var terrain_nodes: Dictionary = chunk_entry["terrain"]
	var structure_nodes: Dictionary = chunk_entry["structures"]
	_update_terrain_atlases(chunk_pos, chunk_data.get("terrain", {}), terrain_nodes)

	# --------------------------------------------------
	# Structures and blocks
	# --------------------------------------------------
	var struct_dict: Dictionary = chunk_data.get("structure", {})

	var s_verts: PackedVector2Array = struct_dict.get(
		"vertices", PackedVector2Array()
	)
	var s_inds: PackedInt32Array = struct_dict.get(
		"indices", PackedInt32Array()
	)
	var s_types: PackedInt32Array = struct_dict.get(
		"type_ids", PackedInt32Array()
	)
	var source_uvs: PackedVector2Array = struct_dict.get(
		"uvs", PackedVector2Array()
	)


	# Track which types occur in this update so stale nodes can be cleared.
	var active_types: Dictionary = {}

	if not s_verts.is_empty() and not s_inds.is_empty():
		var groups: Dictionary = {}

		# Group triangles by type ID.
		for t in range(s_inds.size() / 3):
			var idx0: int = s_inds[t * 3]
			var idx1: int = s_inds[t * 3 + 1]
			var idx2: int = s_inds[t * 3 + 2]

			if (
				idx0 < 0 or idx1 < 0 or idx2 < 0
				or idx0 >= s_verts.size()
				or idx1 >= s_verts.size()
				or idx2 >= s_verts.size()
			):
				continue

			var type_id: int = (
				s_types[idx0] if idx0 < s_types.size() else -1
			)

			if type_id == -1:
				continue

			if not groups.has(type_id):
				groups[type_id] = {
					"vertices": PackedVector2Array(),
					"uvs": PackedVector2Array(),
					"indices": PackedInt32Array()
				}

			var g: Dictionary = groups[type_id]
			var base_idx: int = g["vertices"].size()

			g["vertices"].append(s_verts[idx0])
			g["vertices"].append(s_verts[idx1])
			g["vertices"].append(s_verts[idx2])

			var uv0: Vector2 = (
				source_uvs[idx0]
				if idx0 < source_uvs.size()
				else Vector2.ZERO
			)
			var uv1: Vector2 = (
				source_uvs[idx1]
				if idx1 < source_uvs.size()
				else Vector2.ZERO
			)
			var uv2: Vector2 = (
				source_uvs[idx2]
				if idx2 < source_uvs.size()
				else Vector2.ZERO
			)

			var st_data: Dictionary = structure_registry.get(type_id, {})
			var is_building: bool = st_data.get("isBuilding", false)

			# Structure ID and underlying blockType may differ.
			var block_type: int = int(st_data.get("blockType", 0))
			if block_type <= 0:
				block_type = type_id

			# Blocks use the configured atlas; buildings use their whole image.
			if not is_building:
				uv0 = get_block_atlas_uv(block_type, uv0)
				uv1 = get_block_atlas_uv(block_type, uv1)
				uv2 = get_block_atlas_uv(block_type, uv2)

			g["uvs"].append(uv0)
			g["uvs"].append(uv1)
			g["uvs"].append(uv2)

			g["indices"].append(base_idx)
			g["indices"].append(base_idx + 1)
			g["indices"].append(base_idx + 2)
			
			var min_uv := Vector2(INF, INF)
			var max_uv := Vector2(-INF, -INF)

			for uv in g["uvs"]:
				min_uv.x = minf(min_uv.x, uv.x)
				min_uv.y = minf(min_uv.y, uv.y)
				max_uv.x = maxf(max_uv.x, uv.x)
				max_uv.y = maxf(max_uv.y, uv.y)

			#print("Building type: ", type_id,
		  #" UV range: ", min_uv, " to ", max_uv)
			#print("Type ", type_id, " first triangle UVs: ",
			#g["uvs"].slice(0, 6))

		# Build one independent mesh per type ID.
		for type_id_variant in groups.keys():
			var type_id: int = type_id_variant
			var g: Dictionary = groups[type_id]

			active_types[type_id] = true

			var st_data: Dictionary = structure_registry.get(type_id, {})
			var is_building: bool = st_data.get("isBuilding", false)

			# Create the node if this type hasn't appeared in this chunk before.
			var structure_node: MeshInstance2D = structure_nodes.get(
				type_id, null
			)

			if structure_node == null:
				structure_node = MeshInstance2D.new()
				structure_node.name = "Structure_%s_%s_%s" % [
					chunk_pos.x,
					chunk_pos.y,
					type_id
				]
				structure_node.scale = MESH_SCALE
				structure_node.show_behind_parent = false

				add_child(structure_node)
				structure_nodes[type_id] = structure_node

			# Select the texture matching the blockType, not structureID.
			if is_building:
				structure_node.material = building_materials.get(type_id, null)
			else:
				var block_type: int = int(st_data.get("blockType", 0))
				if block_type <= 0:
					block_type = type_id
				structure_node.material = block_atlases.get_block_material(block_type)

			# Warn if this type has no assigned texture material.
			if structure_node.material == null:
				push_warning(
					"No texture material assigned for type ID: "
					+ str(type_id)
				)

			var structure_mesh := ArrayMesh.new()
			var s_array: Array = []
			s_array.resize(Mesh.ARRAY_MAX)
			s_array[Mesh.ARRAY_VERTEX] = g["vertices"]
			s_array[Mesh.ARRAY_INDEX] = g["indices"]
			s_array[Mesh.ARRAY_TEX_UV] = g["uvs"]
			#print(
			#	"TYPE=", type_id,
			#	" VERTEX COUNT=", g["vertices"].size(),
			#	" FIRST VERTICES=", g["vertices"].slice(0, 6),
			#	" FIRST UVS=", g["uvs"].slice(0, 6)
			#)

			structure_mesh.add_surface_from_arrays(
				Mesh.PRIMITIVE_TRIANGLES,
				s_array
			)

			structure_node.mesh = structure_mesh

	# --------------------------------------------------
	# Clear geometry for types no longer present
	# --------------------------------------------------
	for type_id_variant in structure_nodes.keys():
		var type_id: int = type_id_variant

		if not active_types.has(type_id):
			var stale_node: MeshInstance2D = structure_nodes[type_id]
			stale_node.mesh = null

	# Save the updated node dictionary.
	chunk_entry["structures"] = structure_nodes
	chunk_nodes[chunk_pos] = chunk_entry
