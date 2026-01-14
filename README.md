# Neon Gerstner C++ Experiment

This project started as a personal experiment to explore C++ and OpenGL 4.5 capabilities ("Just trying to experiment a bit with C++"), but evolved into a full audio-reactive visualization.

![Initial Clip](clip.gif)

## Overview

A real-time particle simulation utilizing **Gerstner Waves** math to create an oceanic surface that dances to music.

### Key Features
*   **Audio Reactivity (WASAPI):** The system captures desktop audio in real-time, analyzing Bass, Mids, and Treble.
    *   **Bass:** Drives wave height and background lighting pulse.
    *   **Treble:** Controls particle "sparkle" and speed.
    *   **Silence Detection:** Waves calm down smoothly when music pulses.
*   **Visuals:**
    *   **Neon Palette:** A carefully balanced Cyan/Magenta cyberpunk aesthetic.
    *   **Dynamic Layers:** 3 distinct wave layers (Near, Main, Far) with unique behaviors and density.
    *   **Nebula Skybox:** A procedural, noise-driven background with random lightning storms.
    *   **Parallax Starfield:** Floating stars that drift and glimmer.

![Final State](/uploaded_image_1768374130786.png)
