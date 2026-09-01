shader_type canvas_item;

varying vec4 v_color;

void vertex() {
    v_color = COLOR;
}

void fragment() {
    vec4 tex_color = v_color;
    
    // Discard pixels that blend too close to air/transparent
    if (tex_color.a < 0.5) {
        discard;
    }
    
    // Keep full opacity for valid terrain boundaries
    tex_color.a = 1.0;
    COLOR = tex_color;
}