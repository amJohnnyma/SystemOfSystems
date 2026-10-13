shader_type canvas_item;

uniform sampler2D terrain_atlas : source_color, filter_nearest;

varying vec2 secondary_uv;

void vertex() {
    secondary_uv = CUSTOM0.xy;
}

void fragment() {
    vec4 texture_a = texture(terrain_atlas, UV);
    vec4 texture_b = texture(terrain_atlas, secondary_uv);

    COLOR = mix(texture_a, texture_b, 0.5);
}