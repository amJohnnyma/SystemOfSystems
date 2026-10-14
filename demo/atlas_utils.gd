
class_name AtlasUtils
extends RefCounted


static func generate_atlas_coords(
		texture_dimensions: Vector2i,
		sprite_size: Vector2i,
		start_id: int = 1
) -> Dictionary[int, Vector2i]:
	var atlas: Dictionary[int, Vector2i] = {}

	if sprite_size.x <= 0 or sprite_size.y <= 0:
		push_error("Sprite size must be positive.")
		return atlas

	var columns: int = texture_dimensions.x / sprite_size.x
	var rows: int = texture_dimensions.y / sprite_size.y

	var id: int = start_id

	for y in range(rows):
		for x in range(columns):
			atlas[id] = Vector2i(x, y)
			id += 1

	return atlas
