extends Node2D

@onready var world: GodotSimulation = $GodotSimulation

var chunk_nodes: Dictionary = {} # Vector2i(cx, cy) -> MeshInstance2D
var camera: Camera2D
var shader_mat: ShaderMaterial

# Mesh configuration
const MESH_SCALE := Vector2(4.0, 4.0)

# Camera settings
var min_zoom: float = 0.2
var max_zoom: float = 10.0
var zoom_factor: float = 0.1
var is_panning: bool = false

# Brush & Grid settings
var cell_size: float = 128.0
var world_width: int = 6400
var world_height: int = 1800
var brush_radius_cells: float = 2.0
var current_mouse_local: Vector2 = Vector2.ZERO

var is_left_down: bool = false
var is_right_down: bool = false
var paint_cooldown: float = 0.0
const PAINT_INTERVAL: float = 0.1

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

func _process(_delta: float) -> void:
	# Convert global mouse position to local cell coordinates
	current_mouse_local = get_local_mouse_position() / MESH_SCALE
	queue_redraw()
	
	if paint_cooldown > 0.0:
		paint_cooldown -= _delta

	if (is_left_down or is_right_down) and paint_cooldown <= 0.0:
		_apply_brush()
		paint_cooldown = PAINT_INTERVAL

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
	var circle_center := current_mouse_local * MESH_SCALE
	var pixel_radius := brush_radius_cells * MESH_SCALE.x
	draw_arc(circle_center, pixel_radius, 0, TAU, 32, Color(1, 1, 0, 0.8), 0.5)

# Full rebuild (called once at start)
func _setup_full_mesh_display() -> void:
	var all_chunks: Dictionary = world.getMeshData() # Vector2i -> Chunk Dict
	for chunk_pos in all_chunks:
		_update_chunk_node(chunk_pos, all_chunks[chunk_pos])

# Delta update (called during editing)
func update_mesh_display() -> void:
	var updated_chunks: Array = world.updateMeshData() # Array of modified Chunk Dicts
	print("Updated chunk count: ", updated_chunks.size()) # Should be > 0 on start & click
	for chunk_data in updated_chunks:
		print("Chunk vertices: ", chunk_data["vertices"].size())
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

func _apply_brush() -> void:
	var cell_x := int(current_mouse_local.x)
	var cell_y := int(current_mouse_local.y)

	if is_left_down:
		print("Painting at: ", cell_x, ", ", cell_y)
		world.setDensity(cell_x, cell_y, brush_radius_cells, 256, 2)
	elif is_right_down:
		print("Erasing at: ", cell_x, ", ", cell_y)
		world.setDensity(cell_x, cell_y, brush_radius_cells, -256, 2)

	update_mesh_display()

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_LEFT:
			is_left_down = event.pressed
		elif event.button_index == MOUSE_BUTTON_RIGHT:
			is_right_down = event.pressed

		if event.pressed and (is_left_down or is_right_down):
			_apply_brush()
			paint_cooldown = PAINT_INTERVAL

		elif event.button_index == MOUSE_BUTTON_WHEEL_UP:
			_zoom_camera(1.0 + zoom_factor, event.position)
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			_zoom_camera(1.0 - zoom_factor, event.position)

	elif event is InputEventKey and event.pressed:
		if event.keycode == KEY_A:
			print("FILL AT: ", int(current_mouse_local.x), ", ", int(current_mouse_local.y))
			world.fillCell(int(current_mouse_local.x), int(current_mouse_local.y), 2, false)
			update_mesh_display()
		elif event.keycode == KEY_S:
			print("FILL AT: ", int(current_mouse_local.x), ", ", int(current_mouse_local.y))
			world.fillCell(int(current_mouse_local.x), int(current_mouse_local.y), 2, true)
			update_mesh_display()

		elif event.keycode == KEY_1:
			brush_radius_cells -= 0.5
		elif event.keycode == KEY_2:
			brush_radius_cells += 0.5

func _input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_MIDDLE:
			is_panning = event.pressed

	if event is InputEventMouseMotion and is_panning:
		camera.global_position -= event.relative / camera.zoom

func _zoom_camera(factor: float, _mouse_screen_pos: Vector2) -> void:
	var old_zoom := camera.zoom
	var new_zoom := (old_zoom * factor).clamp(Vector2(min_zoom, min_zoom), Vector2(max_zoom, max_zoom))

	if old_zoom == new_zoom:
		return

	var mouse_world_before := camera.get_global_mouse_position()
	camera.zoom = new_zoom
	var mouse_world_after := camera.get_global_mouse_position()

	camera.position += (mouse_world_before - mouse_world_after)
