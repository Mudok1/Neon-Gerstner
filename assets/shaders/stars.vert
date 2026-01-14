/*
 * Vertex Shader - Starfield Background
 * Renders distant stars with subtle audio-reactive brightness
 */

#version 450 core

layout (location = 0) in vec2 position;

uniform float time;
uniform mat4 mvp;
uniform float uBass;
uniform float uTreble;

out float starBrightness;

// Pseudo-random based on position
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main() {
    // Add jitter to break grid patterns
    vec2 jitter = vec2(hash(position) - 0.5, hash(position * 1.5) - 0.5) * 1.5;
    vec2 jitteredPos = position + jitter;
    
    // Slow parallax drift
    jitteredPos.y -= time * 0.03;
    
    vec3 starPos = vec3(jitteredPos.x, jitteredPos.y, 0.0);
    gl_Position = mvp * vec4(starPos, 1.0);
    
    // Star size
    float baseSize = 1.0 + hash(position * 2.0) * 1.0;

    // Sparkle effect
    float starId = hash(position * 7.0);
    float sparkThreshold = 0.97 - (uTreble * 0.05); 
    float sparkPhase = sin(time * (2.0 + starId * 4.0) + starId * 100.0);
    float isSparking = step(sparkThreshold, starId) * step(0.6, sparkPhase);
    float sparkBoost = isSparking * (0.5 + uBass * 0.5);
    
    gl_PointSize = baseSize + sparkBoost * 1.5;
    
    // Base brightness
    float baseBrightness = 0.1 + hash(position * 3.0) * 0.15;
    float sparkBrightness = sparkBoost * 0.4;
    starBrightness = baseBrightness + sparkBrightness;
}
