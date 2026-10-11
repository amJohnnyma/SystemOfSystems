extends Node2D

@onready var world: GodotSimulation = $GodotSimulation
@onready var input_manager: InputManager = $InputManager
@onready var ui_root_scene = preload("res://demo/ui_root.tscn")

var chunk_nodes: Dictionary = {} # Vector2i(cx, cy) -> MeshInstance2D
var camera: Camera2D
var shader_mat: ShaderMaterial

# Mesh configuration
const MESH_SCALE := Vector2(4.0, 4.0)
var cell_size: float = 64.0
var world_width: int = 6400
var world_height: int = 1800

var structure_registry: Dictionary = {}
var selected_structure: Dictionary = {}

var selected_hotbar_slot = -1


var type_colors: Dictionary = {
	0: Color.TRANSPARENT,
	1: Color(0.4, 0.3, 0.2), # Dirt (Brown)
	2: Color(0.2, 0.8, 0.3), # Grass (Green)
	3: Color(0.5, 0.5, 0.5), # Stone (Gray)
}

func _ready() -> void:
	# Create and configure Camera2D
	camera = Camera2D.new()
	add_child(camera)
	camera.make_current()

	# Cache shared shader material
	shader_mat = ShaderMaterial.new()
	shader_mat.shader = preload("res://shaders/flat_colour.gdshader")


	
	# Connect to InputManager signals
	input_manager.setup(world, camera, MESH_SCALE)
	input_manager.terrain_modified.connect(update_mesh_display)
	input_manager.cursor_moved.connect(func(_pos): queue_redraw())
	input_manager.brush_radius_changed.connect(func(_radius): queue_redraw())
	input_manager.building_placed.connect(_place_selected_building)
	input_manager.building_cancelled.connect(_cancel_building)

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
	
	# Call the C++ method that triggers the emission
	world.trigger_test_signal("MEOW")
	
		# Initialize C++ world grid (world_x, world_y, g_size)
	world.createWorld(world_width, world_height, int(cell_size))
	structure_registry = world.getStructureRegistry()
	print("Loaded structures into Godot: ", structure_registry)
	
	# Populate all chunk nodes initially
	_setup_full_mesh_display()
	update_mesh_display()
	
func get_selected_hotbar_slot():
	return selected_hotbar_slot

func _on_custom_event(message: String) -> void:
	print("Received signal from C++: - ", message)

func _on_slot_action_requested(slot_number: int) -> void:
	print("Main script heard that slot was clicked: ", slot_number)

	UIManager.close_top_window()
	selected_structure = structure_registry.get(slot_number, {})
	input_manager.set_input_mode(3)

func _on_hotbar_slot_selected(slot_number: int) -> void:
	print("Main script received hotbar selection: Slot ", slot_number)
	input_manager.setSelectedHotbarSlot(slot_number)
	
	if slot_number == 3:
		selected_structure = structure_registry.get(1, {})
		input_manager.set_input_mode(InputManager.InputMode.BUILDING_PLACEMENT)
	elif slot_number == 4:
		selected_structure = structure_registry.get(2, {})
		input_manager.set_input_mode(InputManager.InputMode.BUILDING_PLACEMENT)
	elif slot_number == 5:
		selected_structure = structure_registry.get(2, {})
		input_manager.set_input_mode(InputManager.InputMode.BUILDING_PLACEMENT)
	else:
		selected_structure = {}
		input_manager.set_input_mode(InputManager.InputMode.FULL_GAMEPLAY)
		
func _draw_building_overlay():
	# For now just a box with the dimensions
	var color := Color(0.325, 0.282, 0.29, 1.0)
	var w = selected_structure.get("width", 1)
	var h = selected_structure.get("height", 1)

	var size = Vector2(w, -h) * MESH_SCALE.x 

	var cell_x = int(input_manager.current_mouse_local.x)
	var cell_y = int(input_manager.current_mouse_local.y)
	var pos = Vector2(cell_x, cell_y+1) * MESH_SCALE.x

	var rect = Rect2(pos, size)

	draw_rect(rect, color, false, 0.5)

func _place_selected_building():
	print("Placing: ", selected_structure)
	var cell_x := int(input_manager.getCurMouseX())
	var cell_y := int(input_manager.getCurMouseY())
	var structureID = selected_structure.get("id")
	world.placeStructure(cell_x, cell_y, structureID)
	selected_structure = {}
	input_manager.set_input_mode(0)
	input_manager.setSelectedHotbarSlot(-1)
	update_mesh_display()

func _cancel_building():
	print("Building cancelled")
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

# Internal helper to construct/update a single chunk's MeshInstance2D
func _update_chunk_node(chunk_pos: Vector2i, chunk_data: Dictionary) -> void:
	var chunk_node: MeshInstance2D = chunk_nodes.get(chunk_pos, null)
	if chunk_node == null:
		chunk_node = MeshInstance2D.new()
		chunk_node.scale = MESH_SCALE
		chunk_node.show_behind_parent = true
		add_child(chunk_node)
		chunk_nodes[chunk_pos] = chunk_node

	var terrain_dict: Dictionary = chunk_data.get("terrain", {})
	var struct_dict: Dictionary = chunk_data.get("structure", {})

	var array_mesh := ArrayMesh.new()

	# --- Surface 0: Smooth Terrain ---
	var t_verts: PackedVector2Array = terrain_dict.get("vertices", PackedVector2Array())
	var t_inds: PackedInt32Array = terrain_dict.get("indices", PackedInt32Array())
	var t_types: PackedInt32Array = terrain_dict.get("type_ids", PackedInt32Array())

	if not t_verts.is_empty() and not t_inds.is_empty():
		var t_colors := PackedColorArray()
		t_colors.resize(t_types.size())
		for i in range(t_types.size()):
			t_colors[i] = type_colors.get(t_types[i], Color.WHITE)

		var t_array: Array = []
		t_array.resize(Mesh.ARRAY_MAX)
		t_array[Mesh.ARRAY_VERTEX] = t_verts
		t_array[Mesh.ARRAY_INDEX] = t_inds
		t_array[Mesh.ARRAY_COLOR] = t_colors
		
		array_mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, t_array)
		# Assign your marching squares shader to surface 0
		chunk_node.material = shader_mat 

	# --- Surface 1: Crisp Structures ---
	var s_verts: PackedVector2Array = struct_dict.get("vertices", PackedVector2Array())
	var s_inds: PackedInt32Array = struct_dict.get("indices", PackedInt32Array())
	var s_types: PackedInt32Array = struct_dict.get("type_ids", PackedInt32Array())

	if not s_verts.is_empty() and not s_inds.is_empty():
		var s_colors := PackedColorArray()
		s_colors.resize(s_types.size())
		for i in range(s_types.size()):
			s_colors[i] = type_colors.get(s_types[i], Color.WHITE)

		var s_array: Array = []
		s_array.resize(Mesh.ARRAY_MAX)
		s_array[Mesh.ARRAY_VERTEX] = s_verts
		s_array[Mesh.ARRAY_INDEX] = s_inds
		s_array[Mesh.ARRAY_COLOR] = s_colors
		
		array_mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, s_array)
		# Surface 1 will use default blocky rendering (no shader or a crisp building material)

	chunk_node.mesh = array_mesh if array_mesh.get_surface_count() > 0 else null
