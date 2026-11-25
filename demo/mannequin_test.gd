extends Node2D

@onready var viewport: SubViewport = $ModelViewport
@onready var player_visual: Sprite2D = $PlayerVisual
@onready var model_camera : Camera3D = $ModelViewport/Camera3D

func _ready() -> void:
	player_visual.texture = viewport.get_texture()
	model_camera.size = 5.0                                                                                                                                                                                                                
	model_camera.make_current()                                                                                                                                                                                                             
	print("Rendering camera: ", viewport.get_camera_3d().get_path())                                                                                                                                                                        
	print("Rendering size: ", viewport.get_camera_3d().size)
	var active_camera := viewport.get_camera_3d()                                                                                                                                                                                           
	print("Active viewport camera: ", active_camera)                                                                                                                                                                                        
	if active_camera:                                                                                                                                                                                                                       
		print("Camera size: ", active_camera.size)                                                                                                                                                                                          
		print("Camera projection: ", active_camera.projection)                                                                                                                                                                              
	print("Viewport size: ", viewport.size)                                                                                                                                                                                                 
	print("Sprite region enabled: ", player_visual.region_enabled)      
