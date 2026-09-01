extends Node

var canvas_layer: CanvasLayer
var fps_label: Label

func _ready() -> void:
	# 1. Create a CanvasLayer to lock rendering to the viewport/screen space
	canvas_layer = CanvasLayer.new()
	canvas_layer.layer = 100 # High layer number ensures it renders on top of everything
	add_child(canvas_layer)

	# 2. Create and configure the Label
	fps_label = Label.new()
	fps_label.position = Vector2(16, 16) # Offset slightly from top-left screen edge
	fps_label.add_theme_color_override("font_color", Color.GREEN)

	# 3. Add the Label to the CanvasLayer (NOT directly to world nodes)
	canvas_layer.add_child(fps_label)

func _process(_delta: float) -> void:
	var current_fps := Engine.get_frames_per_second()
	fps_label.text = "FPS: %d" % current_fps
