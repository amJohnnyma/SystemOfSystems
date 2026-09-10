class_name InputManager

extends Node


# Signals
signal terrain_modified()
signal cursor_moved(local_cell_pos: Vector2)
signal brush_radius_changed(new_radius : float)


# Global toggles
var is_input_enabled: bool = true

# Dependencies
var world : GodotSimulation
var camera : Camera2D
var mesh_scale : Vector2 = Vector2(4.0, 4.0)

# Brush & Timing
var brush_radius_cells : float = 6.0
var current_mouse_local: Vector2 = Vector2.ZERO
var is_left_down : bool = false
var is_right_down : bool = false
var is_panning : bool = false
var paint_cooldown : float = 0.0
const PAINT_INTERVAL : float = 0.1

# Camera settings
var min_zoom: float = 0.001
var max_zoom: float = 20.0
var zoom_factor: float = 0.1


func setup(p_world: GodotSimulation, p_camera: Camera2D, p_mesh_scale: Vector2) -> void:
	world = p_world
	camera = p_camera
	mesh_scale = p_mesh_scale

func _process(delta: float) -> void:
	if not is_input_enabled or not camera:
		return

	# Calculate mouse local position relative to camera & scale
	current_mouse_local = camera.get_global_mouse_position() / mesh_scale
	cursor_moved.emit(current_mouse_local)

	# Handle continuous paint interval
	if paint_cooldown > 0.0:
		paint_cooldown -= delta

	if (is_left_down or is_right_down) and paint_cooldown <= 0.0:
		_apply_brush()
		paint_cooldown = PAINT_INTERVAL

func _unhandled_input(event: InputEvent) -> void:
	if not is_input_enabled:
		return

	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_LEFT:
			is_left_down = event.pressed
		elif event.button_index == MOUSE_BUTTON_RIGHT:
			is_right_down = event.pressed

		if event.pressed and (is_left_down or is_right_down):
			_apply_brush()
			paint_cooldown = PAINT_INTERVAL

		elif event.button_index == MOUSE_BUTTON_WHEEL_UP:
			_zoom_camera(1.0 + zoom_factor)
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			_zoom_camera(1.0 - zoom_factor)

	elif event is InputEventKey and event.pressed:
		var cell_x := int(current_mouse_local.x)
		var cell_y := int(current_mouse_local.y)

		if event.keycode == KEY_A:
			_apply_fill(cell_x, cell_y, 2, false)
		elif event.keycode == KEY_S:
			_apply_fill(cell_x, cell_y, 2, true)
		elif event.keycode == KEY_1:
			brush_radius_cells = max(0.5, brush_radius_cells - 0.5)
			brush_radius_changed.emit(brush_radius_cells)
		elif event.keycode == KEY_2:
			brush_radius_cells += 0.5
			brush_radius_changed.emit(brush_radius_cells)

func _input(event: InputEvent) -> void:
	if not is_input_enabled or not camera:
		return

	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_MIDDLE:
		is_panning = event.pressed

	if event is InputEventMouseMotion and is_panning:
		camera.global_position -= event.relative / camera.zoom

func _apply_brush() -> void:
	if not world:
		return
	var cell_x := int(current_mouse_local.x)
	var cell_y := int(current_mouse_local.y)

	if is_left_down:
		world.setDensity(cell_x, cell_y, brush_radius_cells, 256, 2)
	elif is_right_down:
		world.setDensity(cell_x, cell_y, brush_radius_cells, -256, 2)

	terrain_modified.emit()

func _apply_fill(x: int, y: int, type_id: int, negative_fill: bool) -> void:
	if not world:
		return
	world.fillCell(x, y, type_id, negative_fill)
	terrain_modified.emit()

func _zoom_camera(factor: float) -> void:
	if not camera:
		return
	var old_zoom := camera.zoom
	var new_zoom := (old_zoom * factor).clamp(Vector2(min_zoom, min_zoom), Vector2(max_zoom, max_zoom))

	if old_zoom == new_zoom:
		return

	var mouse_world_before := camera.get_global_mouse_position()
	camera.zoom = new_zoom
	var mouse_world_after := camera.get_global_mouse_position()

	camera.position += (mouse_world_before - mouse_world_after)
