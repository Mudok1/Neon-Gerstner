/*
 * Fragment Shader - Starfield Background
 * Renders soft glowing stars with neon colors
 */

#version 450 core

in float starBrightness;
out vec4 FragColor;

void main() {
    // Soft circular point - discard outside circle
    vec2 coord = gl_PointCoord - vec2(0.5);
    float dist = length(coord);
    if (dist > 0.5) discard; // Clean circle edge
    float alpha = 1.0 - smoothstep(0.2, 0.5, dist); // Tighter falloff
    
    // Star colors: mix of cool blues, magentas, and whites
    vec3 coolBlue = vec3(0.4, 0.6, 1.0);
    vec3 magenta = vec3(0.8, 0.3, 0.9);
    vec3 white = vec3(1.0, 0.95, 1.0);
    
    // Random color per star based on brightness
    vec3 starColor = mix(coolBlue, magenta, starBrightness * 0.5);
    starColor = mix(starColor, white, starBrightness * 0.3);
    
    FragColor = vec4(starColor * starBrightness, alpha * starBrightness);
}
