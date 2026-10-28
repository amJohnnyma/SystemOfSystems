extends Control

@export var required_input_mode = InputManager.InputMode.UI_ONLY
@export var slot_pixel_size: int = 46
@export var grid_columns: int = 9

@onready var close_button: Button = %CloseButton
@onready var item_grid: GridContainer = %ItemGrid
@onready var item_scroll: ScrollContainer = %ItemScroll

# Temporary item-definition browser. This is NOT the player's owned inventory.
# Each item ID appears once regardless of max_stack or player inventory contents.
var _registry_preview: Dictionary = {}
var _texture_cache: Dictionary = {}  # resource path -> Texture2D

func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_STOP

	if close_button:
		close_button.pressed.connect(
			func(): UIManager.close_top_window()
		)

	# Scroll configuration
	item_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	item_scroll.vertical_scroll_mode = ScrollContainer.SCROLL_MODE_AUTO
	item_scroll.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	item_scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL

	# Grid configuration
	item_grid.columns = max(1, grid_columns)
	item_grid.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	item_grid.size_flags_vertical = Control.SIZE_SHRINK_BEGIN

	if not _registry_preview.is_empty():
		_render_registry()
	else:
		call_deferred("_load_registry_preview")

# Public entry point. Later this can be changed to actual inventory slots.
# The current data is a Dictionary keyed by item ID from getItemRegistry().
func setup(inventory_data: Dictionary) -> void:
	_registry_preview = inventory_data
	if is_node_ready():
		_render_registry()

func _load_registry_preview() -> void:
	if not _registry_preview.is_empty():
		return
	var scene := get_tree().current_scene
	if scene == null:
		return

	var simulation: Node = scene.find_child("GodotSimulation", true, false)
	if simulation != null and simulation.has_method("getItemRegistry"):
		setup(simulation.getItemRegistry())
	else:
		push_warning("Inventory preview couldn't find initialized GodotSimulation")

func _render_registry() -> void:
	_clear_grid()
	item_grid.columns = max(1, grid_columns)

	var ids: Array = _registry_preview.keys()
	ids.sort()  # unordered_map -> deterministic display order

	for raw_id in ids:
		var item_id: int = int(raw_id)
		var item: Dictionary = _registry_preview[raw_id]
		if item_id == 100001:
			print("BLOCK ITEM DATA: ", item)
			print("SPRITE SHEET: ", item.get("sprite_sheet", {}))
			print("TERRAIN EXISTS: ",
				ResourceLoader.exists("res://game/atlas/terrain.png"))
		var button := Button.new()
		button.custom_minimum_size = Vector2(slot_pixel_size, slot_pixel_size)
		button.name = "Item_%d" % item_id
		button.focus_mode = Control.FOCUS_NONE
		button.expand_icon = true
		button.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
		button.icon_alignment = HORIZONTAL_ALIGNMENT_CENTER
		button.tooltip_text = "%s\nItem ID: %d\n%s" % [
			str(item.get("name", "Unknown")),
			item_id,
			str(item.get("category", ""))
		]

		var icon_texture: Texture2D = _make_item_icon(item)
		if icon_texture != null:
			button.icon = icon_texture
		else:
			# Unknown/missing art still produces a visible, clickable item.
			button.text = str(item_id)

		button.pressed.connect(_on_item_press.bind(item_id))
		item_grid.add_child(button)

	print("Inventory registry preview: ", ids.size(), " item definitions")
	call_deferred("_debug_scroll")

func _debug_scroll() -> void:
	await get_tree().process_frame

	var bar := item_scroll.get_v_scroll_bar()

	print("Scroll viewport height: ", item_scroll.size.y)
	print("Grid content height: ", item_grid.size.y)
	print("Scroll range: ", bar.max_value - bar.page)
func _make_item_icon(item: Dictionary) -> Texture2D:
	# A single PNG is preferred for standalone building sprites or overrides.
	var standalone_path: String = str(item.get("icon", ""))
	if not standalone_path.is_empty():
		# Some older definitions used relative paths.
		if not standalone_path.begins_with("res://"):
			standalone_path = "res://game/sprite/" + standalone_path
		var standalone: Texture2D = _get_texture(standalone_path)
		if standalone != null:
			return standalone

	# AtlasTexture is a static inventory icon, not an animated sprite.
	var sheet: Dictionary = item.get("sprite_sheet", {})


	var path: String = str(sheet.get("path", ""))
	if path.is_empty():
		return null

	var atlas: Texture2D = _get_texture(path)
	if atlas == null:
		return null

	var fw: int = maxi(1, int(sheet.get("frame_width", 16)))
	var fh: int = maxi(1, int(sheet.get("frame_height", 16)))
	var columns: int = int(atlas.get_width() / fw)
	var rows: int = int(atlas.get_height() / fh)
	var frame: int = int(sheet.get("icon_frame", 0))
	if columns <= 0 or rows <= 0 or frame < 0 or frame >= columns * rows:
		return null

	var icon := AtlasTexture.new()
	icon.atlas = atlas
	icon.region = Rect2(
		(frame % columns) * fw,
		int(frame / columns) * fh,
		fw,
		fh
	)
	icon.filter_clip = true
	return icon

func _get_texture(path: String) -> Texture2D:
	if _texture_cache.has(path):
		return _texture_cache[path]
	var texture: Texture2D = null
	if ResourceLoader.exists(path):
		texture = load(path) as Texture2D
	_texture_cache[path] = texture
	return texture

func _clear_grid() -> void:
	for child in item_grid.get_children():
		item_grid.remove_child(child)
		child.queue_free()

func _on_item_press(item_id: int) -> void:
	var item: Dictionary = _registry_preview.get(item_id, {})
	print("Preview item: ", item_id, " ", item.get("name", ""),
		  " structure_id=", item.get("structure_id", 0),
		  " actions=", item.get("actions", []))
	# Intentionally do NOT emit UIManager.slot_action_requested here.
	# That signal currently selects structures/hotbar actions, while these
	# are ItemRegistry IDs (generated structure items start at 100000).
