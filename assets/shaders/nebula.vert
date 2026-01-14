/*
 * Vertex Shader - Nebula Background (3D)
 * Renders nebula on 3D panels like starfield
 */

#version 450 core

layout (location = 0) in vec3 position;

uniform mat4 mvp;

out vec3 fragTexCoord;

void main() {
    // Use position directly
    gl_Position = mvp * vec4(position, 1.0);
    
    // Pass 3D position for 3D noise sampling
    fragTexCoord = position;
}
