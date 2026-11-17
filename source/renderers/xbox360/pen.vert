#version 450
layout(location = 0) in vec4 a_pos;
layout(location = 1) in vec4 a_color;
layout(location = 0) out vec4 v_color;
void main() {
    gl_Position = a_pos;
    v_color = a_color;
}
