#version 430 core
layout(location=0) in vec3 position;
layout(location=1) in vec4 color;
uniform mat4 uMVP;
out vec4 vertexColor;
void main() {
    gl_Position=uMVP*vec4(position,1.0);
    gl_PointSize=8.0;
    vertexColor=color;
}
