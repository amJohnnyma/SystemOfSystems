extends Control

@export var required_input_mode = InputManager.InputMode.UI_ONLY

@onready var close_button:Button = %CloseButton
@onready var item_grid: GridContainer = %ItemGrid

# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_STOP
	if close_button:
		close_button.pressed.connect(func(): UIManager.close_top_window())
		
	_create_placeholder_slots(60)


func setup(inventory_data: Dictionary) -> void:
	_clear_grid()
	for item_data in inventory_data:
		var slot = PanelContainer.new()
		slot.custom_minimum_size = Vector2(30,30)
		# Add the necessary data
		item_grid.add_child(slot)
	

func _create_placeholder_slots(count: int):
	_clear_grid()
	for i in range(count):
		var slot = PanelContainer.new()
		slot.custom_minimum_size = Vector2(30, 30)
		
		var label = Label.new()
		label.text = str(i + 1)
		label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
		slot.add_child(label)
		
		item_grid.add_child(slot)
		
	
func _clear_grid():
	for child in item_grid.get_children():
		child.queue_free()
