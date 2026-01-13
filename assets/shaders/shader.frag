/*
 * Fragment Shader - Particulas Neon
 * Renderiza particulas circulares con brillo suave
 */

#version 450 core

in vec3 particleColor;
out vec4 FragColor;

void main() {
    // Crear forma circular suave
    vec2 coord = gl_PointCoord - vec2(0.5);
    float dist = length(coord);
    
    // Descartamos pixeles fuera del circulo
    if (dist > 0.5) discard;
    
    // Brillo que se desvanece hacia los bordes
    float glow = 1.0 - smoothstep(0.0, 0.5, dist);
    
    // Color final con alpha para efecto de glow
    FragColor = vec4(particleColor * glow, glow);
}
