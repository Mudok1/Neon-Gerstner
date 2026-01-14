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
    // Reverted to linear multiplier (but stronger) to fix "dead zone" at low volume
    // pow(2.0) was killing impacts below 50% volume. 
    float bassPunch = uBass * 0.4; 
    float audioEnergy = (bassPunch * 0.8) + (uMids * 0.2);
    float audioAmp = audioEnergy * 0.5; // Increased multiplier for visibility
    float currentAmp = amplitude + audioAmp;
    
    // Wave 1
    float phase1 = dot(dir1, driftedPos) * frequency - t * speed;
    float wave1 = sin(phase1) * currentAmp;
    float dx1 = steepness * currentAmp * dir1.x * cos(phase1);
    float dz1 = steepness * currentAmp * dir1.y * cos(phase1);
    
    // Wave 2 (crossed)
    float phase2 = dot(dir2, driftedPos) * frequency * 1.3 - t * speed * 0.8;
    float wave2 = sin(phase2) * currentAmp * 0.6;
    float dx2 = steepness * currentAmp * 0.6 * dir2.x * cos(phase2);
    float dz2 = steepness * currentAmp * 0.6 * dir2.y * cos(phase2);
    
    // Combine
    float height = wave1 + wave2;
    float offsetX = dx1 + dx2;
    float offsetZ = dz1 + dz2;
    
    return vec3(driftedPos.x + offsetX, height, driftedPos.y + offsetZ);
}

// Pseudo-random for sparkles (kept just in case, though unused currently)
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main() {
    vec3 wavePos = gerstnerWave(position, time);
    wavePos.y += layerOffset;
    gl_Position = mvp * vec4(wavePos, 1.0);
    
    // Size and Color based on height
    // Normalize against consistent range
    float maxExpectedAmp = 0.4;
    float rawHeight = (wavePos.y - layerOffset + maxExpectedAmp) / (2.0 * maxExpectedAmp);
    float heightFactor = pow(clamp(rawHeight, 0.001, 1.0), peakExp); 
    
    // Treble adds sparkle size
    float sparkleBoost = uTreble * 2.0; 
    
    float maxSize = ((peakExp > 1.5) ? 7.0 : 6.0) + sparkleBoost; 
    float minSize = 2.0; 
    gl_PointSize = mix(minSize, maxSize, heightFactor) * intensity;
    
    // Identify Layers based on Grid Size
    // Far: 25.0 (> 10.0)
    // Main: 6.0 (> 5.0)
    // Near: 4.5 (< 5.0)
    bool isFarLayer = uGridSize > 10.0;
    bool isMainLayer = uGridSize > 5.0 && !isFarLayer;

    // Color Palette: Deep Cyan to Neon Magenta
    vec3 cyan, magenta;
    
    if (isFarLayer) {
        // Far Layer: Strong, Electric Neon (Original values)
        cyan = vec3(0.1, 0.6, 0.9);
        magenta = vec3(1.3, 0.1, 1.3); 
    } else {
        // Main/Near Layers: Slightly toned down ("Bajarle un tonito")
        // Reducing intensity by 15%
        cyan = vec3(0.1, 0.6, 0.9) * 0.85;
        magenta = vec3(1.3, 0.1, 1.3) * 0.85;
    }
    
    vec3 pastelPink = vec3(1.0, 0.8, 1.0); // Foam color
    
    // Brighten slightly with treble
    magenta += vec3(uTreble * 0.2); 
    
    // Color Mix Factor (Cyan -> Magenta)
    float colorMix = smoothstep(0.35, 0.75, rawHeight);
    vec3 baseColor = mix(cyan, magenta, colorMix);
    
    // Foam/White Tip Factor
    float foamMix = 0.0;
    
    if (isFarLayer) {
        // Far: No foam (Pure Neon)
        foamMix = 0.0;
    } else if (isMainLayer) {
        // Main: Standard foam (Visible tips)
        foamMix = smoothstep(0.93, 1.0, rawHeight);
    } else {
        // Near: "Menos notorio" (Less visible)
        // Same threshold but reduced intensity
        foamMix = smoothstep(0.93, 1.0, rawHeight) * 0.3;
    }
    
    // Dynamic Brightness Pulse
    float pulse = 0.0;
    if (isFarLayer) {
        // "Capa Inferior": Strong Neon Reactivity
        pulse = uBass * 0.8; 
    } else {
        // Main/Near layers: Subtle pulse
        pulse = uBass * 0.15;
    }
    
    float dynamicIntensity = intensity * (1.0 + pulse);
    
    // Apply final mix: Base -> Pastel Pink (only if foamMix > 0)
    particleColor = mix(baseColor, pastelPink, foamMix) * dynamicIntensity;
}
