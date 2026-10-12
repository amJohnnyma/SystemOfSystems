extends Sprite2D

@onready var world: GodotSimulation = $GodotSimulation

var position_vector: Vector2 = Vector2.ZERO
var velocity: Vector2 = Vector2.ZERO

# Physics constants
var gravity: float = 800.0
var move_speed: float = 200.0
var entity_width: float = 16.0
var entity_height: float = 24.0

func _ready() -> void:
	# 1. Load the player sprite texture
	texture = load("res://game/sprite/player.png")
	
	var world_width_pixels = 100 * 16 
	position_vector = Vector2(world_width_pixels * 0.5, 0.0)
	global_position = position_vector

func _physics_process(delta: float) -> void:
	# 1. Apply gravity to vertical velocity
	velocity.y += gravity * delta
	
	# 2. Optional: Horizontal input movement (left/right)
	var input_x := Input.get_axis("ui_left", "ui_right")
	velocity.x = input_x * move_speed

	# 3. Store old Y position to check if we hit the ground
	var old_y = position_vector.y

	# 4. Pass velocity and bounding box to C++ for collision resolution
	position_vector = world.resolveCollision(position_vector, velocity, entity_width, entity_height, delta)
	
	# 5. If we hit a floor and stopped moving downward, zero out vertical velocity
	if position_vector.y == old_y and velocity.y > 0:
		velocity.y = 0.0

	# 6. Update the actual node position in Godot
	global_position = position_vector
