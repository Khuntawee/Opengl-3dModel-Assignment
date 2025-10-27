// main.cpp
// Full scene: skybox, textured floor, controllable car, third-person chase camera

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader_m.h>
#include <learnopengl/model.h>

#include <iostream>
#include <vector>

// window
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// camera follow parameters (third-person)
glm::vec3 cameraUp(0.0f, 1.0f, 0.0f);
glm::vec3 cameraPos(0.0f, 3.0f, 8.0f); // initial
float cameraSmoothSpeed = 6.0f; // lerp speed

// car state
glm::vec3 carPos(0.0f, 0.0f, 0.0f);
float carYaw = 0.0f;       // degrees, 0 -> +Z in our code (consistent with example)
float carSpeed = 0.0f;     // units per second

// car bounding box (approximate)
glm::vec3 carSize(1.5f, 1.0f, 3.0f); // width, height, length

// wall bounding box (match your wall dimensions)
glm::vec3 wallPos(0.0f, 2.0f, 20.0f); // center
glm::vec3 wallSize(4.0f, 4.0f, 0.5f); // width, height, depth

// physics params
const float MAX_SPEED = 12.0f;
const float ACCELERATION = 20.0f;   // units/s^2
const float BRAKE = 30.0f;
const float FRICTION = 6.0f;
const float TURN_SPEED = 90.0f;     // degrees per second at full input

// inputs
bool keys[1024] = {false};

// function declarations
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
unsigned int loadTexture(const char *path);
unsigned int loadCubemap(std::vector<std::string> faces);

// Axis-Aligned Bounding Box collision check
bool checkCollision(const glm::vec3 &posA, const glm::vec3 &sizeA,
                    const glm::vec3 &posB, const glm::vec3 &sizeB)
{
    return (fabs(posA.x - posB.x) * 2 < (sizeA.x + sizeB.x)) &&
           (fabs(posA.y - posB.y) * 2 < (sizeA.y + sizeB.y)) &&
           (fabs(posA.z - posB.z) * 2 < (sizeA.z + sizeB.z));
}

int main()
{
    // ---- GLFW init ----
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Car + Skybox + Textured Floor", nullptr, nullptr);
    if (!window) { std::cerr << "Failed to create GLFW window\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cerr << "Failed to initialize GLAD\n"; return -1; }
    glEnable(GL_DEPTH_TEST);

    // ---- Shaders ----
    Shader modelShader("1.model_loading.vs", "1.model_loading.fs"); // your existing model shader
    Shader skyboxShader("6.2.skybox.vs", "6.2.skybox.fs");         // your existing skybox shader
    Shader floorShader("floor.vs", "floor.fs");                   // floor shader provided below

    // ---- FLOOR geometry (big tiled quad) ----
    float floorVertices[] = {
        // positions            // normals         // texcoords
        -50.0f, 0.0f, -50.0f,    0.0f, 1.0f, 0.0f,    0.0f, 50.0f,
         50.0f, 0.0f, -50.0f,    0.0f, 1.0f, 0.0f,   50.0f, 50.0f,
         50.0f, 0.0f,  50.0f,    0.0f, 1.0f, 0.0f,   50.0f,  0.0f,
        -50.0f, 0.0f,  50.0f,    0.0f, 1.0f, 0.0f,    0.0f,  0.0f
    };
    unsigned int floorIndices[] = { 0, 1, 2, 2, 3, 0 };

    unsigned int floorVAO, floorVBO, floorEBO;
    glGenVertexArrays(1, &floorVAO);
    glGenBuffers(1, &floorVBO);
    glGenBuffers(1, &floorEBO);

    glBindVertexArray(floorVAO);
    glBindBuffer(GL_ARRAY_BUFFER, floorVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(floorVertices), floorVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(floorIndices), floorIndices, GL_STATIC_DRAW);

    // pos
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // texcoord
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    // ---- Load floor texture ----
    unsigned int floorTex = loadTexture(FileSystem::getPath("resources/textures/wood.png").c_str());
    if (floorTex == 0) std::cout << "Warning: Floor texture failed to load\n";

    // ---- Skybox geometry ----
    float skyboxVertices[] = {
        // positions
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    // ---- WALL geometry ----
    float wallVertices[] = {
        // positions          // normals          // texcoords
        -2.0f, 0.0f, 20.0f,    0.0f, 0.0f, -1.0f,    0.0f, 0.0f, // bottom left
        2.0f, 0.0f, 20.0f,    0.0f, 0.0f, -1.0f,    1.0f, 0.0f, // bottom right
        2.0f, 4.0f, 20.0f,    0.0f, 0.0f, -1.0f,    1.0f, 1.0f, // top right
        -2.0f, 4.0f, 20.0f,    0.0f, 0.0f, -1.0f,    0.0f, 1.0f  // top left
    };
    unsigned int wallIndices[] = { 0, 1, 2, 2, 3, 0 };

    unsigned int wallVAO, wallVBO, wallEBO;
    glGenVertexArrays(1, &wallVAO);
    glGenBuffers(1, &wallVBO);
    glGenBuffers(1, &wallEBO);

    glBindVertexArray(wallVAO);
    glBindBuffer(GL_ARRAY_BUFFER, wallVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(wallVertices), wallVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wallEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(wallIndices), wallIndices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);


    // ---- Load cubemap textures ----
    std::vector<std::string> faces
    {
        FileSystem::getPath("resources/textures/skybox/right.jpg"),
        FileSystem::getPath("resources/textures/skybox/left.jpg"),
        FileSystem::getPath("resources/textures/skybox/top.jpg"),
        FileSystem::getPath("resources/textures/skybox/bottom.jpg"),
        FileSystem::getPath("resources/textures/skybox/front.jpg"),
        FileSystem::getPath("resources/textures/skybox/back.jpg")
    };
    unsigned int cubemapTexture = loadCubemap(faces);
    skyboxShader.use();
    skyboxShader.setInt("skybox", 0);

    // ---- Load car model ----
    Model carModel(FileSystem::getPath("resources/objects/AC Cobra/Shelby.obj"));

    // Light position (for floor lighting)
    glm::vec3 lightPos(0.0f, 10.0f, 0.0f);

    // initial camera position behind car
    {
        glm::vec3 forward = glm::vec3(sin(glm::radians(carYaw)), 0.0f, cos(glm::radians(carYaw)));
        cameraPos = carPos - forward * 8.0f + glm::vec3(0.0f, 3.0f, 0.0f);
    }

    // ---- Render loop ----
    while (!glfwWindowShouldClose(window))
    {
        // per-frame time
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // ---- input / physics ----
        // accelerate / brake
        float accelInput = 0.0f;
        if (keys[GLFW_KEY_W]) accelInput += 1.0f;
        if (keys[GLFW_KEY_S]) accelInput -= 1.0f;

        // steering
        float steerInput = 0.0f;
        if (keys[GLFW_KEY_A]) steerInput += 1.0f;
        if (keys[GLFW_KEY_D]) steerInput -= 1.0f;

        // update speed
        if (accelInput > 0.0f) {
            carSpeed += ACCELERATION * accelInput * deltaTime;
        } else if (accelInput < 0.0f) {
            carSpeed += -BRAKE * (-accelInput) * deltaTime;
        } else {
            // friction / rolling resistance
            if (carSpeed > 0.0f) carSpeed -= FRICTION * deltaTime;
            else if (carSpeed < 0.0f) carSpeed += FRICTION * deltaTime;
        }
        // clamp small speeds to zero
        if (fabs(carSpeed) < 0.01f) carSpeed = 0.0f;
        // clamp to max
        if (carSpeed > MAX_SPEED) carSpeed = MAX_SPEED;
        if (carSpeed < -MAX_SPEED * 0.5f) carSpeed = -MAX_SPEED * 0.5f; // slower reverse

        // turning scales with speed (simple car feel)
        float turnAmount = TURN_SPEED * (carSpeed >= 0 ? 1.0f : -1.0f) * deltaTime;
        carYaw += steerInput * turnAmount;

        // update car position
        glm::vec3 forward = glm::vec3(sin(glm::radians(carYaw)), 0.0f, cos(glm::radians(carYaw)));
        // carPos += forward * carSpeed * deltaTime;
        glm::vec3 nextPos = carPos + forward * carSpeed * deltaTime;

        // check wall collision
        if (!checkCollision(nextPos, carSize, wallPos, wallSize)) {
            carPos = nextPos; // safe to move
        } else {
            // simple reaction: stop movement
            carSpeed = 0.0f;
            
            // optional: slide along wall
            // carPos += glm::vec3(0.0f, 0.0f, 0.0f); // or adjust direction
        }


        // ---- update camera: place behind car and lerp for smoothing ----
        glm::vec3 desiredCameraPos = carPos - forward * 8.0f + glm::vec3(0.0f, 3.0f, 0.0f);
        // smooth interpolate
        cameraPos = glm::mix(cameraPos, desiredCameraPos, glm::clamp(cameraSmoothSpeed * deltaTime, 0.0f, 1.0f));
        glm::vec3 cameraTarget = carPos + glm::vec3(0.0f, 1.0f, 0.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraTarget, cameraUp);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 200.0f);

        // ---- render ----
        glClearColor(0.05f, 0.05f, 0.07f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 1) draw floor (textured)
        floorShader.use();
        glm::mat4 floorModel = glm::mat4(1.0f);
        floorShader.setMat4("projection", projection);
        floorShader.setMat4("view", view);
        floorShader.setMat4("model", floorModel);
        floorShader.setVec3("lightPos", lightPos);
        floorShader.setVec3("viewPos", cameraPos);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, floorTex);
        floorShader.setInt("floorTexture", 0);

        glBindVertexArray(floorVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        // 2) draw car model
        modelShader.use();
        modelShader.setMat4("projection", projection);
        modelShader.setMat4("view", view);
        glm::mat4 carModelMat = glm::mat4(1.0f);
        carModelMat = glm::translate(carModelMat, carPos + glm::vec3(0.0f, 0.1f, 0.0f)); // small lift
        carModelMat = glm::rotate(carModelMat, glm::radians(90.0f), glm::vec3(0, 1, 0));
        carModelMat = glm::rotate(carModelMat, glm::radians(carYaw), glm::vec3(0,1,0));
        carModelMat = glm::scale(carModelMat, glm::vec3(0.6f)); // adjust to taste
        modelShader.setMat4("model", carModelMat);
        // if your model shader needs camera pos or lights, set them here:
        modelShader.setVec3("viewPos", cameraPos);
        modelShader.setVec3("lightPos", lightPos);
        carModel.Draw(modelShader);

        // 3) draw skybox (last)
        glDepthFunc(GL_LEQUAL);
        skyboxShader.use();
        // remove translation from the view matrix
        glm::mat4 skyView = glm::mat4(glm::mat3(view));
        skyboxShader.setMat4("view", skyView);
        skyboxShader.setMat4("projection", projection);
        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glDepthFunc(GL_LESS);

        // 1.5) draw wall
        floorShader.use();
        glm::mat4 wallModel = glm::mat4(1.0f);
        floorShader.setMat4("projection", projection);
        floorShader.setMat4("view", view);
        floorShader.setMat4("model", wallModel);
        floorShader.setVec3("lightPos", lightPos);
        floorShader.setVec3("viewPos", cameraPos);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, floorTex); // reuse floor texture for simplicity
        floorShader.setInt("floorTexture", 0);

        glBindVertexArray(wallVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);


        // swap and poll
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // cleanup (optional)
    glDeleteVertexArrays(1, &floorVAO);
    glDeleteBuffers(1, &floorVBO);
    glDeleteBuffers(1, &floorEBO);
    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteBuffers(1, &skyboxVBO);

    glfwTerminate();
    return 0;
}

// ----- callbacks and helpers -----
void framebuffer_size_callback(GLFWwindow* /*window*/, int width, int height)
{
    glViewport(0, 0, width, height);
}

void key_callback(GLFWwindow* /*window*/, int key, int /*scancode*/, int action, int /*mods*/)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) glfwSetWindowShouldClose(glfwGetCurrentContext(), true);
    if (key >= 0 && key < 1024)
    {
        if (action == GLFW_PRESS) keys[key] = true;
        else if (action == GLFW_RELEASE) keys[key] = false;
    }
}

unsigned int loadTexture(const char *path)
{
    stbi_set_flip_vertically_on_load(true);
    unsigned int textureID;
    glGenTextures(1, &textureID);
    int width, height, nrComponents;
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format = (nrComponents == 1) ? GL_RED : (nrComponents == 3) ? GL_RGB : GL_RGBA;
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        // texture params
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Failed to load texture at path: " << path << std::endl;
        stbi_image_free(data);
        return 0;
    }
    return textureID;
}

unsigned int loadCubemap(std::vector<std::string> faces)
{
    stbi_set_flip_vertically_on_load(false); // cubemaps usually not flipped
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data)
        {
            GLenum format = (nrChannels == 3) ? GL_RGB : GL_RGBA;
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}
