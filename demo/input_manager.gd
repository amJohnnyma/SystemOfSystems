class_name InputManager
extends Node

enum InputMode {
	FULL_GAMEPLAY,
	CAMERA_ONLY,
	UI_ONLY,
	BUILDING_PLACEMENT
}


const HOTBAR_ACTIONS: Array[StringName] = [
	&"hotbar_1", &"hotbar_2", &"hotbar_3", &"hotbar_4", &"hotbar_5",
	&"hotbar_6", &"hotbar_7", &"hotbar_8", &"hotbar_9", &"hotbar_10"
]

signal terrain_modified()
signal cursor_moved(local_cell_pos: Vector2)
signal brush_radius_changed(new_radius: float)
signal building_placed()
signal building_cancelled()
signal player_data_updated(data: Dictionary)

var current_mode: InputMode = InputMode.FULL_GAMEPLAY
var is_input_enabled: bool = true

var world: GodotSimulation
var camera: Camera2D
var mesh_scale: Vector2 = Vector2(4.0, 4.0)

var brush_radius_cells: float = 2.0
var current_mouse_local: Vector2 = Vector2.ZERO
var paint_cooldown: float = 0.0
const PAINT_INTERVAL: float = 0.1

var min_zoom: float = 0.001
var max_zoom: float = 20.0
var zoom_factor: float = 0.1
var selected_hotbar_slot: int = -1


func setup(p_world: GodotSimulation, p_camera: Camera2D, p_mesh_scale: Vector2) -> void:
	world = p_world
	camera = p_camera
	mesh_scale = p_mesh_scale


func getCurMouseX() -> float:
	return current_mouse_local.x


func getCurMouseY() -> float:
	return current_mouse_local.y


func setSelectedHotbarSlot(slot: int) -> void:
	selected_hotbar_slot = slot


func _process(delta: float) -> void:
	if not is_input_enabled or not camera:
		return

	current_mouse_local = camera.get_global_mouse_position() / mesh_scale
	cursor_moved.emit(current_mouse_local)
	paint_cooldown = maxf(0.0, paint_cooldown - delta)

	if current_mode == InputMode.FULL_GAMEPLAY and paint_cooldown <= 0.0:
		if _is_brush_action_held():
			_apply_brush()
			paint_cooldown = PAINT_INTERVAL


func _physics_process(delta: float) -> void:
	if not world:
		return

	var input_vector := Vector2.ZERO
	var jump_pressed := false
	var primary_attack := false
	var secondary_attack := false

	var gameplay_enabled := is_input_enabled and (
		current_mode == InputMode.FULL_GAMEPLAY
		or current_mode == InputMode.BUILDING_PLACEMENT
	)

	if gameplay_enabled: # AND selected item is a weapon
		input_vector = Input.get_vector("move_left", "move_right", "move_up", "move_down")
		jump_pressed = Input.is_action_just_pressed("jump")
		primary_attack = Input.is_action_just_pressed("primary_attack")
		secondary_attack = Input.is_action_just_pressed("secondary_attack")

	var input := {
		"deltaTime": delta,
		"moveInput": input_vector,
		"selectedHotbarSlot": -1,
		"hasJumpInput": jump_pressed,
		"hasMoveInput": true,
		"hasPrimaryAttackInput": primary_attack,
		"hasSecondaryAttackInput": secondary_attack,
		"swappedHotbar": false
	}

	world.processPlayerInputs(input)
	player_data_updated.emit(world.getPlayerData())


func _unhandled_input(event: InputEvent) -> void:
	if not is_input_enabled or current_mode == InputMode.UI_ONLY:
		return

	if event.is_action_pressed("camera_zoom_in"):
		_zoom_camera(1.0 + zoom_factor)
		return
	if event.is_action_pressed("camera_zoom_out"):
		_zoom_camera(1.0 - zoom_factor)
		return

	if current_mode == InputMode.BUILDING_PLACEMENT:
		if event.is_action_pressed("primary_attack") and not event.is_echo():
			_place_building()
		elif event.is_action_pressed("cancel_build") and not event.is_echo():
			_cancel_building()
		return

	if current_mode != InputMode.FULL_GAMEPLAY or event.is_echo():
		return

	if event.is_action_pressed("primary_attack") or event.is_action_pressed("secondary_attack"):
		if selected_hotbar_slot == 9:
			world.selectStructure(int(current_mouse_local.x), int(current_mouse_local.y))
		elif paint_cooldown <= 0.0 and _is_brush_action_held():
			_apply_brush()
			paint_cooldown = PAINT_INTERVAL
		return

	var cell_x := int(current_mouse_local.x)
	var cell_y := int(current_mouse_local.y)

	if event.is_action_pressed("brush_smaller"):
		brush_radius_cells = maxf(0.5, brush_radius_cells - 0.5)
		brush_radius_changed.emit(brush_radius_cells)
	elif event.is_action_pressed("brush_larger"):
		brush_radius_cells += 0.5
		brush_radius_changed.emit(brush_radius_cells)
	else:
		for i in range(HOTBAR_ACTIONS.size()):
			if event.is_action_pressed(HOTBAR_ACTIONS[i]):
				_trigger_hotbar_selection(i + 1)
				break


func _input(event: InputEvent) -> void:
	if not is_input_enabled or not camera or current_mode == InputMode.UI_ONLY:
		return

	# Panning works in FULL_GAMEPLAY, CAMERA_ONLY and BUILDING_PLACEMENT.
	if event is InputEventMouseMotion and Input.is_action_pressed("camera_pan"):
		camera.global_position -= event.relative / camera.zoom


func _is_brush_action_held() -> bool:
	return (
		(selected_hotbar_slot == 1 and Input.is_action_pressed("primary_attack"))
		or (selected_hotbar_slot == 2 and Input.is_action_pressed("secondary_attack"))
	)


func _trigger_hotbar_selection(slot_number: int) -> void:
	var hotbar = get_tree().root.find_child("Hotbar", true, false)
	if hotbar and hotbar.has_method("select_slot"):
		hotbar.select_slot(slot_number)


func _apply_brush() -> void:
	if not world:
		return
	var cell_x := int(current_mouse_local.x)
	var cell_y := int(current_mouse_local.y)
	if selected_hotbar_slot == 1 and Input.is_action_pressed("primary_attack"):
		world.setDensity(cell_x, cell_y, brush_radius_cells, 256, 2)
	elif selected_hotbar_slot == 2 and Input.is_action_pressed("secondary_attack"):
		world.setDensity(cell_x, cell_y, brush_radius_cells, -256, 2)
	else:
		return
	terrain_modified.emit()


func _place_building() -> void:
	if world:
		building_placed.emit()


func _cancel_building() -> void:
	if world:
		building_cancelled.emit()


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
	camera.position += mouse_world_before - mouse_world_after


func set_input_mode(mode: InputMode) -> void:
	current_mode = mode
	paint_cooldown = 0.0
