extends Control

var player_size := Vector2(2, 3)

var pixels_per_cell := 60.0
var player_origin := Vector2.ZERO

var current_frame: int = 0
var frame_count: int = 8

# Frame number -> Array of hitboxes
var hitboxes: Dictionary = {}

var selected_index: int = -1

var dragging := false
var drawing := false

var drag_start := Vector2.ZERO
var original_offset := Vector2.ZERO

var draw_start := Vector2.ZERO
var draw_end := Vector2.ZERO

var snap_size := 0.25

func world_to_screen(point: Vector2) -> Vector2:
	return player_origin + Vector2(
		point.x,
		-point.y
	) * pixels_per_cell


func screen_to_world(point: Vector2) -> Vector2:
	var local := (
		point - player_origin
	) / pixels_per_cell

	return Vector2(local.x, -local.y)


func snap_point(point: Vector2) -> Vector2:
	return point.snapped(Vector2.ONE * snap_size)
	
func _draw_grid() -> void:
	if pixels_per_cell <= 0.0:
		return

	var grid_color := Color(1, 1, 1, 0.12)

	var x := fposmod(
		player_origin.x, pixels_per_cell
	)

	while x < size.x:
		draw_line(
			Vector2(x, 0),
			Vector2(x, size.y),
			grid_color
		)

		x += pixels_per_cell

	var y := fposmod(
		player_origin.y, pixels_per_cell
	)

	while y < size.y:
		draw_line(
			Vector2(0, y),
			Vector2(size.x, y),
			grid_color
		)

		y += pixels_per_cell


func _draw_player_bounds() -> void:
	var top_left := world_to_screen(
		Vector2(0, player_size.y)
	)

	var rect := Rect2(
		top_left,
		player_size * pixels_per_cell
	)

	draw_rect(
		rect,
		Color(0, 1, 0, 0.08),
		true
	)

	draw_rect(
		rect,
		Color(0, 1, 0, 0.7),
		false,
		2.0
	)

	draw_circle(
		player_origin,
		4.0,
		Color.GREEN
	)
	
func _draw_hitbox(
	box: Dictionary,
	selected: bool = false
) -> void:

	var box_size: Vector2 = box["size"]
	var offset: Vector2 = box["offset_r"]

	var center := world_to_screen(
		offset + box_size * 0.5
	)

	var angle := deg_to_rad(
		float(box.get("angle_deg", 0.0))
	)

	var dimensions := box_size * pixels_per_cell

	draw_set_transform(
		center,
		angle,
		Vector2.ONE
	)

	var rect := Rect2(
		-dimensions * 0.5,
		dimensions
	)

	draw_rect(
		rect,
		Color(1, 0, 0, 0.25),
		true
	)

	draw_rect(
		rect,
		Color.YELLOW if selected else Color.RED,
		false,
		2.0
	)

	draw_circle(
		Vector2.ZERO,
		3.0,
		Color.WHITE
	)

	draw_set_transform(Vector2.ZERO)
	
func _draw() -> void:
	_draw_grid()
	_draw_player_bounds()
	_draw_previous_hitboxes()

	var boxes: Array = hitboxes.get(current_frame, [])


	for i in range(boxes.size()):
		_draw_hitbox(
			boxes[i],
			i == selected_index
		)

	if drawing:
		var rect := Rect2(
			draw_start,
			draw_end - draw_start
		).abs()

		draw_rect(
			rect,
			Color(1, 0, 0, 0.2),
			true
		)

		draw_rect(
			rect,
			Color.YELLOW,
			false,
			2.0
		)
		
func _find_hitbox(point: Vector2) -> int:
	var boxes: Array = hitboxes.get(current_frame, [])

	for i in range(boxes.size() - 1, -1, -1):
		var box: Dictionary = boxes[i]

		var box_size: Vector2 = box["size"]
		var offset: Vector2 = box["offset_r"]

		var center := world_to_screen(
			offset + box_size * 0.5
		)

		var local_point := (
			point - center
		).rotated(
			-deg_to_rad(float(box["angle_deg"]))
		)

		var dimensions := (
			box_size * pixels_per_cell
		)

		var rect := Rect2(
			-dimensions * 0.5,
			dimensions
		)

		if rect.has_point(local_point):
			return i

	return -1
	
func _add_hitbox(
	start: Vector2,
	end: Vector2
) -> void:

	var a := snap_point(screen_to_world(start))
	var b := snap_point(screen_to_world(end))

	var offset := Vector2(
		minf(a.x, b.x),
		minf(a.y, b.y)
	)

	var box_size := (b - a).abs()

	if box_size.x < snap_size:
		return

	if box_size.y < snap_size:
		return

	if not hitboxes.has(current_frame):
		hitboxes[current_frame] = []

	hitboxes[current_frame].append({
		"size": box_size,
		"offset_r": offset,
		"angle_deg": 0.0
	})

	selected_index = hitboxes[current_frame].size() - 1

	queue_redraw()
	
func _gui_input(event: InputEvent) -> void:
	if event is InputEventMouseButton:

		if event.button_index == MOUSE_BUTTON_LEFT:

			if event.pressed:
				selected_index = _find_hitbox(
					event.position
				)

				if selected_index >= 0:
					dragging = true
					drag_start = event.position

					original_offset = hitboxes[
						current_frame
					][selected_index]["offset_r"]

				else:
					drawing = true
					draw_start = event.position
					draw_end = event.position

			else:
				if drawing:
					_add_hitbox(
						draw_start,
						event.position
					)

				drawing = false
				dragging = false

			accept_event()

		elif event.button_index == MOUSE_BUTTON_RIGHT:
			if event.pressed:
				var index := _find_hitbox(event.position)

				if index >= 0:
					hitboxes[current_frame].remove_at(index)

					if hitboxes[current_frame].is_empty():
						hitboxes.erase(current_frame)

					selected_index = -1
					dragging = false

				accept_event()

		elif event.pressed and selected_index >= 0:
			var direction := 0.0

			if event.button_index == MOUSE_BUTTON_WHEEL_UP:
				direction = 1.0

			elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
				direction = -1.0

			if direction != 0.0:
				var box: Dictionary = hitboxes[
					current_frame
				][selected_index]

				if event.shift_pressed:
					var old_size: Vector2 = box["size"]

					var new_size := (
						old_size +
						Vector2.ONE * direction * snap_size
					).max(Vector2.ONE * snap_size)

					# Preserve the centre while resizing.
					box["offset_r"] += (
						old_size - new_size
					) * 0.5

					box["size"] = new_size

				else:
					box["angle_deg"] += direction * 5.0

				accept_event()

	elif event is InputEventMouseMotion:
		if drawing:
			draw_end = event.position

		elif dragging and selected_index >= 0:
			var movement := (
				screen_to_world(event.position) -
				screen_to_world(drag_start)
			)

			hitboxes[current_frame][selected_index][
                "offset_r"
			] = snap_point(original_offset + movement)

	queue_redraw()
	
func load_hitboxes(
	source: Array,
	count: int
) -> void:

	hitboxes.clear()

	frame_count = maxi(1, count)
	current_frame = 0
	selected_index = -1
	dragging = false
	drawing = false

	for box in source:
		var frame := int(box["frame"])

		if frame < 0 or frame >= frame_count:
			continue

		if not hitboxes.has(frame):
			hitboxes[frame] = []

		hitboxes[frame].append({
			"size": Vector2(
				box["size"][0],
				box["size"][1]
			),
			"offset_r": Vector2(
				box["offset_r"][0],
				box["offset_r"][1]
			),
			"angle_deg": float(
				box.get("angle_deg", 0.0)
			)
		})

	queue_redraw()
	
func export_hitboxes() -> Array:
	var result: Array = []

	for frame in range(frame_count):
		for box in hitboxes.get(frame, []):
			var s: Vector2 = box["size"]
			var offset: Vector2 = box["offset_r"]

			result.append({
				"frame": frame,
				"size": [s.x, s.y],
				"offset_r": [offset.x, offset.y],
				"angle_deg": box["angle_deg"]
			})

	return result
	
func duplicate_to_next_frame() -> bool:
	var next_frame := current_frame + 1

	if next_frame >= frame_count:
		return false

	if not hitboxes.has(current_frame):
		return false

	# Avoid accidentally overwriting existing hitboxes.
	if not hitboxes.get(next_frame, []).is_empty():
		return false

	hitboxes[next_frame] = hitboxes[
		current_frame
	].duplicate(true)

	return true
	
func _draw_previous_hitboxes() -> void:
	if current_frame <= 0:
		return

	for box in hitboxes.get(current_frame - 1, []):
		var box_size: Vector2 = box["size"]
		var offset: Vector2 = box["offset_r"]

		var center := world_to_screen(
			offset + box_size * 0.5
		)

		var dimensions := box_size * pixels_per_cell

		draw_set_transform(
			center,
			deg_to_rad(float(box["angle_deg"]))
		)

		draw_rect(
			Rect2(-dimensions * 0.5, dimensions),
			Color(0.3, 0.6, 1.0, 0.35),
			false,
			1.0
		)

		draw_set_transform(Vector2.ZERO)
		
func interpolate_frames(
	start_frame: int,
	end_frame: int
) -> bool:

	if start_frame < 0 or end_frame >= frame_count:
		return false

	if end_frame <= start_frame + 1:
		return false

	if not hitboxes.has(start_frame):
		return false

	if not hitboxes.has(end_frame):
		return false

	var start_boxes: Array = hitboxes[start_frame]
	var end_boxes: Array = hitboxes[end_frame]

	# Only interpolate matching numbers of hitboxes.
	if start_boxes.size() != end_boxes.size():
		return false

	for frame in range(start_frame + 1, end_frame):
		var t := float(
			frame - start_frame
		) / float(end_frame - start_frame)

		var result: Array = []

		for i in range(start_boxes.size()):
			var a: Dictionary = start_boxes[i]
			var b: Dictionary = end_boxes[i]

			var start_size: Vector2 = a["size"]
			var end_size: Vector2 = b["size"]

			var start_offset: Vector2 = a["offset_r"]
			var end_offset: Vector2 = b["offset_r"]

			result.append({
				"size": start_size.lerp(end_size, t),

				"offset_r": start_offset.lerp(
					end_offset, t
				),

				"angle_deg": rad_to_deg(
					lerp_angle(
						deg_to_rad(a["angle_deg"]),
						deg_to_rad(b["angle_deg"]),
						t
					)
				)
			})

		# Preserve any manually authored frames.
		if not hitboxes.get(frame, []).is_empty():
			continue

		hitboxes[frame] = result

	queue_redraw()

	return true
