extends Control

const RIGHT_HAND_BONE := "mixamorig1_RightHand"
const ITEMS_PATH := "res://game/items.json"
const PLAYER_PATH := "res://game/player.json"

@onready var model_root: Node3D = %ModelRoot
@onready var model_camera: Camera3D = %Camera3D
@onready var model_viewport: SubViewport = %ModelViewport

@onready var player_skeleton: Skeleton3D = \
	model_root.get_node("Idle/Skeleton3D")

@onready var player_animation: AnimationPlayer = \
	model_root.get_node("Idle/AnimationPlayer")

@onready var weapon_select: OptionButton = %WeaponSelect
@onready var attack_select: OptionButton = %AttackSelect
@onready var frame_slider: HSlider = %FrameSlider
@onready var frame_label: Label = %FrameLabel

@onready var overlay = %HitboxOverlay

var hand_socket: BoneAttachment3D
var equipped_weapon_model: Node3D

var model_height_3d := 0.0
var player_size := Vector2(2.0, 3.0)

var items: Array[Dictionary] = []
var attacks: Array[Dictionary] = []

var selected_item: Dictionary = {}
var selected_action: Dictionary = {}

var current_frame: int = 0
var frame_count: int = 8

var attack_clip: StringName = &""
var attack_duration: float = 1.0

var preview_playing := false
var preview_elapsed := 0.0

@export var preview_speed := 0.5

var attack_clips: Dictionary = {}

func _ready() -> void:
	var player_data = _read_json(PLAYER_PATH)

	if player_data is Dictionary:
		var s: Array = player_data.get("size", [2, 3])

		player_size = Vector2(
			float(s[0]),
			float(s[1])
		)
	
	%Interpolate.pressed.connect(_interpolate_hitboxes)
	if not player_animation.has_animation_library(&"editor"):
		player_animation.add_animation_library(
			&"editor", AnimationLibrary.new()
		)
	_configure_camera()
	_create_hand_socket()
	_load_items()

	weapon_select.item_selected.connect(
		_on_weapon_selected
	)

	attack_select.item_selected.connect(
		_on_attack_selected
	)

	frame_slider.value_changed.connect(
		_on_frame_slider_changed
	)

	%FrameCount.value_changed.connect(
		_on_frame_count_changed
	)

	%Previous.pressed.connect(
		_on_previous_pressed
	)

	%Next.pressed.connect(
		_on_next_pressed
	)

	%DuplicateNext.pressed.connect(
		_duplicate_next
	)

	%CopyJSON.pressed.connect(
		_copy_hitbox_json
	)

	%CopyWeaponJSON.pressed.connect(
		_copy_weapon_json
	)

	%PlayPreview.pressed.connect(
		_toggle_preview
	)

	%SideView.pressed.connect(
		func(): _set_preview_view("side")
	)

	%FrontView.pressed.connect(
		func(): _set_preview_view("front")
	)

	%TopView.pressed.connect(
		func(): _set_preview_view("top")
	)

	for control in [
		%PositionX,
		%PositionY,
		%PositionZ,
		%RotationX,
		%RotationY,
		%RotationZ,
		%WeaponScale
	]:
		control.value_changed.connect(
			_update_weapon_transform
		)

	overlay.resized.connect(_sync_overlay)

	await get_tree().process_frame

	_sync_overlay()

	if not items.is_empty():
		weapon_select.select(0)
		_on_weapon_selected(0)

func _unhandled_key_input(
	event: InputEvent
) -> void:

	if not event is InputEventKey:
		return

	if not event.pressed:
		return

	match event.keycode:
		KEY_RIGHT:
			_set_frame(current_frame + 1)

		KEY_LEFT:
			_set_frame(current_frame - 1)

		KEY_D:
			_duplicate_next()

		KEY_SPACE:
			_toggle_preview()

		_:
			return

	get_viewport().set_input_as_handled()

func _process(delta: float) -> void:
	if not preview_playing:
		return
		
	if attack_clip == &"" or not player_animation.has_animation(attack_clip):
		_stop_preview()
		return

	preview_elapsed += delta * preview_speed

	var phase := clampf(
		preview_elapsed / maxf(attack_duration, 0.001),
		0.0,
		1.0
	)

	if phase >= 1.0:
		preview_elapsed = 0.0
		phase = 0.0

	var animation := player_animation.get_animation(
		attack_clip
	)

	player_animation.seek(
		phase * animation.length,
		true,
		true
	)

	current_frame = clampi(
		int(phase * frame_count),
		0,
		frame_count - 1
	)

	frame_slider.set_value_no_signal(current_frame)

	frame_label.text = "Frame %d / %d | %.3fs" % [
		current_frame,
		frame_count - 1,
		phase * attack_duration
	]

	overlay.current_frame = current_frame
	overlay.queue_redraw()

func _toggle_preview() -> void:
	if attack_clip == &"":
		return

	preview_playing = not preview_playing

	if preview_playing:
		preview_elapsed = 0.0
		overlay.mouse_filter = Control.MOUSE_FILTER_IGNORE
	else:
		overlay.mouse_filter = Control.MOUSE_FILTER_STOP
		_set_frame(current_frame)

func _create_hand_socket() -> void:
	var bone_index := player_skeleton.find_bone(
		RIGHT_HAND_BONE
	)

	if bone_index == -1:
		push_error("Right hand bone not found")
		return

	hand_socket = BoneAttachment3D.new()
	hand_socket.name = "RightHandSocket"

	player_skeleton.add_child(hand_socket)
	hand_socket.bone_idx = bone_index
	
func _configure_camera() -> void:
	var min_corner := Vector3(INF, INF, INF)
	var max_corner := Vector3(-INF, -INF, -INF)
	var found := false

	for node in model_root.find_children(
		"*", "MeshInstance3D", true, false
	):
		var mesh := node as MeshInstance3D

		if mesh.mesh == null:
			continue

		for corner in range(8):
			var point := mesh.to_global(
				mesh.get_aabb().get_endpoint(corner)
			)

			min_corner = min_corner.min(point)
			max_corner = max_corner.max(point)

		found = true

	if not found:
		push_error("No character mesh found")
		return

	var body_size := max_corner - min_corner
	var center := (min_corner + max_corner) * 0.5

	model_height_3d = body_size.y

	model_camera.projection = Camera3D.PROJECTION_ORTHOGONAL
	model_camera.keep_aspect = Camera3D.KEEP_HEIGHT

	# Leave plenty of space for attack animations.
	model_camera.size = model_height_3d * 3.0

	model_camera.position = center + Vector3(
		0.0, 0.0, model_height_3d * 5.0
	)

	model_camera.look_at(center, Vector3.UP)
	model_camera.make_current()

	# Match the game's right-facing attack orientation.
	model_root.rotation_degrees.y = 90.0
	
func _read_json(path: String) -> Variant:
	if not FileAccess.file_exists(path):
		push_error("Missing file: " + path)
		return null

	var parser := JSON.new()
	var error := parser.parse(
		FileAccess.get_file_as_string(path)
	)

	if error != OK:
		push_error(
			"%s: line %d: %s" % [
				path,
				parser.get_error_line(),
				parser.get_error_message()
			]
		)
		return null

	return parser.data
	
func _load_items() -> void:
	var document = _read_json(ITEMS_PATH)

	if document == null:
		return

	var source_items: Array = []

	if document is Dictionary:
		source_items = document.get("items", [])
	elif document is Array:
		source_items = document

	items.clear()
	weapon_select.clear()

	for item in source_items:
		if not item is Dictionary:
			continue

		if item.get("category", "") != "Weapon":
			continue

		if not item.has("weapon"):
			continue

		items.append(item)

		weapon_select.add_item(
			"%s (%d)" % [
				item.get("name", "Unknown"),
				int(item.get("iID", 0))
			]
		)
		
func _on_weapon_selected(index: int) -> void:
	
	print("Weapon selected: ", index)
	preview_playing = false
	preview_elapsed = 0.0

	player_animation.stop()
	attack_clip = &""

	_store_current_edits()
	_stop_preview()

	selected_action = {}
	selected_item = items[index]

	_load_weapon_model()
	_sync_weapon_controls()
	
	attacks.clear()
	attack_select.clear()


	for action in selected_item.get("actions", []):
		if action.get("type", "") != "Attack":
			continue

		attacks.append(action)

		attack_select.add_item(
			str(action.get("animation", "Attack"))
		)

	if not attacks.is_empty():
		attack_select.select(0)
		_on_attack_selected(0)
		
func _array_to_vector3(
	values: Array
) -> Vector3:
	return Vector3(
		float(values[0]),
		float(values[1]),
		float(values[2])
	)
	
func _load_weapon_model() -> void:
	if equipped_weapon_model != null:
		hand_socket.remove_child(equipped_weapon_model)
		equipped_weapon_model.queue_free()
		equipped_weapon_model = null

	if hand_socket == null:
		push_error("Hand socket missing")
		return

	var weapon: Dictionary = selected_item["weapon"]
	var path: String = weapon.get("model", "")

	if path.is_empty():
		return

	var scene := load(path) as PackedScene

	if scene == null:
		push_error("Cannot load weapon: " + path)
		return

	equipped_weapon_model = scene.instantiate() as Node3D

	if equipped_weapon_model == null:
		push_error("Weapon root must be Node3D")
		return

	hand_socket.add_child(equipped_weapon_model)

	equipped_weapon_model.position = _array_to_vector3(
		weapon.get("position_offset", [0, 0, 0])
	)

	equipped_weapon_model.rotation_degrees = _array_to_vector3(
		weapon.get("rotation_offset", [0, 0, 0])
	)

	equipped_weapon_model.scale = Vector3.ONE * float(
		weapon.get("scale", 1.0)
	)

func _load_attack_animation(
	source: String,
	clip_name: String
) -> bool:

	var cache_key := source + "|" + clip_name

	# Reuse previously loaded animations.
	if attack_clips.has(cache_key):
		attack_clip = attack_clips[cache_key]

	else:
		var scene := load(source) as PackedScene

		if scene == null:
			push_error("Cannot load animation: " + source)
			return false

		var instance := scene.instantiate()

		var source_player := instance.find_child(
			"AnimationPlayer", true, false
		) as AnimationPlayer

		if source_player == null:
			instance.free()
			return false

		if not source_player.has_animation(clip_name):
			instance.free()
			return false

		var animation := source_player.get_animation(
			clip_name
		).duplicate(true) as Animation

		# Remove root movement.
		for track in range(
			animation.get_track_count() - 1, -1, -1
		):
			if animation.track_get_type(track) != Animation.TYPE_POSITION_3D:
				continue

			var path := str(
				animation.track_get_path(track)
			).to_lower()

			if path.contains("hips") or path.contains("root"):
				animation.remove_track(track)

		instance.free()

		var library := player_animation.get_animation_library(
			&"editor"
		)

		var local_name := StringName(
			"attack_%d" % attack_clips.size()
		)

		if library.add_animation(local_name, animation) != OK:
			push_error("Failed to register animation: " + source)
			return false

		attack_clip = StringName("editor/" + str(local_name))
		attack_clips[cache_key] = attack_clip

	# Verify that the animation exists before playing.
	if not player_animation.has_animation(attack_clip):
		push_error("Animation not found: " + str(attack_clip))
		return false

	player_animation.stop()
	player_skeleton.show_rest_only = false

	player_animation.play(attack_clip)
	player_animation.advance(0.0)
	player_animation.pause()

	return true
	
func _on_attack_selected(index: int) -> void:
	print("Attack selected: ", index)
	_store_current_edits()
	_stop_preview()
	selected_action = attacks[index]

	var animations: Dictionary = selected_item.get(
		"animations", {}
	)

	var animation: Dictionary = animations.get(
		selected_action.get("animation", ""), {}
	)

	attack_duration = float(
		animation.get("duration", 1.0)
	)

	frame_count = maxi(
		1,
		int(selected_action.get("hitbox_frame_count", 8))
	)
	%FrameCount.set_value_no_signal(frame_count)

	attack_clip = &""

	if not _load_attack_animation(
		animation.get("source", ""),
		animation.get("clip", "")
	):
		return

	frame_slider.min_value = 0
	frame_slider.max_value = frame_count - 1
	frame_slider.step = 1

	current_frame = 0
	frame_slider.set_value_no_signal(0)

	overlay.load_hitboxes(
		selected_action.get("hitbox", []),
		frame_count
	)

	_update_preview()
	
func _update_preview() -> void:
	if attack_clip == &"":
		return

	var phase := float(current_frame) / float(frame_count)

	var animation := player_animation.get_animation(
		attack_clip
	)

	player_animation.seek(
		phase * animation.length,
		true,
		true
	)

	frame_label.text = "Frame %d / %d | %.3fs" % [
		current_frame,
		frame_count - 1,
		phase * attack_duration
	]

	overlay.current_frame = current_frame
	overlay.queue_redraw()
	
func _set_frame(frame: int) -> void:
	preview_playing = false

	current_frame = clampi(
		frame, 0, frame_count - 1
	)

	overlay.mouse_filter = Control.MOUSE_FILTER_STOP
	overlay.selected_index = -1
	overlay.dragging = false
	overlay.drawing = false

	frame_slider.set_value_no_signal(current_frame)

	_update_preview()


func _on_previous_pressed() -> void:
	_set_frame(current_frame - 1)


func _on_next_pressed() -> void:
	_set_frame(current_frame + 1)


func _on_frame_slider_changed(value: float) -> void:
	_set_frame(int(value))
	
func _sync_overlay() -> void:
	if model_height_3d <= 0.0:
		return

	var pixels_per_3d_unit : float = (
		overlay.size.y / model_camera.size
	)

	var player_height_px := (
		model_height_3d * pixels_per_3d_unit
	)

	overlay.player_size = player_size

	overlay.pixels_per_cell = (
		player_height_px / player_size.y
	)

	overlay.player_origin = Vector2(
		overlay.size.x * 0.5 -
			player_size.x * overlay.pixels_per_cell * 0.5,

		overlay.size.y * 0.5 +
			player_height_px * 0.5
	)

	overlay.queue_redraw()
	
func _copy_hitbox_json() -> void:
	var result := {
		"hitbox_frame_count": frame_count,
		"hitbox": overlay.export_hitboxes()
	}

	var json := JSON.stringify(result, "\t")

	DisplayServer.clipboard_set(json)

	print(json)


func _duplicate_next() -> void:
	if overlay.duplicate_to_next_frame():
		_set_frame(current_frame + 1)
		
func _on_frame_count_changed(value: float) -> void:
	var new_count := maxi(1, int(value))

	# Avoid changing the timing of existing hitboxes
	# without an explicit decision.
	if not overlay.hitboxes.is_empty():
		%FrameCount.set_value_no_signal(frame_count)

		push_warning(
            "Clear existing hitboxes before changing frame count"
		)

		return

	frame_count = new_count
	overlay.frame_count = new_count

	frame_slider.max_value = frame_count - 1

	_set_frame(current_frame)
	
func _update_weapon_transform(
	_value: float = 0.0
) -> void:

	if equipped_weapon_model == null:
		return

	equipped_weapon_model.position = Vector3(
		%PositionX.value,
		%PositionY.value,
		%PositionZ.value
	)

	equipped_weapon_model.rotation_degrees = Vector3(
		%RotationX.value,
		%RotationY.value,
		%RotationZ.value
	)

	equipped_weapon_model.scale = Vector3.ONE * \
		%WeaponScale.value
		
func _sync_weapon_controls() -> void:
	if equipped_weapon_model == null:
		return

	var pos := equipped_weapon_model.position
	var rot := equipped_weapon_model.rotation_degrees

	%PositionX.set_value_no_signal(pos.x)
	%PositionY.set_value_no_signal(pos.y)
	%PositionZ.set_value_no_signal(pos.z)

	%RotationX.set_value_no_signal(rot.x)
	%RotationY.set_value_no_signal(rot.y)
	%RotationZ.set_value_no_signal(rot.z)

	%WeaponScale.set_value_no_signal(
		equipped_weapon_model.scale.x
	)


func _set_preview_view(view: String) -> void:
	match view:
		"side":
			model_root.rotation_degrees = Vector3(
				0, 90, 0
			)

		"front":
			model_root.rotation_degrees = Vector3.ZERO

		"top":
			model_root.rotation_degrees = Vector3(
				90, 0, 0
			)

	overlay.visible = view == "side"
	
func _copy_weapon_json() -> void:
	if equipped_weapon_model == null:
		return

	var pos := equipped_weapon_model.position
	var rot := equipped_weapon_model.rotation_degrees

	var result := {
		"weapon": {
			"model": selected_item["weapon"]["model"],

			"position_offset": [
				pos.x, pos.y, pos.z
			],

			"rotation_offset": [
				rot.x, rot.y, rot.z
			],

			"scale": equipped_weapon_model.scale.x
		}
	}

	DisplayServer.clipboard_set(
		JSON.stringify(result, "\t")
	)
	
func _stop_preview() -> void:
	preview_playing = false
	preview_elapsed = 0.0

	overlay.mouse_filter = Control.MOUSE_FILTER_STOP

	overlay.selected_index = -1
	overlay.dragging = false
	overlay.drawing = false
	
func _interpolate_hitboxes() -> void:
	var start_frame := int(%InterpolationStart.value)
	var end_frame := int(%InterpolationEnd.value)

	if not overlay.interpolate_frames(
		start_frame,
		end_frame
	):
		push_warning("Cannot interpolate selected frames")
		return

	_set_frame(start_frame + 1)

func _store_current_edits() -> void:
	if not selected_action.is_empty():
		selected_action["hitbox_frame_count"] = frame_count
		selected_action["hitbox"] = overlay.export_hitboxes()

	if not selected_item.is_empty() and equipped_weapon_model != null:
		var pos := equipped_weapon_model.position
		var rot := equipped_weapon_model.rotation_degrees

		var weapon: Dictionary = selected_item["weapon"]

		weapon["position_offset"] = [pos.x, pos.y, pos.z]
		weapon["rotation_offset"] = [rot.x, rot.y, rot.z]
		weapon["scale"] = equipped_weapon_model.scale.x
