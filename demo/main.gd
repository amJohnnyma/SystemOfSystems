extends Node2D

@onready var world: GodotSimulation = $GodotSimulation
@onready var input_manager: InputManager = $InputManager

var chunk_nodes: Dictionary = {} # Vector2i(cx, cy) -> MeshInstance2D
var camera: Camera2D
var shader_mat: ShaderMaterial

# Mesh configuration
const MESH_SCALE := Vector2(4.0, 4.0)
var cell_size: float = 64.0
var world_width: int = 6400
var world_height: int = 1800


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

	# Initialize C++ world grid (world_x, world_y, g_size)
	world.createWorld(world_width, world_height, int(cell_size))
	
	# Populate all chunk nodes initially
	_setup_full_mesh_display()
	update_mesh_display()
	
	# Connect to InputManager signals
	input_manager.setup(world, camera, MESH_SCALE)
	input_manager.terrain_modified.connect(update_mesh_display)
	input_manager.cursor_moved.connect(func(_pos): queue_redraw())
	input_manager.brush_radius_changed.connect(func(_radius): queue_redraw())
	
	# Connect using the standard GDScript signal syntax
	world.test_signal_event.connect(_on_custom_event)
	
	# Call the C++ method that triggers the emission
	world.trigger_test_signal("MEOW")
	
func _on_custom_event(message: String) -> void:
	print("Received signal from C++: - ", message)


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
	var vertices: PackedVector2Array = chunk_data.get("vertices", PackedVector2Array())
	var indices: PackedInt32Array = chunk_data.get("indices", PackedInt32Array())
	var type_ids: PackedInt32Array = chunk_data.get("type_ids", PackedInt32Array())

	# Fetch existing node or instantiate a new chunk node
	var chunk_node: MeshInstance2D = chunk_nodes.get(chunk_pos, null)
	if chunk_node == null:
		chunk_node = MeshInstance2D.new()
		chunk_node.scale = MESH_SCALE
		chunk_node.show_behind_parent = true
		chunk_node.material = shader_mat
		add_child(chunk_node)
		chunk_nodes[chunk_pos] = chunk_node

	# If the mesh chunk is empty, clear the node mesh and return
	if vertices.is_empty() or indices.is_empty():
		chunk_node.mesh = null
		return

	# Build colors array
	var colors := PackedColorArray()
	colors.resize(type_ids.size())
	for i in range(type_ids.size()):
		colors[i] = type_colors.get(type_ids[i], Color.WHITE)

	# Assign to ArrayMesh
	var surface_array: Array = []
	surface_array.resize(Mesh.ARRAY_MAX)
	surface_array[Mesh.ARRAY_VERTEX] = vertices
	surface_array[Mesh.ARRAY_INDEX] = indices
	surface_array[Mesh.ARRAY_COLOR] = colors

	var array_mesh := ArrayMesh.new()
	array_mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, surface_array)
	chunk_node.mesh = array_mesh
