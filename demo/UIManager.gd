extends Node

signal hud_updated(data: Dictionary)
signal window_opened(win: Control)
signal window_closed()

var window_stack: Array[Control] = []

var window_container: Control
var hud_dock: Control

func register_ui_root(ui_root: Node) -> void:
	if not ui_root:
		push_error("UIManager: Passed ui_root is null.")
		return

	window_container = ui_root.find_child("WindowContainer", true, false) as Control
	hud_dock = ui_root.find_child("HUDDock", true, false) as Control

	if not window_container:
		push_error("UIManager: Could not find a node named 'WindowContainer' inside UIRoot.")
	if not hud_dock:
		push_warning("UIManager: Could not find a node named 'HUDDock' inside UIRoot.")

func connect_simulation(simulation: Node) -> void:
	if simulation and simulation.has_signal("test_signal_event"):
		simulation.test_signal_event.connect(_on_simulation_signal)

func open_window(window_scene: PackedScene) -> Control:
	if not window_container:
		push_error("UIManager: WindowContainer reference missing. Did you call UIManager.register_ui_root()?")
		return null

	var window_instance: Control = window_scene.instantiate() as Control
	window_instance.mouse_filter = Control.MOUSE_FILTER_STOP
	
	window_container.add_child(window_instance)
	window_stack.append(window_instance)
	
	_update_input_state()

	window_opened.emit(window_instance)
	return window_instance

func close_top_window() -> void:
	if window_stack.is_empty():
		return

	var top_win: Control = window_stack.pop_back()
	top_win.queue_free()

	_update_input_state()

	window_closed.emit()

func close_all_windows() -> void:
	while not window_stack.is_empty():
		close_top_window()

func _update_input_state() -> void:
	# Flexible lookup for InputManager (supports both Autoload or Scene Tree)
	var input_mgr = get_node_or_null("/root/InputManager")
	if not input_mgr:
		input_mgr = get_tree().root.find_child("InputManager", true, false)

	if not input_mgr:
		return

	if window_stack.is_empty():
		input_mgr.current_mode = input_mgr.InputMode.FULL_GAMEPLAY
	else:
		var top_window = window_stack.back()
		# Duck-typing check for property on BaseWindow subclasses
		if top_window and "required_input_mode" in top_window:
			input_mgr.current_mode = top_window.required_input_mode
		else:
			input_mgr.current_mode = input_mgr.InputMode.UI_ONLY

func _unhandled_input(event: InputEvent) -> void:
	if event.is_action_pressed("ui_cancel") and not window_stack.is_empty():
		close_top_window()
		get_viewport().set_input_as_handled()

func _on_simulation_signal(message: String) -> void:
	hud_updated.emit({"message": message})
