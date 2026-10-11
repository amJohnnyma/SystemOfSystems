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
		var btn = Button.new()
		btn.text = str(i + 1)
		btn.custom_minimum_size = Vector2(30, 30)

		# Capture the index and print it when clicked
		var slot_number = i + 1
		btn.pressed.connect(_on_button_press.bind(slot_number))

		item_grid.add_child(btn)
		
	
func _clear_grid():
	for child in item_grid.get_children():
		child.queue_free()

func _add_to_inventory():
	pass

func _on_button_press(slot_number):
	print("Button pressed: ", slot_number)
	UIManager.slot_action_requested.emit(slot_number)
