/*
 * Neon Gerstner - Visualizador de Ondas con Bloom
 * Particulas animadas con ecuacion de Gerstner y efecto glow
 */

#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "AudioCapture.h" // Modulo de audio
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Config
unsigned int currentWidth = 1280;
unsigned int currentHeight = 720;
bool needsResize = false;

// Framebuffers globales
unsigned int sceneFBO = 0, sceneColorBuffer = 0;
unsigned int pingpongFBO[2] = {0, 0};
unsigned int pingpongBuffer[2] = {0, 0};

// Prototipos
void framebufferSizeCallback(GLFWwindow *window, int width, int height);
void processInput(GLFWwindow *window);
std::string readFile(const std::string &path);
unsigned int createShader(const char *vertexCode, const char *fragmentCode);
std::vector<float> generateGrid(int size, float spacing);
void createFramebuffers(unsigned int width, unsigned int height);
void setupQuad(unsigned int &quadVAO, unsigned int &quadVBO);

// Camera
float cameraDistance = 2.5f;
float cameraAngleX = 0.5f;
float cameraAngleY = 0.0f;
double lastMouseX = 0, lastMouseY = 0;

// Time Control
float accumulatedTime = 0.0f;
float lastFrameTime = 0.0f;

bool mousePressed = false;

void scrollCallback(GLFWwindow *window, double xoffset, double yoffset) {
  cameraDistance -= (float)yoffset * 0.3f;
  cameraDistance = glm::clamp(cameraDistance, 0.5f, 10.0f);
}

int main() {
  if (!glfwInit()) {
    std::cerr << "Error iniciando GLFW" << std::endl;
    return -1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow *window = glfwCreateWindow(currentWidth, currentHeight,
                                        "Neon Gerstner", nullptr, nullptr);
  if (!window) {
    std::cerr << "Error creando ventana" << std::endl;
    glfwTerminate();
    return -1;
  }

  // Iniciar captura de audio
  AudioCapture audioCapture;
  audioCapture.start();

  lastFrameTime = (float)glfwGetTime();

  glfwMakeContextCurrent(window);
  glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
  glfwSetScrollCallback(window, scrollCallback);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cerr << "Error iniciando GLAD" << std::endl;
    return -1;
  }

  std::cout << "OpenGL: " << glGetString(GL_VERSION) << std::endl;
  std::cout << "GPU: " << glGetString(GL_RENDERER) << std::endl;

  // Additive blending for glow effect
  glBlendFunc(GL_ONE, GL_ONE);
  glEnable(GL_PROGRAM_POINT_SIZE);

  // Shaders
  std::string vertCode = readFile("shaders/shader.vert");
  std::string fragCode = readFile("shaders/shader.frag");
  unsigned int particleShader =
      createShader(vertCode.c_str(), fragCode.c_str());

  std::string bloomVertCode = readFile("shaders/bloom.vert");
  std::string bloomFragCode = readFile("shaders/bloom.frag");
  unsigned int bloomShader =
      createShader(bloomVertCode.c_str(), bloomFragCode.c_str());

  // === CAPA PRINCIPAL (200x200) ===
  std::vector<float> mainGrid = generateGrid(200, 0.03f);
  int mainCount = 200 * 200;
  unsigned int mainVAO, mainVBO;
  glGenVertexArrays(1, &mainVAO);
  glGenBuffers(1, &mainVBO);
  glBindVertexArray(mainVAO);
  glBindBuffer(GL_ARRAY_BUFFER, mainVBO);
  glBufferData(GL_ARRAY_BUFFER, mainGrid.size() * sizeof(float),
               mainGrid.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  // === CAPA CERCANA (300x300) ===
  std::vector<float> nearGrid = generateGrid(300, 0.015f);
  int nearCount = 300 * 300;
  unsigned int nearVAO, nearVBO;
  glGenVertexArrays(1, &nearVAO);
  glGenBuffers(1, &nearVBO);
  glBindVertexArray(nearVAO);
  glBindBuffer(GL_ARRAY_BUFFER, nearVBO);
  glBufferData(GL_ARRAY_BUFFER, nearGrid.size() * sizeof(float),
               nearGrid.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  // === CAPA LEJANA (100x100) - MUY ESPACIADA ===
  std::vector<float> farGrid = generateGrid(100, 0.25f);
  int farCount = 100 * 100;
  unsigned int farVAO, farVBO;
  glGenVertexArrays(1, &farVAO);
  glGenBuffers(1, &farVBO);
  glBindVertexArray(farVAO);
  glBindBuffer(GL_ARRAY_BUFFER, farVBO);
  glBufferData(GL_ARRAY_BUFFER, farGrid.size() * sizeof(float), farGrid.data(),
               GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  std::cout << "Capa principal: " << mainCount << " particulas" << std::endl;
  std::cout << "Capa cercana: " << nearCount << " particulas" << std::endl;
  std::cout << "Capa lejana: " << farCount << " particulas" << std::endl;

  createFramebuffers(currentWidth, currentHeight);

  unsigned int quadVAO, quadVBO;
  setupQuad(quadVAO, quadVBO);

  // Uniforms
  int timeLoc = glGetUniformLocation(particleShader, "time");
  int mvpLoc = glGetUniformLocation(particleShader, "mvp");
  int layerOffsetLoc = glGetUniformLocation(particleShader, "layerOffset");
  int intensityLoc = glGetUniformLocation(particleShader, "intensity");
  int gridSizeLoc = glGetUniformLocation(particleShader, "uGridSize");
  int peakExpLoc = glGetUniformLocation(particleShader, "peakExp");

  // Audio uniforms
  int bassLoc = glGetUniformLocation(particleShader, "uBass");
  int midsLoc = glGetUniformLocation(particleShader, "uMids");
  int trebleLoc = glGetUniformLocation(particleShader, "uTreble");

  int sceneLoc = glGetUniformLocation(bloomShader, "scene");
  int bloomBlurLoc = glGetUniformLocation(bloomShader, "bloomBlur");
  int horizontalLoc = glGetUniformLocation(bloomShader, "horizontal");
  int passTypeLoc = glGetUniformLocation(bloomShader, "passType");
  int bloomStrengthLoc = glGetUniformLocation(bloomShader, "bloomStrength");

  while (!glfwWindowShouldClose(window)) {
    processInput(window);

    if (needsResize) {
      createFramebuffers(currentWidth, currentHeight);
      needsResize = false;
    }

    // === RENDERIZAR ESCENA ===
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);
    glViewport(0, 0, currentWidth, currentHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_BLEND);

    glUseProgram(particleShader);
    glUseProgram(particleShader);

    // Calcular tiempo variable basado en musica
    float currentFrameTime = (float)glfwGetTime();
    float deltaTime = currentFrameTime - lastFrameTime;
    lastFrameTime = currentFrameTime;

    // Mids control simulation speed boost
    float speedMultiplier = 1.0f + (audioCapture.getMids() * 2.5f);
    accumulatedTime += deltaTime * speedMultiplier;

    glUniform1f(timeLoc, accumulatedTime);

    // Pasar datos de audio
    float bass = audioCapture.getBass();
    float mids = audioCapture.getMids();
    float treble = audioCapture.getTreble();

    glUniform1f(bassLoc, bass);
    glUniform1f(midsLoc, mids);
    glUniform1f(trebleLoc, treble);

    glm::mat4 projection = glm::perspective(
        glm::radians(45.0f), (float)currentWidth / (float)currentHeight, 0.1f,
        100.0f);

    // --- CAPA LEJANA ---
    glm::mat4 farView = glm::mat4(1.0f);
    farView =
        glm::translate(farView, glm::vec3(0.0f, -1.0f, -cameraDistance - 0.5f));
    farView = glm::rotate(farView, cameraAngleX, glm::vec3(1.0f, 0.0f, 0.0f));
    farView = glm::rotate(farView, cameraAngleY, glm::vec3(0.0f, 1.0f, 0.0f));
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE,
                       glm::value_ptr(projection * farView));
    glUniform1f(layerOffsetLoc, 0.0f);
    glUniform1f(intensityLoc, 0.6f);
    glUniform1f(gridSizeLoc, 100.0f * 0.25f);
    glUniform1f(peakExpLoc, 10.0f);
    glBindVertexArray(farVAO);
    glDrawArrays(GL_POINTS, 0, farCount);

    // --- CAPA PRINCIPAL ---
    glm::mat4 mainView = glm::mat4(1.0f);
    mainView = glm::translate(mainView, glm::vec3(0.0f, 0.0f, -cameraDistance));
    mainView = glm::rotate(mainView, cameraAngleX, glm::vec3(1.0f, 0.0f, 0.0f));
    mainView = glm::rotate(mainView, cameraAngleY, glm::vec3(0.0f, 1.0f, 0.0f));
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE,
                       glm::value_ptr(projection * mainView));
    glUniform1f(layerOffsetLoc, 0.0f);
    glUniform1f(intensityLoc, 1.8f);
    glUniform1f(gridSizeLoc, 200.0f * 0.03f);
    glUniform1f(peakExpLoc, 1.0f);
    glBindVertexArray(mainVAO);
    glDrawArrays(GL_POINTS, 0, mainCount);

    // --- CAPA CERCANA ---
    glm::mat4 nearView = glm::mat4(1.0f);
    nearView = glm::translate(nearView, glm::vec3(0.0f, 0.3f, -1.2f));
    nearView = glm::rotate(nearView, cameraAngleX, glm::vec3(1.0f, 0.0f, 0.0f));
    nearView = glm::rotate(nearView, cameraAngleY, glm::vec3(0.0f, 1.0f, 0.0f));
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE,
                       glm::value_ptr(projection * nearView));
    glUniform1f(layerOffsetLoc, 0.0f);
    glUniform1f(intensityLoc, 0.5f);
    glUniform1f(gridSizeLoc, 300.0f * 0.015f);
    glUniform1f(peakExpLoc, 1.0f);
    glBindVertexArray(nearVAO);
    glDrawArrays(GL_POINTS, 0, nearCount);

    // === BLUR ===
    glDisable(GL_BLEND);
    glUseProgram(bloomShader);
    glUniform1i(passTypeLoc, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[0]);
    glClear(GL_COLOR_BUFFER_BIT);
    glUniform1i(horizontalLoc, 1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneColorBuffer);
    glUniform1i(sceneLoc, 0);
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[1]);
    glClear(GL_COLOR_BUFFER_BIT);
    glUniform1i(horizontalLoc, 0);
    glBindTexture(GL_TEXTURE_2D, pingpongBuffer[0]);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    for (int i = 0; i < 3; i++) {
      glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[0]);
      glUniform1i(horizontalLoc, 1);
      glBindTexture(GL_TEXTURE_2D, pingpongBuffer[1]);
      glDrawArrays(GL_TRIANGLES, 0, 6);

      glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[1]);
      glUniform1i(horizontalLoc, 0);
      glBindTexture(GL_TEXTURE_2D, pingpongBuffer[0]);
      glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    // === COMBINAR ===
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, currentWidth, currentHeight);
    glClearColor(0.01f, 0.01f, 0.01f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUniform1i(passTypeLoc, 1);

    float t = glm::clamp((cameraDistance - 1.0f) / 8.0f, 0.0f, 1.0f);
    float dynamicBloom = glm::mix(1.0f, 0.5f, t);
    glUniform1f(bloomStrengthLoc, dynamicBloom);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneColorBuffer);
    glUniform1i(sceneLoc, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, pingpongBuffer[1]);
    glUniform1i(bloomBlurLoc, 1);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glDeleteVertexArrays(1, &mainVAO);
  glDeleteBuffers(1, &mainVBO);
  glDeleteVertexArrays(1, &nearVAO);
  glDeleteBuffers(1, &nearVBO);
  glDeleteVertexArrays(1, &farVAO);
  glDeleteBuffers(1, &farVBO);
  glDeleteProgram(particleShader);
  glDeleteProgram(bloomShader);
  glfwTerminate();

  return 0;
}

void framebufferSizeCallback(GLFWwindow *window, int width, int height) {
  if (width > 0 && height > 0) {
    currentWidth = width;
    currentHeight = height;
    needsResize = true;
  }
}

void processInput(GLFWwindow *window) {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);

  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
    cameraDistance = glm::max(0.5f, cameraDistance - 0.03f);
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
    cameraDistance = glm::min(10.0f, cameraDistance + 0.03f);

  double mouseX, mouseY;
  glfwGetCursorPos(window, &mouseX, &mouseY);
  if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
    if (mousePressed) {
      cameraAngleY += (float)(mouseX - lastMouseX) * 0.005f;
      cameraAngleX += (float)(mouseY - lastMouseY) * 0.005f;
    }
    mousePressed = true;
  } else {
    mousePressed = false;
  }
  lastMouseX = mouseX;
  lastMouseY = mouseY;
}

std::string readFile(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    std::cerr << "Error abriendo: " << path << std::endl;
    return "";
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

std::vector<float> generateGrid(int size, float spacing) {
  std::vector<float> vertices;
  float offset = (size - 1) * spacing / 2.0f;
  for (int z = 0; z < size; z++) {
    for (int x = 0; x < size; x++) {
      vertices.push_back(x * spacing - offset);
      vertices.push_back(z * spacing - offset);
    }
  }
  return vertices;
}

void createFramebuffers(unsigned int width, unsigned int height) {
  if (sceneFBO != 0) {
    glDeleteFramebuffers(1, &sceneFBO);
    glDeleteTextures(1, &sceneColorBuffer);
    glDeleteFramebuffers(2, pingpongFBO);
    glDeleteTextures(2, pingpongBuffer);
  }

  glGenFramebuffers(1, &sceneFBO);
  glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);

  glGenTextures(1, &sceneColorBuffer);
  glBindTexture(GL_TEXTURE_2D, sceneColorBuffer);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA,
               GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         sceneColorBuffer, 0);

  glGenFramebuffers(2, pingpongFBO);
  glGenTextures(2, pingpongBuffer);

  for (int i = 0; i < 2; i++) {
    glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
    glBindTexture(GL_TEXTURE_2D, pingpongBuffer[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA,
                 GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           pingpongBuffer[i], 0);
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void setupQuad(unsigned int &quadVAO, unsigned int &quadVBO) {
  float quadVertices[] = {-1.0f, 1.0f,  0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f,
                          1.0f,  -1.0f, 1.0f, 0.0f, -1.0f, 1.0f,  0.0f, 1.0f,
                          1.0f,  -1.0f, 1.0f, 0.0f, 1.0f,  1.0f,  1.0f, 1.0f};

  glGenVertexArrays(1, &quadVAO);
  glGenBuffers(1, &quadVBO);
  glBindVertexArray(quadVAO);
  glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices,
               GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        (void *)(2 * sizeof(float)));
  glEnableVertexAttribArray(1);
}

unsigned int createShader(const char *vertexCode, const char *fragmentCode) {
  int success;
  char infoLog[512];

  unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vs, 1, &vertexCode, nullptr);
  glCompileShader(vs);
  glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(vs, 512, nullptr, infoLog);
    std::cerr << "Vertex shader error: " << infoLog << std::endl;
  }

  unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fs, 1, &fragmentCode, nullptr);
  glCompileShader(fs);
  glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(fs, 512, nullptr, infoLog);
    std::cerr << "Fragment shader error: " << infoLog << std::endl;
  }

  unsigned int program = glCreateProgram();
  glAttachShader(program, vs);
  glAttachShader(program, fs);
  glLinkProgram(program);
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (!success) {
    glGetProgramInfoLog(program, 512, nullptr, infoLog);
    std::cerr << "Shader link error: " << infoLog << std::endl;
  }

  glDeleteShader(vs);
  glDeleteShader(fs);

  return program;
}
