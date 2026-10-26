extends Node2D

@onready var world: GodotSimulation = $GodotSimulation
@onready var input_manager: InputManager = $InputManager
@onready var ui_root_scene = preload("res://demo/ui_root.tscn")

var chunk_nodes: Dictionary = {
	"terrain":MeshInstance2D,
	"structures":{"type_id" : MeshInstance2D}
}
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
var selected_structure: Dictionary = {}
var textured_shader: Shader
var building_materials = {}

var selected_hotbar_slot = -1

const BLOCK_ATLAS_PATH := "res://game/atlas/terrain.png"
const BLOCK_TILE_SIZE := Vector2i(16, 16)

var BLOCK_ATLAS_COORDS: Dictionary[int, Vector2i] = \
	AtlasUtils.generate_atlas_coords(
		Vector2i(256, 256),
		BLOCK_TILE_SIZE
	)
	
var block_atlas: Texture2D
var block_atlas_material: ShaderMaterial
var terrain_blend_shader: Shader
var terrain_blend_material: ShaderMaterial

var enemy_multimeshes : Dictionary = {}
var enemy_materials : Dictionary = {}
var debug_multimesh_node: MultiMeshInstance2D
var enemy_shader: Shader

const PLAYER_ANIMATIONS := {
	"idle": {
		"sprite": "Knight_idle.png",
		"cols": 14,
		"rows": 2
	},
	"walk": {
		"sprite": "Knight_walk.png",
		"cols": 8,
		"rows": 1
	}
}

var player_multimeshes: Dictionary = {}
var player_materials: Dictionary = {}
var player_debug_multimesh_node: MultiMeshInstance2D





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

	terrain_blend_material = ShaderMaterial.new()
	terrain_blend_material.shader = terrain_blend_shader
	terrain_blend_material.set_shader_parameter(
		"terrain_atlas", load(BLOCK_ATLAS_PATH)
	)
	





	
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
	
	_on_player_data_updated(world.getPlayerData())
	
	
	#print("Loaded structures into Godot: ", structure_registry)
	print("Loaded entities into Godot:\n", JSON.stringify(entity_registry, "\t"))
	
	
	

	
	
	for type_id: int in BLOCK_ATLAS_COORDS:
		var atlas_coords: Vector2i = BLOCK_ATLAS_COORDS[type_id]

		var x: int = atlas_coords.x
		var y: int = atlas_coords.y

		world.placeStructure(x, y, type_id)
	# Populate all chunk nodes initially
	_setup_full_mesh_display()
	update_mesh_display()
	
	
	camera.global_position = Vector2(world_width * MESH_SCALE.x, world_height * MESH_SCALE.y) / 2.0
	
	#world.placeEntity(320,10,1)
	#world.placeEntity(320,30,2)
	world.placeEntity(330,30,3)
	'''for i in range(200):
		world.placeEntity(0,2,1)
		world.placeEntity(0,6,2)
		world.placeEntity(2,2,3)
		world.placeEntity(2,5,4)
		world.placeEntity(0,7,5)'''

	
	
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

	world.createPlayer(data)
func _on_player_data_updated(data: Dictionary) -> void:
	if data.is_empty() or player_multimeshes.is_empty():
		return

	var state: String = str(data.get("state", "idle"))

	# Fall back to idle when the player reports
	# a state without a configured spritesheet.
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

	# Position and size from C++
	var grid_x: float = float(data.get("x_smooth", 0.0))
	var grid_y: float = float(data.get("y_smooth", 0.0))
	var w: float = float(data.get("width", 2.0))
	var h: float = float(data.get("height", 3.0))

	# Same transform convention as entities
	var center := Vector2(
		grid_x + w * 0.5,
		grid_y - h * 0.5 + 1.0
	)

	var facing_right: bool = bool(data.get("facingRight", true))
	var x_scale: float = w if facing_right else -w

	var xform := Transform2D(
		Vector2(x_scale, 0.0),
		Vector2(0.0, -h),
		center
	)

	mm.set_instance_transform_2d(0, xform)

	# Animation frame selection
	var total_cols: int = max(1, int(mm_node.get_meta("cols", 1)))
	var total_rows: int = max(1, int(mm_node.get_meta("rows", 1)))
	var total_frames: int = total_cols * total_rows

	var anim_phase: float = float(data.get("animPhase", 0.0))

	var current_frame: int = int(floor(
		anim_phase * total_frames
	)) % total_frames

	var frame_col: float = float(current_frame % total_cols)
	var frame_row: float = float(
		floori(float(current_frame) / float(total_cols))
	)

	mm.set_instance_custom_data(
		0,
		Color(
			frame_col,
			frame_row,
			float(total_cols),
			float(total_rows)
		)
	)
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
		var sprite_name: String = anim_data.get("sprite", "")

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

		mm_node.set_meta("cols", max(1, int(anim_data.get("cols", 1))))
		mm_node.set_meta("rows", max(1, int(anim_data.get("rows", 1))))

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

				add_child(mm_node)
				
				# Store in dictionary using composite key
				enemy_multimeshes[material_key] = mm_node
			else:
				push_warning("Missing enemy texture path: " + tex_path)

func _init_building_materials() -> void:
	structure_registry = world.getStructureRegistry()
	
		
		
	# Shared atlas material for ordinary blocks.
	if ResourceLoader.exists(BLOCK_ATLAS_PATH):
		block_atlas = load(BLOCK_ATLAS_PATH)

		block_atlas_material = ShaderMaterial.new()
		block_atlas_material.shader = textured_shader
		block_atlas_material.set_shader_parameter(
			"building_tex", block_atlas
		)
	else:
		push_error("Block atlas not found: " + BLOCK_ATLAS_PATH)
	#print("Atlas loaded: ", block_atlas != null)
	if block_atlas != null:
		#print("Atlas size: ", block_atlas.get_size())
		#print("Atlas resource path: ", block_atlas.resource_path)
		pass
	# Individual textures for buildings.
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
			#print("Loaded building texture: ", tex_path)

		else:
			push_warning("Missing building texture: " + tex_path)


func get_block_atlas_uv(type_id: int, uv: Vector2) -> Vector2:
	if block_atlas == null:
		return uv

	if not BLOCK_ATLAS_COORDS.has(type_id):
		return uv

	var tile_coord: Vector2i = BLOCK_ATLAS_COORDS[type_id]
	var atlas_size := Vector2(block_atlas.get_size())
	var tile_size := Vector2(BLOCK_TILE_SIZE)

	return (
		Vector2(tile_coord) * tile_size + uv * tile_size
	) / atlas_size


func get_selected_hotbar_slot():
	return selected_hotbar_slot

func _on_custom_event(message: String) -> void:
	print("Received signal from C++: - ", message)
	
func _on_required_mesh_update():
	# We need a mesh update
	update_mesh_display()


# Helper to get the full texture path for a structure sprite
func get_structure_sprite_path(structure_data: Dictionary) -> String:
	var sprite_filename = structure_data.get("sprite", "")
	if sprite_filename.is_empty():
		return ""
	# Adjust path according to where your sprites are stored
	return "res://game/sprite" + sprite_filename

func _on_required_entity_update(batch_data: Dictionary) -> void:
	# 1. Group entity data by composite key: "type_id_state"
	var grouped_entities: Dictionary = {} # "type_id_state" -> Array of pos_data dicts
	var all_entities: Array = []

	for uID in batch_data:
		var pos_data: Dictionary = batch_data[uID]
		all_entities.append(pos_data)
		
		var type_id: int = int(pos_data.get("entityTypeID", 1))
		var state: String = str(pos_data.get("state", "idle")) # Read state passed from C++
		
		
		var group_key := "%d_%s" % [type_id, state]

		if not grouped_entities.has(group_key):
			grouped_entities[group_key] = []
		grouped_entities[group_key].append(pos_data)

	# 2. Update each MultiMeshInstance2D per state bucket
	for group_key in enemy_multimeshes:
		var mm_node: MultiMeshInstance2D = enemy_multimeshes[group_key]
		var mm: MultiMesh = mm_node.multimesh
		var entities: Array = grouped_entities.get(group_key, [])
		
		# Retrieve this animation state's sheet dimensions from metadata
		var total_cols: float = mm_node.get_meta("cols", 1.0)
		var total_rows: float = mm_node.get_meta("rows", 1.0)

		mm.instance_count = entities.size()

		for idx in range(entities.size()):
			var pos_data: Dictionary = entities[idx]

			var grid_x: float = float(pos_data["x_smooth"])
			var grid_y: float = float(pos_data["y_smooth"])
			var w: float = float(pos_data.get("width", 2))
			var h: float = float(pos_data.get("height", 2))

			var center := Vector2(
				grid_x + w * 0.5,
				grid_y - h * 0.5 + 1.0
			)

			var facing_right: bool = bool(pos_data.get("facingRight", true))
			var x_scale: float = w if facing_right else -w

			var xform := Transform2D(
				Vector2(x_scale, 0.0), # Flips width scale when facing left
				Vector2(0.0, -h),
				center
			)
			mm.set_instance_transform_2d(idx, xform)

			# -----------------------------------------------------------------
			# TEMPORARY / DYNAMIC: Map animPhase (0.0 to 1.0) to frame index
			# -----------------------------------------------------------------
			var anim_phase: float = float(pos_data.get("animPhase", 0.0))
			#print(anim_phase)
			var total_frames: int = int(total_cols * total_rows)
			
			# Snap phase to a discrete frame index
			var current_frame: int = int(floor(anim_phase * total_frames)) % max(1, total_frames)
			
			# Convert linear frame index back into 2D grid column and row coordinates
			var frame_col: float = float(current_frame % int(total_cols))
			var frame_row: float = float(current_frame / int(total_cols))

			mm.set_instance_custom_data(
				idx, 
				Color(frame_col, frame_row, total_cols, total_rows)
			)

	# 3. Update Debug Origin MultiMesh

	
	var debug_mm = debug_multimesh_node.multimesh if debug_multimesh_node else null
	if debug_mm:		
		debug_mm.instance_count = all_entities.size()
		for idx in range(all_entities.size()):
			var pos_data: Dictionary = all_entities[idx]
			var origin_x: int = int(pos_data.get("x_origin", floor(float(pos_data["x_smooth"]))))
			var origin_y: int = int(pos_data.get("y_origin", floor(float(pos_data["y_smooth"]))))
			var w: float = float(pos_data.get("width", 2))
			var h: float = float(pos_data.get("height", 2))
			
			var center := Vector2(
				origin_x + w * 0.5,
				origin_y - h * 0.5 + 1.0
			)

			var facing_right: bool = bool(pos_data.get("facingRight", true))
			var x_scale: float = w if facing_right else -w

			var xform := Transform2D(
				Vector2(x_scale, 0.0), # Flips width scale when facing left
				Vector2(0.0, -h),
				center
			)


			debug_mm.set_instance_transform_2d(idx, xform)
			debug_mm.set_instance_custom_data(idx, Color(1, 0, 1, 0.3)) # Magenta debug marker

		
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

	# Mouse Radius Circle
	var circle_center := input_manager.current_mouse_local * MESH_SCALE
	var pixel_radius := input_manager.brush_radius_cells * MESH_SCALE.x
	draw_arc(circle_center, pixel_radius, 0, TAU, 32, Color(1, 1, 0, 0.8), 0.5)
	
	if not selected_structure.is_empty():
		_draw_building_overlay()

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


func _update_chunk_node(chunk_pos: Vector2i, chunk_data: Dictionary) -> void:
	var chunk_entry: Dictionary = chunk_nodes.get(chunk_pos, {})

	# --------------------------------------------------
	# Create or retrieve the chunk's nodes
	# --------------------------------------------------
	if chunk_entry.is_empty():
		var terrain_node := MeshInstance2D.new()
		terrain_node.name = "Terrain_%s_%s" % [chunk_pos.x, chunk_pos.y]
		terrain_node.scale = MESH_SCALE
		terrain_node.show_behind_parent = true
		terrain_node.material = terrain_blend_material
		add_child(terrain_node)

		chunk_entry = {
			"terrain": terrain_node,
			"structures": {}
		}

		chunk_nodes[chunk_pos] = chunk_entry

	var terrain_node: MeshInstance2D = chunk_entry["terrain"]
	var structure_nodes: Dictionary = chunk_entry["structures"]

# --------------------------------------------------
# Terrain mesh with two-texture blending
# --------------------------------------------------
	var terrain_dict: Dictionary = chunk_data.get("terrain", {})

	var t_verts: PackedVector2Array = terrain_dict.get(
		"vertices", PackedVector2Array()
	)
	var t_inds: PackedInt32Array = terrain_dict.get(
		"indices", PackedInt32Array()
	)
	var t_uvs: PackedVector2Array = terrain_dict.get(
		"uvs", PackedVector2Array()
	)
	var t_colors: PackedColorArray = terrain_dict.get(
		"colors", PackedColorArray()
	)

	var terrain_mesh := ArrayMesh.new()

	if not t_verts.is_empty() and not t_inds.is_empty():
		var valid_data := (
			t_uvs.size() == t_verts.size()
			and t_colors.size() == t_verts.size()
		)

		if not valid_data:
			push_warning(
	            "Terrain mesh arrays have mismatched vertex counts."
			)
		else:
			var atlas_uvs_a := PackedVector2Array()
			var atlas_uvs_b := PackedFloat32Array()
			var blend_colors := PackedColorArray()

			atlas_uvs_a.resize(t_verts.size())
			atlas_uvs_b.resize(t_verts.size() * 2)
			blend_colors.resize(t_verts.size())

			for i in range(t_verts.size()):
				var local_uv: Vector2 = t_uvs[i]
				var source_color: Color = t_colors[i]

				var type_a: int = int(round(source_color.b))
				var type_b: int = int(round(source_color.a))

				# Resolve both terrain types to their atlas UVs.
				atlas_uvs_a[i] = get_block_atlas_uv(
					type_a, local_uv
				)
				var uv_b: Vector2 = get_block_atlas_uv(type_b, local_uv)

				var offset := i * 2
				atlas_uvs_b[offset] = uv_b.x
				atlas_uvs_b[offset + 1] = uv_b.y

				# Only pass blend weights to the shader.
				blend_colors[i] = Color(
					source_color.r,
					source_color.g,
					0.0,
					1.0
				)

			var t_array: Array = []
			t_array.resize(Mesh.ARRAY_MAX)

			t_array[Mesh.ARRAY_VERTEX] = t_verts
			t_array[Mesh.ARRAY_INDEX] = t_inds
			t_array[Mesh.ARRAY_TEX_UV] = atlas_uvs_a
			t_array[Mesh.ARRAY_CUSTOM0] = atlas_uvs_b
			t_array[Mesh.ARRAY_COLOR] = blend_colors

			var format := (
				Mesh.ARRAY_FORMAT_CUSTOM0
				| (Mesh.ARRAY_CUSTOM_RG_FLOAT << Mesh.ARRAY_FORMAT_CUSTOM0_SHIFT)
			)

			terrain_mesh.add_surface_from_arrays(
				Mesh.PRIMITIVE_TRIANGLES,
				t_array,
				[],
				{},
				format
			)

	terrain_node.material = terrain_blend_material
	terrain_node.mesh = (
		terrain_mesh
		if terrain_mesh.get_surface_count() > 0
		else null
	)


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

			# Blocks use atlas UVs; buildings retain their sprite UVs.
			if not is_building:
				uv0 = get_block_atlas_uv(type_id, uv0)
				uv1 = get_block_atlas_uv(type_id, uv1)
				uv2 = get_block_atlas_uv(type_id, uv2)

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

			# Assign the expected material on every update.
			if is_building:
				structure_node.material = building_materials.get(type_id, null)
			else:
				structure_node.material = block_atlas_material

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
