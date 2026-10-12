extends MarginContainer

@onready var hotbar_grid: HBoxContainer = %HotbarGrid

# Signal to notify Main script when a slot is selected
signal hotbar_slot_selected(slot_number: int)

var current_selected_slot: int = -1

func _ready() -> void:
	_create_hotbar_slots(10)

func _create_hotbar_slots(count: int) -> void:
	for child in hotbar_grid.get_children():
		child.queue_free()

	for i in range(count):
		var btn = Button.new()
		var display_num = (i + 1) if (i + 1) < 10 else 0
		btn.text = str(display_num)
		btn.custom_minimum_size = Vector2(40, 40)
		
		btn.toggle_mode = true
		btn.button_group = ButtonGroup.new() # Ensures only one button is pressed at a time
		
		var slot_number = i + 1
		btn.pressed.connect(func(): select_slot(slot_number))

		hotbar_grid.add_child(btn)

func select_slot(slot_number: int) -> void:
	current_selected_slot = slot_number
	
	var buttons = hotbar_grid.get_children()
	for i in range(buttons.size()):
		var btn = buttons[i] as Button
		if btn:
			btn.button_pressed = (i + 1 == slot_number)

	#print("Hotbar slot selected: ", slot_number)
	hotbar_slot_selected.emit(slot_number)
