/*
 * Fragment Shader - Bloom
 * Blur gaussiano + combinacion con escena original
 */

#version 450 core

in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D scene;       // Escena original
uniform sampler2D bloomBlur;   // Escena con blur
uniform int horizontal;        // 0 = vertical, 1 = horizontal
uniform int passType;          // 0 = blur, 1 = combine
uniform float bloomStrength;   // Controlado desde CPU

// Pesos del blur gaussiano
const float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main() {
    vec2 texOffset = 1.0 / textureSize(scene, 0);
    
    if (passType == 0) {
        // Pasada de blur
        vec3 result = texture(scene, TexCoords).rgb * weights[0];
        
        if (horizontal == 1) {
            for (int i = 1; i < 5; i++) {
                result += texture(scene, TexCoords + vec2(texOffset.x * i, 0.0)).rgb * weights[i];
                result += texture(scene, TexCoords - vec2(texOffset.x * i, 0.0)).rgb * weights[i];
            }
        } else {
            for (int i = 1; i < 5; i++) {
                result += texture(scene, TexCoords + vec2(0.0, texOffset.y * i)).rgb * weights[i];
                result += texture(scene, TexCoords - vec2(0.0, texOffset.y * i)).rgb * weights[i];
            }
        }
        
        FragColor = vec4(result, 1.0);
    } else {
        // Pasada de combinacion
        vec3 hdrColor = texture(scene, TexCoords).rgb;
        vec3 bloomColor = texture(bloomBlur, TexCoords).rgb;
        
        // Mezcla aditiva con intensidad controlada por CPU
        vec3 result = hdrColor + bloomColor * bloomStrength;
        
        // Exposure tone mapping - mantiene colores mas saturados
        float exposure = 0.7;
        result = vec3(1.0) - exp(-result * exposure);
        
        // Boost de saturacion para efecto neon (evita lavado a blanco)
        float luminance = dot(result, vec3(0.299, 0.587, 0.114));
        vec3 satBoost = mix(vec3(luminance), result, 1.3); // 1.3 = 30% mas saturacion
        result = clamp(satBoost, 0.0, 1.0);
        
        // Gamma correction
        result = pow(result, vec3(1.0/2.2));
        
        FragColor = vec4(result, 1.0);
    }
}
