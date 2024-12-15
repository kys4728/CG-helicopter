#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_IMPLEMENTATION
#include <iostream>
#include <vector>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <gl/glm/glm.hpp>
#include <gl/glm/gtc/matrix_transform.hpp>
#include <gl/glm/gtc/type_ptr.hpp>
#include "tiny_obj_loader.h"
#include "stb_image.h"
#include <chrono>

#define TINYOBJLOADER_IMPLEMENTATION
std::chrono::steady_clock::time_point lastRKeyTime;

GLuint shaderID;
GLuint objVAOs[20], objVBOs[20], objEBOs[20]; // 20���� ����
GLuint objIndexCounts[20];
std::vector<int> objMaterialIndices[20]; // �� OBJ ������ ���� �ε��� �迭
std::vector<tinyobj::material_t> globalMaterials[20];

glm::vec3 cameraPos = glm::vec3(0.0f, 4.0f, -5.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, 1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
float cameraSpeed = 0.05f;

bool isAmbientLightOn = true; // �ֺ� ���� ���� (�ʱⰪ: ����)
float lightIntensity = 4.0f; // �ʱ� ���� ���� (�⺻��: 1.0f)

bool isLightOrbiting = false; // ������ ���� ������ ����
float lightOrbitAngle = 0.0f; // ������ ȸ�� ���� (����)
bool isOrbitingClockwise = true; // ���� ���� ���� (true: �ð� ����, false: �ݽð� ����)
float lightOrbitRadius = 3.0f; // �ʱ� �ݰ� ����

glm::vec3 lightInitialPos = glm::vec3(0.0f, 3.0f, 3.0f); // �ʱ� ���� ��ġ
glm::vec3 lightPos = lightInitialPos; // ���� ���� ��ġ


glm::vec3 lightColors[] = {
    glm::vec3(1.0f, 1.0f, 1.0f),
    glm::vec3(1.0f, 0.0f, 0.0f),
    glm::vec3(0.0f, 1.0f, 0.0f), // �ʷ�
    glm::vec3(0.0f, 0.0f, 1.0f)  // �Ķ�
};

int currentLightColorIndex = 0; // ���� ���� �ε���
int lightPositionIndex = 0; // �ʱ� ��ġ �ε��� (0~3)
float lightHeight = 3.0f;   // ������ y�� ����

GLuint textures[6]; // ����ü �ؽ�ó
GLuint pyramidTextures[5]; // �簢�� �ؽ�ó
GLuint bgtextures[6];
GLuint helitextures[1];
GLuint wingtextures[1];
GLuint groundtextures[1];
GLuint cloudtextures[1];

float rotationX = 0.0f; // x�� ȸ�� ����
float rotationY = 0.0f; // y�� ȸ�� ����

float lightScale = 1.5f; // �⺻��
bool isWingAutoRotate = false; // ������ �ڵ� ȸ�� ����
float wingRotationSpeed = 0.0f; // ������ ���� ȸ�� �ӵ�
float wingAcceleration = 0.0001f; // ������ ���ӵ� (�ʱⰪ)

bool isHelicopterControlEnabled = false;

// �︮������ y�� ��ġ ����
float heliY = 0.0f; // �ʱ� ��ġ ����
float heliX = 0.0f; // �︮������ �ʱ� X�� ��ġ
float heliZ = 0.0f; // �ʱ� Z�� ��ġ

bool isRKeyPressed = false;
float tiltAngle = 0.0f; // �︮������ ���� ����
float rollAngle = 0.0f; // �︮������ Z�� ���� ����

bool isFirstPersonView = false; // �ʱⰪ: 3��Ī ����

const float HELI_BOUNDARY_MIN_XZ = -35.0f; // X, Z �ּ� ���
const float HELI_BOUNDARY_MAX_XZ = 35.0f;  // X, Z �ִ� ���
const float HELI_BOUNDARY_MIN_Y = 0.0f;    // Y �ּ� ���
const float HELI_BOUNDARY_MAX_Y = 28.0f;   // Y �ִ� ���


char* filetobuf(const char* file) {
    FILE* fptr;
    long length;
    char* buf;
    fptr = fopen(file, "rb");
    if (!fptr) return NULL;
    fseek(fptr, 0, SEEK_END);
    length = ftell(fptr);
    buf = (char*)malloc(length + 1);
    fseek(fptr, 0, SEEK_SET);
    fread(buf, length, 1, fptr);
    fclose(fptr);
    buf[length] = 0;
    return buf;
}

void make_vertexShaders() {
    char* vertexSource = filetobuf("teamvt.glsl");
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, (const GLchar**)&vertexSource, 0);
    glCompileShader(vertexShader);

    GLint result;
    GLchar errorLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &result);
    if (!result) {
        glGetShaderInfoLog(vertexShader, 512, NULL, errorLog);
        std::cerr << "ERROR: Vertex shader compile failed\n" << errorLog << std::endl;
        exit(-1);
    }
    shaderID = glCreateProgram();
    glAttachShader(shaderID, vertexShader);
    free(vertexSource);
}

void make_fragmentShaders() {
    char* fragmentSource = filetobuf("teamfr.glsl");
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, (const GLchar**)&fragmentSource, 0);
    glCompileShader(fragmentShader);

    GLint result;
    GLchar errorLog[512];
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &result);
    if (!result) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, errorLog);
        std::cerr << "ERROR: Fragment shader compile failed\n" << errorLog << std::endl;
        exit(-1);
    }
    glAttachShader(shaderID, fragmentShader);
    glLinkProgram(shaderID);

    glDeleteShader(fragmentShader);
    free(fragmentSource);
}

GLuint make_shaderProgram() {
    glLinkProgram(shaderID);
    glUseProgram(shaderID);
    return shaderID;
}

void loadObj(const std::string& filename,
    std::vector<float>& vertices,
    std::vector<unsigned int>& indices,
    std::vector<int>& materialIndices,
    std::vector<tinyobj::material_t>& materials) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename.c_str())) {
        std::cerr << "Failed to load .obj file: " << err << std::endl;
        return;
    }

    std::vector<glm::vec3> tempNormals(attrib.vertices.size() / 3, glm::vec3(0.0f));

    for (const auto& shape : shapes) {
        for (size_t i = 0; i < shape.mesh.indices.size(); i += 3) {
            tinyobj::index_t idx0 = shape.mesh.indices[i];
            tinyobj::index_t idx1 = shape.mesh.indices[i + 1];
            tinyobj::index_t idx2 = shape.mesh.indices[i + 2];

            // ���� ��ǥ
            glm::vec3 v0(attrib.vertices[3 * idx0.vertex_index + 0],
                attrib.vertices[3 * idx0.vertex_index + 1],
                attrib.vertices[3 * idx0.vertex_index + 2]);
            glm::vec3 v1(attrib.vertices[3 * idx1.vertex_index + 0],
                attrib.vertices[3 * idx1.vertex_index + 1],
                attrib.vertices[3 * idx1.vertex_index + 2]);
            glm::vec3 v2(attrib.vertices[3 * idx2.vertex_index + 0],
                attrib.vertices[3 * idx2.vertex_index + 1],
                attrib.vertices[3 * idx2.vertex_index + 2]);

            // ���� ���
            glm::vec3 edge1 = v1 - v0;
            glm::vec3 edge2 = v2 - v0;
            glm::vec3 faceNormal = glm::normalize(glm::cross(edge1, edge2));

            tempNormals[idx0.vertex_index] += faceNormal;
            tempNormals[idx1.vertex_index] += faceNormal;
            tempNormals[idx2.vertex_index] += faceNormal;

            // �ؽ�ó ��ǥ
            glm::vec2 uv0(0.0f, 0.0f);
            glm::vec2 uv1(0.0f, 0.0f);
            glm::vec2 uv2(0.0f, 0.0f);

            if (idx0.texcoord_index >= 0) {
                uv0.x = attrib.texcoords[2 * idx0.texcoord_index + 0];
                uv0.y = 1.0f - attrib.texcoords[2 * idx0.texcoord_index + 1];
            }
            if (idx1.texcoord_index >= 0) {
                uv1.x = attrib.texcoords[2 * idx1.texcoord_index + 0];
                uv1.y = 1.0f - attrib.texcoords[2 * idx1.texcoord_index + 1];
            }
            if (idx2.texcoord_index >= 0) {
                uv2.x = attrib.texcoords[2 * idx2.texcoord_index + 0];
                uv2.y = 1.0f - attrib.texcoords[2 * idx2.texcoord_index + 1];
            }

            // ���� ������ �߰� (v0, uv0)
            vertices.push_back(v0.x);
            vertices.push_back(v0.y);
            vertices.push_back(v0.z);
            vertices.push_back(faceNormal.x);
            vertices.push_back(faceNormal.y);
            vertices.push_back(faceNormal.z);
            vertices.push_back(uv0.x);
            vertices.push_back(uv0.y);

            // ���� ������ �߰� (v1, uv1)
            vertices.push_back(v1.x);
            vertices.push_back(v1.y);
            vertices.push_back(v1.z);
            vertices.push_back(faceNormal.x);
            vertices.push_back(faceNormal.y);
            vertices.push_back(faceNormal.z);
            vertices.push_back(uv1.x);
            vertices.push_back(uv1.y);

            // ���� ������ �߰� (v2, uv2)
            vertices.push_back(v2.x);
            vertices.push_back(v2.y);
            vertices.push_back(v2.z);
            vertices.push_back(faceNormal.x);
            vertices.push_back(faceNormal.y);
            vertices.push_back(faceNormal.z);
            vertices.push_back(uv2.x);
            vertices.push_back(uv2.y);

            // �ε��� �߰�
            indices.push_back(indices.size());
            indices.push_back(indices.size());
            indices.push_back(indices.size());

            // ���� �ε��� �߰�
            if (i / 3 < shape.mesh.material_ids.size()) {
                materialIndices.push_back(shape.mesh.material_ids[i / 3]);
            }
            else {
                materialIndices.push_back(-1); // �⺻��
            }
        }
    }

    // ���� ���� ����ȭ
    for (size_t i = 0; i < tempNormals.size(); ++i) {
        tempNormals[i] = glm::normalize(tempNormals[i]);
        if (glm::dot(tempNormals[i], glm::vec3(0.0f, 0.0f, 1.0f)) < 0) {
            tempNormals[i] = -tempNormals[i]; // ����
        }
    }
}



void InitObj(const std::string& filename, int objIndex) {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    std::vector<int> materialIndices;

    loadObj(filename, vertices, indices, materialIndices, globalMaterials[objIndex]);

    objMaterialIndices[objIndex] = materialIndices;
    objIndexCounts[objIndex] = static_cast<GLuint>(indices.size());

    glGenVertexArrays(1, &objVAOs[objIndex]);
    glBindVertexArray(objVAOs[objIndex]);

    glGenBuffers(1, &objVBOs[objIndex]);
    glBindBuffer(GL_ARRAY_BUFFER, objVBOs[objIndex]);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);

    glGenBuffers(1, &objEBOs[objIndex]);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, objEBOs[objIndex]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
}


GLuint lightVAO, lightVBO, lightEBO;
GLuint lightIndexCount;

void InitLightObj(const std::string& filename) {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    std::vector<int> materialIndices;

    std::vector<tinyobj::material_t> materials;
    loadObj(filename, vertices, indices, materialIndices, materials);

    lightIndexCount = static_cast<GLuint>(indices.size());

    glGenVertexArrays(1, &lightVAO);
    glBindVertexArray(lightVAO);

    GLuint lightVBO, lightEBO;
    glGenBuffers(1, &lightVBO);
    glBindBuffer(GL_ARRAY_BUFFER, lightVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);

    glGenBuffers(1, &lightEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, lightEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

GLuint loadTexture(const char* path) {
    GLuint textureID;
    glGenTextures(1, &textureID);

    int width, height, nrChannels;
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0); ///STBI_rgb �Ǵ� 0
    if (data) {
        std::cout << "Loaded texture: " << path << " (" << width << "x" << height << ", " << nrChannels << " channels)" << std::endl;
    }
    else {
        std::cerr << "Failed to load texture: " << path << std::endl;
    }
    if (data) {
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // S�� �ݺ�
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); // T�� �ݺ�


        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    else {
        std::cerr << "Failed to load texture: " << path << std::endl;
    }
    stbi_image_free(data);
    return textureID;
}
void applyMaterial(int objIndex, int materialIndex) {
    if (!globalMaterials[objIndex].empty() && materialIndex >= 0) {
        const auto& material = globalMaterials[objIndex][materialIndex];

        glm::vec3 ambient = glm::vec3(material.ambient[0], material.ambient[1], material.ambient[2]);
        glm::vec3 diffuse = glm::vec3(material.diffuse[0], material.diffuse[1], material.diffuse[2]);
        glm::vec3 specular = glm::vec3(material.specular[0], material.specular[1], material.specular[2]);

        if (glm::length(ambient) == 0.0f) ambient = glm::vec3(0.2f); // �⺻�� ����
        if (glm::length(diffuse) == 0.0f) diffuse = glm::vec3(0.8f); // �⺻ Ȯ���
        if (glm::length(specular) == 0.0f) specular = glm::vec3(1.0f); // �⺻ �ݻ籤

        glUniform3fv(glGetUniformLocation(shaderID, "material.ambient"), 1, glm::value_ptr(ambient));
        glUniform3fv(glGetUniformLocation(shaderID, "material.diffuse"), 1, glm::value_ptr(diffuse));
        glUniform3fv(glGetUniformLocation(shaderID, "material.specular"), 1, glm::value_ptr(specular));
        glUniform1f(glGetUniformLocation(shaderID, "material.shininess"), material.shininess * 128.0f);
    }
    else {
        // �⺻ ����
        glm::vec3 defaultAmbient = glm::vec3(0.2f, 0.2f, 0.2f);
        glm::vec3 defaultDiffuse = glm::vec3(0.8f, 0.8f, 0.8f);
        glm::vec3 defaultSpecular = glm::vec3(1.0f, 1.0f, 1.0f);

        glUniform3fv(glGetUniformLocation(shaderID, "material.ambient"), 1, glm::value_ptr(defaultAmbient));
        glUniform3fv(glGetUniformLocation(shaderID, "material.diffuse"), 1, glm::value_ptr(defaultDiffuse));
        glUniform3fv(glGetUniformLocation(shaderID, "material.specular"), 1, glm::value_ptr(defaultSpecular));
        glUniform1f(glGetUniformLocation(shaderID, "material.shininess"), 32.0f);
    }
}

void applyTextures() {

    // ����ü �ؽ�ó (6��)
    textures[0] = loadTexture("platform.png"); // �ո�

    bgtextures[0] = loadTexture("sky.png");
    bgtextures[1] = loadTexture("sky.png");
    bgtextures[2] = loadTexture("sky.png");
    bgtextures[3] = loadTexture("sky.png");
    bgtextures[4] = loadTexture("sky.png");
    bgtextures[5] = loadTexture("sky.png");

    helitextures[0] = loadTexture("helitexture.png");
    wingtextures[0] = loadTexture("wing.png");
    groundtextures[0] = loadTexture("ground.png");
    cloudtextures[0] = loadTexture("cloud.png");
}

// �︮���͸� ����ٴϴ� ī�޶� ������Ʈ �Լ�
void updateCameraPosition() {
    float distanceBehindHelicopter = 10.0f; // �︮���� ���� �Ÿ�
    float heightAboveHelicopter = 5.0f;     // �︮���� ���� ����

    if (isFirstPersonView) {
        // 1��Ī ����: �︮���� ���ʰ� �ణ ���� ��ġ
        cameraPos = glm::vec3(
            heliX,
            heliY,
            heliZ
        );

        // 1��Ī ī�޶� ����: �︮������ ���� ������ ����
        cameraFront = glm::vec3(
            sin(rotationY), // x�� ����
            0.0f,           // y�� ����
            cos(rotationY) // z�� ���� (�ݴ� ���� ���)
        );

        cameraPos += cameraFront * 2.0f;
        cameraUp = glm::vec3(0.0f, 1.0f, 0.0f); // ���� ���� ����
    }
    else {
        // 3��Ī ����: �︮���͸� ����ٴϴ� ī�޶�
        glm::vec3 helicopterPosition = glm::vec3(heliX, heliY, heliZ);

        float offsetX = distanceBehindHelicopter * sin(rotationY);
        float offsetZ = distanceBehindHelicopter * cos(rotationY);

        cameraPos = glm::vec3(
            heliX - offsetX,
            heliY + heightAboveHelicopter,
            heliZ - offsetZ
        );

        cameraFront = glm::normalize(helicopterPosition - cameraPos); // �︮���͸� �ٶ󺸴� ����
        cameraUp = glm::vec3(0.0f, 1.0f, 0.0f); // ���� ���� ����
    }
}

void enforceHelicopterBoundaries() {
    // X�� ��� Ȯ��
    if (heliX < HELI_BOUNDARY_MIN_XZ) heliX = HELI_BOUNDARY_MIN_XZ;
    if (heliX > HELI_BOUNDARY_MAX_XZ) heliX = HELI_BOUNDARY_MAX_XZ;

    // Z�� ��� Ȯ��
    if (heliZ < HELI_BOUNDARY_MIN_XZ) heliZ = HELI_BOUNDARY_MIN_XZ;
    if (heliZ > HELI_BOUNDARY_MAX_XZ) heliZ = HELI_BOUNDARY_MAX_XZ;

    // Y�� ��� Ȯ��
    if (heliY < HELI_BOUNDARY_MIN_Y) heliY = HELI_BOUNDARY_MIN_Y;
    if (heliY > HELI_BOUNDARY_MAX_Y) heliY = HELI_BOUNDARY_MAX_Y;
}
void drawScene() {
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    glUseProgram(shaderID);
    updateCameraPosition();
    // ī�޶� �� ���� ��� ����
    glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
    glUniformMatrix4fv(glGetUniformLocation(shaderID, "view"), 1, GL_FALSE, glm::value_ptr(view));

    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
    glUniformMatrix4fv(glGetUniformLocation(shaderID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));


    // ���� ����
    glm::vec3 lightAmbient = lightColors[currentLightColorIndex] * lightIntensity;
    glm::vec3 lightDiffuse = lightColors[currentLightColorIndex] * lightIntensity;
    glm::vec3 lightSpecular = lightColors[currentLightColorIndex] * lightIntensity;

    glUniform3fv(glGetUniformLocation(shaderID, "light.position"), 1, glm::value_ptr(lightPos));
    glUniform3fv(glGetUniformLocation(shaderID, "light.ambient"), 1, glm::value_ptr(lightAmbient));
    glUniform3fv(glGetUniformLocation(shaderID, "light.diffuse"), 1, glm::value_ptr(lightDiffuse));
    glUniform3fv(glGetUniformLocation(shaderID, "light.specular"), 1, glm::value_ptr(lightSpecular));
    glUniform1i(glGetUniformLocation(shaderID, "uAmbientLightOn"), isAmbientLightOn);
    glUniform3fv(glGetUniformLocation(shaderID, "viewPos"), 1, glm::value_ptr(cameraPos));
    glUniform1f(glGetUniformLocation(shaderID, "uvScale"), 1.0f);

    glDepthMask(GL_FALSE);
    glBindVertexArray(objVAOs[0]);
    for (int i = 0; i < 6; ++i) {
        glm::mat4 bgModel = glm::mat4(1.0f);
        bgModel = glm::scale(bgModel, glm::vec3(-30.0f));
        glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, glm::value_ptr(bgModel));
        applyMaterial(0, objMaterialIndices[0][i]);
        glBindTexture(GL_TEXTURE_2D, bgtextures[i]);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)(i * 6 * sizeof(unsigned int)));
    }
    glDepthMask(GL_TRUE);


    glBindVertexArray(objVAOs[0]);
    glm::mat4 bgModel = glm::mat4(1.0f);
    bgModel = glm::translate(bgModel, glm::vec3(0.0f, -0.9f, 0.0f));
    bgModel = glm::scale(bgModel, glm::vec3(2.0f, 0.2f, 2.0f));

    glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, glm::value_ptr(bgModel));
    applyMaterial(4, 0);
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glDrawElements(GL_TRIANGLES, objIndexCounts[2], GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);


    glBindVertexArray(objVAOs[0]);
    glm::mat4 platModel = glm::mat4(1.0f);
    platModel = glm::translate(platModel, glm::vec3(0.0f, -0.9f, 0.0f));
    platModel = glm::scale(platModel, glm::vec3(30.0f, 0.0f, 30.0f));

    glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, glm::value_ptr(platModel));
    applyMaterial(4, 0);
    glBindTexture(GL_TEXTURE_2D, groundtextures[0]);
    glDrawElements(GL_TRIANGLES, objIndexCounts[2], GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glBindVertexArray(objVAOs[0]);
    glm::mat4 clModel = glm::mat4(1.0f);
    clModel = glm::translate(clModel, glm::vec3(3.0f, 10.0f, 15.0f));
    clModel = glm::scale(clModel, glm::vec3(1.0f, 1.0f, 1.0f));

    glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, glm::value_ptr(clModel));
    applyMaterial(4, 0);
    glBindTexture(GL_TEXTURE_2D, cloudtextures[0]);
    glDrawElements(GL_TRIANGLES, objIndexCounts[2], GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glBindVertexArray(objVAOs[0]);
    glm::mat4 cl2Model = glm::mat4(1.0f);
    cl2Model = glm::translate(cl2Model, glm::vec3(3.0f, 9.5f, 15.0f));
    cl2Model = glm::scale(cl2Model, glm::vec3(1.0f, 1.0f, 1.0f));

    glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, glm::value_ptr(cl2Model));
    applyMaterial(4, 0);
    glBindTexture(GL_TEXTURE_2D, cloudtextures[0]);
    glDrawElements(GL_TRIANGLES, objIndexCounts[2], GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glBindVertexArray(objVAOs[0]);
    glm::mat4 cl3Model = glm::mat4(1.0f);
    cl3Model = glm::translate(cl3Model, glm::vec3(3.5f, 10.5f, 15.5f));
    cl3Model = glm::scale(cl3Model, glm::vec3(1.0f, 1.0f, 1.0f));

    glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, glm::value_ptr(cl3Model));
    applyMaterial(4, 0);
    glBindTexture(GL_TEXTURE_2D, cloudtextures[0]);
    glDrawElements(GL_TRIANGLES, objIndexCounts[2], GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glBindVertexArray(objVAOs[1]);
    // �︮���� �׸���
    glm::mat4 heliModel = glm::mat4(1.0f);

    // �︮���� ��ġ ����
    heliModel = glm::translate(heliModel, glm::vec3(heliX, heliY, heliZ));

    // �︮���� Y�� ȸ�� ����
    heliModel = glm::rotate(heliModel, rotationY, glm::vec3(0.0f, 1.0f, 0.0f));
    // �︮���� X�� ���� ����
    heliModel = glm::rotate(heliModel, tiltAngle, glm::vec3(1.0f, 0.0f, 0.0f));
    // �︮���� Z�� ���� ����
    heliModel = glm::rotate(heliModel, rollAngle, glm::vec3(0.0f, 0.0f, 1.0f));

    // ũ�� ����
    heliModel = glm::scale(heliModel, glm::vec3(1.0f));

    // ���̴��� �� ��� ����
    glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, glm::value_ptr(heliModel));
    applyMaterial(4, 0); // �︮���� ���� ����
    glBindTexture(GL_TEXTURE_2D, helitextures[0]);
    // �︮���� �׸���
    glDrawElements(GL_TRIANGLES, objIndexCounts[1], GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);

    // ���� �׸���
    glBindVertexArray(objVAOs[2]);
    glm::mat4 wingModel = heliModel; // �︮���� �� ����� ������� ��
    wingModel = glm::translate(heliModel, glm::vec3(0.0f, 0.1f, 0.0f));
    // ������ ���� ȸ�� (�︮���Ϳ� �Բ� ȸ���ϸ鼭 �߰����� ȸ��)
    static float wingRotationAngle = 0.0f;
    wingRotationAngle += 0.0001f; // ������ ȸ�� �ӵ� (������ �����Ͽ� �ݽð� ����))
    if (isWingAutoRotate) {
        wingRotationAngle += wingRotationSpeed; // ���� �ӵ��� ȸ��
        if (wingRotationAngle > 360.0f) {
            wingRotationAngle -= 360.0f;
        }
    }

    // ������ ȸ�� ��� ����
    wingModel = glm::rotate(wingModel, glm::radians(wingRotationAngle), glm::vec3(0.0f, 1.0f, 0.0f));

    // ������ ũ�� ���� (�ʿ信 ���� ����)
    wingModel = glm::scale(wingModel, glm::vec3(0.8f));
    // ���̴��� �� ��� ����
    glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, glm::value_ptr(wingModel));
    applyMaterial(4, 0); // ���� ���� ����
    glBindTexture(GL_TEXTURE_2D, wingtextures[0]);
    // ���� �׸���
    glDrawElements(GL_TRIANGLES, objIndexCounts[2], GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);

    // Ŭ������ ���� ��ǥ���� AABB ���� (�� ���������� ũ��)
    glm::vec3 cloudLocalMin(-0.5f, -0.5f, -0.5f); // Ŭ���� ���� �ּ� ��ǥ
    glm::vec3 cloudLocalMax(0.5f, 0.5f, 0.5f);   // Ŭ���� ���� �ִ� ��ǥ

    // Ŭ���� ���� ���������� AABB ���
    glm::vec3 cloudWorldMin = glm::vec3(clModel * glm::vec4(cloudLocalMin, 1.0f));
    glm::vec3 cloudWorldMax = glm::vec3(clModel * glm::vec4(cloudLocalMax, 1.0f));
    // �︮������ ���� ��ǥ���� AABB ���� (�� ���������� ũ��)
    glm::vec3 heliLocalMin(-0.5f, -0.5f, -0.5f); // �︮���� ���� �ּ� ��ǥ
    glm::vec3 heliLocalMax(0.5f, 0.5f, 0.5f);   // �︮���� ���� �ִ� ��ǥ

    // �︮���� ���� ���������� AABB ���
    glm::vec3 heliWorldMin = glm::vec3(heliModel * glm::vec4(heliLocalMin, 1.0f));
    glm::vec3 heliWorldMax = glm::vec3(heliModel * glm::vec4(heliLocalMax, 1.0f));
    // �浹 ���� (AABB �浹 üũ)
    bool isColliding = (heliWorldMin.x <= cloudWorldMax.x && heliWorldMax.x >= cloudWorldMin.x) &&
        (heliWorldMin.y <= cloudWorldMax.y && heliWorldMax.y >= cloudWorldMin.y) &&
        (heliWorldMin.z <= cloudWorldMax.z && heliWorldMax.z >= cloudWorldMin.z);

    if (isColliding) {
        std::cout << "Helicopter reached the cloud. Exiting program." << std::endl;
        exit(0); // ���α׷� ����
    }


    glutSwapBuffers();
}


void display() {
    drawScene();
}

GLvoid Reshape(int w, int h) {
    glViewport(0, 0, w, h);
}

GLvoid KeyBoard(unsigned char key, int x, int y) {
    const float movementStep = 0.1f; // �̵� ����
    const float rotationStep = glm::radians(3.0f); // ȸ�� ���� (5��)

    auto currentTime = std::chrono::steady_clock::now(); // ���� �ð� ����

    switch (key) {
    case 'q':
        exit(0); // ���α׷� ����
        break;

    case 32: // �����̽��� �Է½� ���� �ڵ� ȸ�� ��� �� ���� Ȱ��ȭ ����
        isWingAutoRotate = !isWingAutoRotate;
        lastRKeyTime = currentTime;
        isRKeyPressed = true;
        isHelicopterControlEnabled = false;
        std::cout << "Helicopter control will be enabled after 8 seconds." << std::endl;
        if (isWingAutoRotate) {
            wingRotationSpeed = 0.0f; // �ʱ� �ӵ� ����
            std::cout << "Wing auto rotation enabled with acceleration." << std::endl;
        }
        else {
            std::cout << "Wing auto rotation disabled." << std::endl;
        }
        break;
    case 'v': // V Ű�� ������ �� ���� ��ȯ
        isFirstPersonView = !isFirstPersonView;
        if (isFirstPersonView) {
            std::cout << "First-person view enabled." << std::endl;
        }
        else {
            std::cout << "Third-person view enabled." << std::endl;
        }
        break;
        // ���� �̵�
    case 'w':
        enforceHelicopterBoundaries();
        if (!isRKeyPressed) {
            std::cout << "Cannot move. Press 'r' to enable helicopter control." << std::endl;
        }
        else if (isHelicopterControlEnabled) {
            heliY += movementStep;
            std::cout << "Helicopter moved up. Current Y position: " << heliY << std::endl;
        }
        else {
            auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastRKeyTime).count();
            if (elapsedTime >= 8) {
                isHelicopterControlEnabled = true;
                heliY += movementStep;
                std::cout << "Helicopter moved up. Current Y position: " << heliY << std::endl;
            }
            else {
                std::cout << "Cannot move. Please wait for " << (8 - elapsedTime) << " seconds." << std::endl;
            }
        }
        break;

        // �Ʒ��� �̵�
    case 's':
        enforceHelicopterBoundaries();
        if (!isRKeyPressed) {
            std::cout << "Cannot move. Press 'r' to enable helicopter control." << std::endl;
        }
        else if (isHelicopterControlEnabled) {
            heliY -= movementStep;
            std::cout << "Helicopter moved down. Current Y position: " << heliY << std::endl;
        }
        else {
            auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastRKeyTime).count();
            if (elapsedTime >= 8) {
                isHelicopterControlEnabled = true;
                heliY -= movementStep;
                std::cout << "Helicopter moved down. Current Y position: " << heliY << std::endl;
            }
            else {
                std::cout << "Cannot move. Please wait for " << (15 - elapsedTime) << " seconds." << std::endl;
            }
        }
        break;
    case 'a': // ���� ȸ��
    case 'd': // ������ ȸ��
        if (!isRKeyPressed) {
            std::cout << "Cannot rotate. Press 'r' to enable helicopter control." << std::endl;
        }
        else if (isHelicopterControlEnabled) {
            if (key == 'a') rotationY += rotationStep;
            if (key == 'd') rotationY -= rotationStep;
            std::cout << "Helicopter rotated. Current Y rotation: " << glm::degrees(rotationY) << " degrees." << std::endl;
        }
        else {
            auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastRKeyTime).count();
            if (elapsedTime >= 8) {
                isHelicopterControlEnabled = true;
                if (key == 'a') rotationY += rotationStep;
                if (key == 'd') rotationY -= rotationStep;
                std::cout << "Helicopter rotated. Current Y rotation: " << glm::degrees(rotationY) << " degrees." << std::endl;
            }
            else {
                std::cout << "Cannot rotate. Please wait for " << (8 - elapsedTime) << " seconds." << std::endl;
            }
        }
        break;
    }
    // ī�޶� ��ġ ������Ʈ
    updateCameraPosition();

    // ȭ�� ���� ��û
    glutPostRedisplay();
}

GLvoid SpecialKey(int key, int x, int y) {
    const float movementStep = 0.05f; // �̵� ����

    // �õ� ���� Ȯ��
    if (!isWingAutoRotate) {
        std::cout << "Cannot move. Start the helicopter by pressing the spacebar." << std::endl;
        return;
    }

    auto currentTime = std::chrono::steady_clock::now(); // ���� �ð� ����
    auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastRKeyTime).count();

    if (!isHelicopterControlEnabled && elapsedTime < 8) {
        std::cout << "Cannot move. Please wait for " << (8 - elapsedTime) << " seconds." << std::endl;
        return;
    }

    if (elapsedTime >= 8) {
        isHelicopterControlEnabled = true; // ���� Ȱ��ȭ
    }

    // ���� ȸ�� ������ ���� �̵� ���� ���
    glm::vec3 forwardDir = glm::vec3(sin(rotationY), 0.0f, cos(rotationY)); // ���� ����
    glm::vec3 rightDir = glm::vec3(cos(rotationY), 0.0f, -sin(rotationY));  // ������ ����

    switch (key) {
    case GLUT_KEY_UP: // ���� ȭ��ǥ �Է� �� (������ ����)
        heliX += forwardDir.x * movementStep;
        heliZ += forwardDir.z * movementStep;
        tiltAngle = glm::radians(30.0f); // ���� �� ������ ����
        break;

    case GLUT_KEY_DOWN: // �Ʒ��� ȭ��ǥ �Է� �� (�ڷ� ����)
        heliX -= forwardDir.x * movementStep;
        heliZ -= forwardDir.z * movementStep;
        tiltAngle = glm::radians(-30.0f); // ���� �� �ڷ� ����
        break;

    case GLUT_KEY_RIGHT: // ������ ȭ��ǥ �Է� �� (������ �̵�)
        heliX -= rightDir.x * movementStep;
        heliZ -= rightDir.z * movementStep;
        rollAngle = glm::radians(30.0f); // ���������� ����
        break;

    case GLUT_KEY_LEFT: // ���� ȭ��ǥ �Է� �� (���� �̵�)
        heliX += rightDir.x * movementStep;
        heliZ += rightDir.z * movementStep;
        rollAngle = glm::radians(-30.0f); // �������� ����
        break;

    default:
        tiltAngle = glm::radians(0.0f); // ���� �ʱ�ȭ
        rollAngle = glm::radians(0.0f);
        break;
    }

    // �︮���� ���� ��ġ ���
    std::cout << "Helicopter position: (" << heliX << ", " << heliY << ", " << heliZ << ")" << std::endl;

    // ��� �浹 ó��
    enforceHelicopterBoundaries();

    // ī�޶� ��ġ ������Ʈ
    updateCameraPosition();

    // ȭ�� ���� ��û
    glutPostRedisplay();
}

GLvoid SpecialKeyUp(int key, int x, int y) {
    switch (key) {
    case GLUT_KEY_UP:
    case GLUT_KEY_DOWN:
        tiltAngle = 0.0f; // ����/���� ���� �ʱ�ȭ
        break;
    case GLUT_KEY_LEFT:
    case GLUT_KEY_RIGHT:
        rollAngle = 0.0f; // �¿� ���� �ʱ�ȭ
        break;
    default:
        break;
    }
    glutPostRedisplay(); // ȭ�� ���� ��û
}
void updateHelicopterState() {
    const float returnSpeed = 0.0001f; // ���� ���� �ӵ�

    // X�� ���� ����
    if (tiltAngle > 0.0f) {
        tiltAngle -= returnSpeed;
        if (tiltAngle < 0.0f) tiltAngle = 0.0f;
    }
    else if (tiltAngle < 0.0f) {
        tiltAngle += returnSpeed;
        if (tiltAngle > 0.0f) tiltAngle = 0.0f;
    }

    // Z�� ���� ����
    if (rollAngle > 0.0f) {
        rollAngle -= returnSpeed;
        if (rollAngle < 0.0f) rollAngle = 0.0f;
    }
    else if (rollAngle < 0.0f) {
        rollAngle += returnSpeed;
        if (rollAngle > 0.0f) rollAngle = 0.0f;
    }
    glutPostRedisplay(); // ȭ�� ���� ��û
}


int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowPosition(100, 100);
    glutInitWindowSize(800, 800);
    glutCreateWindow("texture draw");

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Unable to initialize GLEW" << std::endl;
        exit(EXIT_FAILURE);
    }

    make_vertexShaders();
    make_fragmentShaders();
    shaderID = make_shaderProgram();
    applyTextures();

    InitObj("cube.obj", 0);
    InitObj("heli.obj", 1);
    InitObj("wing.obj", 2);

    // ���� ��Ȱ��ȭ �ʱ� ����
    isHelicopterControlEnabled = false;
    lastRKeyTime = std::chrono::steady_clock::now(); // ���α׷� ���� �� �ʱ� �ð� ����


    glutDisplayFunc(display);
    glutReshapeFunc(Reshape);
    glutKeyboardFunc(KeyBoard);
    glutSpecialFunc(SpecialKey);
    glutSpecialUpFunc(SpecialKeyUp); // Ű ������ �̺�Ʈ ���
    glutIdleFunc([]() {
        if (isWingAutoRotate) {
            // ������ ȸ�� �ӵ� ����
            wingRotationSpeed += wingAcceleration;
            if (wingRotationSpeed > 50.0f) { // �ִ� �ӵ� ����
                wingRotationSpeed = 50.0f;
            }
            glutPostRedisplay(); // ȭ�� ���� ��û
        }
        // �︮���� ���� ������Ʈ (���� ����)
        updateHelicopterState();
        });

    glutMainLoop();
    system("pause"); //�ݵ�� return 0; ���� �߰�
    return 0;
}