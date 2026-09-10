class_name BaseWindow
extends PanelContainer

# Change this to alter which inputs are suppressed
@export var required_input_mode: InputManager.InputMode = InputManager.InputMode.UI_ONLY
@export var close_button: Button

func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_STOP
	if close_button:
		close_button.pressed.connect(func(): UIManager.close_top_window())

func _on_close_pressed() -> void:
	var ui_mgr = get_node_or_null("/root/UIManager")
	if ui_mgr:
		ui_mgr.close_top_window()
	else:
		queue_free()
