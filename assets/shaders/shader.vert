/*
 * Vertex Shader - Ondas Gerstner
 * Desplaza particulas siguiendo la ecuacion de onda de Gerstner
 */

#version 450 core

layout (location = 0) in vec2 position;

uniform float time;
uniform mat4 mvp;
uniform float layerOffset;  // Offset vertical de la capa
uniform float intensity;    // Intensidad del color (1.0 = principal)
uniform float uGridSize;    // Tamaño del grid para wrapping correcto
uniform float peakExp;      // Exponente para resaltar solo picos (1.0 = normal, >1.0 = solo picos)

// Audio Uniforms (Reactividad)
uniform float uBass;
uniform float uMids;
uniform float uTreble;

out vec3 particleColor;

// Parametros de las ondas
const float amplitude = 0.15;
const float frequency = 3.0;
const float speed = 1.5;
const float steepness = 0.5;
const float PI = 3.14159;

// Funcion de onda Gerstner
vec3 gerstnerWave(vec2 pos, float t) {
    // Scroll infinito uniforme
    float driftSpeed = 0.4;
    float gridSize = uGridSize;
    
    // Wrap en ambos ejes para evitar bordes
    vec2 driftedPos;
    driftedPos.x = mod(pos.x + gridSize * 0.5, gridSize) - gridSize * 0.5;
    driftedPos.y = mod(pos.y + t * driftSpeed + gridSize * 0.5, gridSize) - gridSize * 0.5;
    
    // Direcciones de onda
    vec2 dir1 = normalize(vec2(1.0, 0.5));
    vec2 dir2 = normalize(vec2(-0.7, 1.0));
    
    // Audio Reactivity (Amplitude)
    float audioAmp = uBass * 0.15; 
    float currentAmp = amplitude + audioAmp;
    
    // Onda 1
    float phase1 = dot(dir1, driftedPos) * frequency - t * speed;
    float wave1 = sin(phase1) * currentAmp;
    float dx1 = steepness * currentAmp * dir1.x * cos(phase1);
    float dz1 = steepness * currentAmp * dir1.y * cos(phase1);
    
    // Onda 2 (cruzada)
    float phase2 = dot(dir2, driftedPos) * frequency * 1.3 - t * speed * 0.8;
    float wave2 = sin(phase2) * currentAmp * 0.6;
    float dx2 = steepness * currentAmp * 0.6 * dir2.x * cos(phase2);
    float dz2 = steepness * currentAmp * 0.6 * dir2.y * cos(phase2);
    
    // Combinar
    float height = wave1 + wave2;
    float offsetX = dx1 + dx2;
    float offsetZ = dz1 + dz2;
    
    return vec3(driftedPos.x + offsetX, height, driftedPos.y + offsetZ);
}

void main() {
    vec3 wavePos = gerstnerWave(position, time);
    wavePos.y += layerOffset;
    gl_Position = mvp * vec4(wavePos, 1.0);
    
    // Size based on height
    float rawHeight = (wavePos.y - layerOffset + amplitude) / (2.0 * amplitude);
    float heightFactor = pow(clamp(rawHeight, 0.001, 1.0), peakExp); 
    
    // Treble adds sparkle size
    float sparkleBoost = uTreble * 2.0; 
    
    float maxSize = ((peakExp > 1.5) ? 7.0 : 6.0) + sparkleBoost; 
    float minSize = 2.0; 
    gl_PointSize = mix(minSize, maxSize, heightFactor) * intensity;
    
    // Color Palette: Deep Cyan to Neon Magenta
    vec3 cyan = vec3(0.1, 0.6, 0.9);
    vec3 magenta = vec3(1.4, 0.2, 1.4);
    
    // Treble brightens the color
    magenta += vec3(uTreble * 0.5);
    
    particleColor = mix(cyan, magenta, pow(heightFactor, 1.2)) * intensity;
}
