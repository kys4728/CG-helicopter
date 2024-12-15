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
GLuint objVAOs[20], objVBOs[20], objEBOs[20]; // 20개로 증가
GLuint objIndexCounts[20];
std::vector<int> objMaterialIndices[20]; // 각 OBJ 파일의 재질 인덱스 배열
std::vector<tinyobj::material_t> globalMaterials[20];

glm::vec3 cameraPos = glm::vec3(0.0f, 4.0f, -5.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, 1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
float cameraSpeed = 0.05f;

bool isAmbientLightOn = true; // 주변 조명 상태 (초기값: 켜짐)
float lightIntensity = 4.0f; // 초기 조명 세기 (기본값: 1.0f)

bool isLightOrbiting = false; // 조명이 공전 중인지 여부
float lightOrbitAngle = 0.0f; // 조명의 회전 각도 (라디안)
bool isOrbitingClockwise = true; // 공전 방향 상태 (true: 시계 방향, false: 반시계 방향)
float lightOrbitRadius = 3.0f; // 초기 반경 설정

glm::vec3 lightInitialPos = glm::vec3(0.0f, 3.0f, 3.0f); // 초기 조명 위치
glm::vec3 lightPos = lightInitialPos; // 현재 조명 위치


glm::vec3 lightColors[] = {
    glm::vec3(1.0f, 1.0f, 1.0f),
    glm::vec3(1.0f, 0.0f, 0.0f),
    glm::vec3(0.0f, 1.0f, 0.0f), // 초록
    glm::vec3(0.0f, 0.0f, 1.0f)  // 파랑
};

int currentLightColorIndex = 0; // 현재 색상 인덱스
int lightPositionIndex = 0; // 초기 위치 인덱스 (0~3)
float lightHeight = 3.0f;   // 조명의 y축 높이

GLuint textures[6]; // 육면체 텍스처
GLuint pyramidTextures[5]; // 사각뿔 텍스처
GLuint bgtextures[6];
GLuint helitextures[1];
GLuint wingtextures[1];
GLuint groundtextures[1];
GLuint cloudtextures[1];

float rotationX = 0.0f; // x축 회전 각도
float rotationY = 0.0f; // y축 회전 각도

float lightScale = 1.5f; // 기본값
bool isWingAutoRotate = false; // 날개의 자동 회전 상태
float wingRotationSpeed = 0.0f; // 날개의 현재 회전 속도
float wingAcceleration = 0.0001f; // 날개의 가속도 (초기값)

bool isHelicopterControlEnabled = false;

// 헬리콥터의 y축 위치 변수
float heliY = 0.0f; // 초기 위치 설정
float heliX = 0.0f; // 헬리콥터의 초기 X축 위치
float heliZ = 0.0f; // 초기 Z축 위치

bool isRKeyPressed = false;
float tiltAngle = 0.0f; // 헬리콥터의 기울기 각도
float rollAngle = 0.0f; // 헬리콥터의 Z축 기울기 각도

bool isFirstPersonView = false; // 초기값: 3인칭 시점

const float HELI_BOUNDARY_MIN_XZ = -35.0f; // X, Z 최소 경계
const float HELI_BOUNDARY_MAX_XZ = 35.0f;  // X, Z 최대 경계
const float HELI_BOUNDARY_MIN_Y = 0.0f;    // Y 최소 경계
const float HELI_BOUNDARY_MAX_Y = 28.0f;   // Y 최대 경계


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

            // 정점 좌표
            glm::vec3 v0(attrib.vertices[3 * idx0.vertex_index + 0],
                attrib.vertices[3 * idx0.vertex_index + 1],
                attrib.vertices[3 * idx0.vertex_index + 2]);
            glm::vec3 v1(attrib.vertices[3 * idx1.vertex_index + 0],
                attrib.vertices[3 * idx1.vertex_index + 1],
                attrib.vertices[3 * idx1.vertex_index + 2]);
            glm::vec3 v2(attrib.vertices[3 * idx2.vertex_index + 0],
                attrib.vertices[3 * idx2.vertex_index + 1],
                attrib.vertices[3 * idx2.vertex_index + 2]);

            // 법선 계산
            glm::vec3 edge1 = v1 - v0;
            glm::vec3 edge2 = v2 - v0;
            glm::vec3 faceNormal = glm::normalize(glm::cross(edge1, edge2));

            tempNormals[idx0.vertex_index] += faceNormal;
            tempNormals[idx1.vertex_index] += faceNormal;
            tempNormals[idx2.vertex_index] += faceNormal;

            // 텍스처 좌표
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

            // 정점 데이터 추가 (v0, uv0)
            vertices.push_back(v0.x);
            vertices.push_back(v0.y);
            vertices.push_back(v0.z);
            vertices.push_back(faceNormal.x);
            vertices.push_back(faceNormal.y);
            vertices.push_back(faceNormal.z);
            vertices.push_back(uv0.x);
            vertices.push_back(uv0.y);

            // 정점 데이터 추가 (v1, uv1)
            vertices.push_back(v1.x);
            vertices.push_back(v1.y);
            vertices.push_back(v1.z);
            vertices.push_back(faceNormal.x);
            vertices.push_back(faceNormal.y);
            vertices.push_back(faceNormal.z);
            vertices.push_back(uv1.x);
            vertices.push_back(uv1.y);

            // 정점 데이터 추가 (v2, uv2)
            vertices.push_back(v2.x);
            vertices.push_back(v2.y);
            vertices.push_back(v2.z);
            vertices.push_back(faceNormal.x);
            vertices.push_back(faceNormal.y);
            vertices.push_back(faceNormal.z);
            vertices.push_back(uv2.x);
            vertices.push_back(uv2.y);

            // 인덱스 추가
            indices.push_back(indices.size());
            indices.push_back(indices.size());
            indices.push_back(indices.size());

            // 재질 인덱스 추가
            if (i / 3 < shape.mesh.material_ids.size()) {
                materialIndices.push_back(shape.mesh.material_ids[i / 3]);
            }
            else {
                materialIndices.push_back(-1); // 기본값
            }
        }
    }

    // 법선 벡터 정규화
    for (size_t i = 0; i < tempNormals.size(); ++i) {
        tempNormals[i] = glm::normalize(tempNormals[i]);
        if (glm::dot(tempNormals[i], glm::vec3(0.0f, 0.0f, 1.0f)) < 0) {
            tempNormals[i] = -tempNormals[i]; // 반전
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
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0); ///STBI_rgb 또는 0
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

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // S축 반복
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); // T축 반복


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

        if (glm::length(ambient) == 0.0f) ambient = glm::vec3(0.2f); // 기본값 설정
        if (glm::length(diffuse) == 0.0f) diffuse = glm::vec3(0.8f); // 기본 확산색
        if (glm::length(specular) == 0.0f) specular = glm::vec3(1.0f); // 기본 반사광

        glUniform3fv(glGetUniformLocation(shaderID, "material.ambient"), 1, glm::value_ptr(ambient));
        glUniform3fv(glGetUniformLocation(shaderID, "material.diffuse"), 1, glm::value_ptr(diffuse));
        glUniform3fv(glGetUniformLocation(shaderID, "material.specular"), 1, glm::value_ptr(specular));
        glUniform1f(glGetUniformLocation(shaderID, "material.shininess"), material.shininess * 128.0f);
    }
    else {
        // 기본 재질
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

    // 육면체 텍스처 (6면)
    textures[0] = loadTexture("platform.png"); // 앞면

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

// 헬리콥터를 따라다니는 카메라 업데이트 함수
void updateCameraPosition() {
    float distanceBehindHelicopter = 10.0f; // 헬리콥터 뒤쪽 거리
    float heightAboveHelicopter = 5.0f;     // 헬리콥터 위쪽 높이

    if (isFirstPersonView) {
        // 1인칭 시점: 헬리콥터 앞쪽과 약간 위로 위치
        cameraPos = glm::vec3(
            heliX,
            heliY,
            heliZ
        );

        // 1인칭 카메라 방향: 헬리콥터의 현재 방향을 따라감
        cameraFront = glm::vec3(
            sin(rotationY), // x축 방향
            0.0f,           // y축 고정
            cos(rotationY) // z축 방향 (반대 방향 고려)
        );

        cameraPos += cameraFront * 2.0f;
        cameraUp = glm::vec3(0.0f, 1.0f, 0.0f); // 상향 벡터 유지
    }
    else {
        // 3인칭 시점: 헬리콥터를 따라다니는 카메라
        glm::vec3 helicopterPosition = glm::vec3(heliX, heliY, heliZ);

        float offsetX = distanceBehindHelicopter * sin(rotationY);
        float offsetZ = distanceBehindHelicopter * cos(rotationY);

        cameraPos = glm::vec3(
            heliX - offsetX,
            heliY + heightAboveHelicopter,
            heliZ - offsetZ
        );

        cameraFront = glm::normalize(helicopterPosition - cameraPos); // 헬리콥터를 바라보는 방향
        cameraUp = glm::vec3(0.0f, 1.0f, 0.0f); // 상향 벡터 유지
    }
}

void enforceHelicopterBoundaries() {
    // X축 경계 확인
    if (heliX < HELI_BOUNDARY_MIN_XZ) heliX = HELI_BOUNDARY_MIN_XZ;
    if (heliX > HELI_BOUNDARY_MAX_XZ) heliX = HELI_BOUNDARY_MAX_XZ;

    // Z축 경계 확인
    if (heliZ < HELI_BOUNDARY_MIN_XZ) heliZ = HELI_BOUNDARY_MIN_XZ;
    if (heliZ > HELI_BOUNDARY_MAX_XZ) heliZ = HELI_BOUNDARY_MAX_XZ;

    // Y축 경계 확인
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
    // 카메라 및 투영 행렬 설정
    glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
    glUniformMatrix4fv(glGetUniformLocation(shaderID, "view"), 1, GL_FALSE, glm::value_ptr(view));

    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
    glUniformMatrix4fv(glGetUniformLocation(shaderID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));


    // 조명 설정
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
    // 헬리콥터 그리기
    glm::mat4 heliModel = glm::mat4(1.0f);

    // 헬리콥터 위치 설정
    heliModel = glm::translate(heliModel, glm::vec3(heliX, heliY, heliZ));

    // 헬리콥터 Y축 회전 적용
    heliModel = glm::rotate(heliModel, rotationY, glm::vec3(0.0f, 1.0f, 0.0f));
    // 헬리콥터 X축 기울기 적용
    heliModel = glm::rotate(heliModel, tiltAngle, glm::vec3(1.0f, 0.0f, 0.0f));
    // 헬리콥터 Z축 기울기 적용
    heliModel = glm::rotate(heliModel, rollAngle, glm::vec3(0.0f, 0.0f, 1.0f));

    // 크기 조정
    heliModel = glm::scale(heliModel, glm::vec3(1.0f));

    // 쉐이더에 모델 행렬 전송
    glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, glm::value_ptr(heliModel));
    applyMaterial(4, 0); // 헬리콥터 재질 적용
    glBindTexture(GL_TEXTURE_2D, helitextures[0]);
    // 헬리콥터 그리기
    glDrawElements(GL_TRIANGLES, objIndexCounts[1], GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);

    // 날개 그리기
    glBindVertexArray(objVAOs[2]);
    glm::mat4 wingModel = heliModel; // 헬리콥터 모델 행렬을 기반으로 함
    wingModel = glm::translate(heliModel, glm::vec3(0.0f, 0.1f, 0.0f));
    // 날개의 개별 회전 (헬리콥터와 함께 회전하면서 추가적인 회전)
    static float wingRotationAngle = 0.0f;
    wingRotationAngle += 0.0001f; // 날개의 회전 속도 (음수로 설정하여 반시계 방향))
    if (isWingAutoRotate) {
        wingRotationAngle += wingRotationSpeed; // 현재 속도로 회전
        if (wingRotationAngle > 360.0f) {
            wingRotationAngle -= 360.0f;
        }
    }

    // 날개의 회전 행렬 적용
    wingModel = glm::rotate(wingModel, glm::radians(wingRotationAngle), glm::vec3(0.0f, 1.0f, 0.0f));

    // 날개의 크기 조정 (필요에 따라 조정)
    wingModel = glm::scale(wingModel, glm::vec3(0.8f));
    // 쉐이더에 모델 행렬 전송
    glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, glm::value_ptr(wingModel));
    applyMaterial(4, 0); // 날개 재질 적용
    glBindTexture(GL_TEXTURE_2D, wingtextures[0]);
    // 날개 그리기
    glDrawElements(GL_TRIANGLES, objIndexCounts[2], GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);

    // 클라우드의 로컬 좌표에서 AABB 설정 (모델 공간에서의 크기)
    glm::vec3 cloudLocalMin(-0.5f, -0.5f, -0.5f); // 클라우드 모델의 최소 좌표
    glm::vec3 cloudLocalMax(0.5f, 0.5f, 0.5f);   // 클라우드 모델의 최대 좌표

    // 클라우드 월드 공간에서의 AABB 계산
    glm::vec3 cloudWorldMin = glm::vec3(clModel * glm::vec4(cloudLocalMin, 1.0f));
    glm::vec3 cloudWorldMax = glm::vec3(clModel * glm::vec4(cloudLocalMax, 1.0f));
    // 헬리콥터의 로컬 좌표에서 AABB 설정 (모델 공간에서의 크기)
    glm::vec3 heliLocalMin(-0.5f, -0.5f, -0.5f); // 헬리콥터 모델의 최소 좌표
    glm::vec3 heliLocalMax(0.5f, 0.5f, 0.5f);   // 헬리콥터 모델의 최대 좌표

    // 헬리콥터 월드 공간에서의 AABB 계산
    glm::vec3 heliWorldMin = glm::vec3(heliModel * glm::vec4(heliLocalMin, 1.0f));
    glm::vec3 heliWorldMax = glm::vec3(heliModel * glm::vec4(heliLocalMax, 1.0f));
    // 충돌 감지 (AABB 충돌 체크)
    bool isColliding = (heliWorldMin.x <= cloudWorldMax.x && heliWorldMax.x >= cloudWorldMin.x) &&
        (heliWorldMin.y <= cloudWorldMax.y && heliWorldMax.y >= cloudWorldMin.y) &&
        (heliWorldMin.z <= cloudWorldMax.z && heliWorldMax.z >= cloudWorldMin.z);

    if (isColliding) {
        std::cout << "Helicopter reached the cloud. Exiting program." << std::endl;
        exit(0); // 프로그램 종료
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
    const float movementStep = 0.1f; // 이동 간격
    const float rotationStep = glm::radians(3.0f); // 회전 각도 (5도)

    auto currentTime = std::chrono::steady_clock::now(); // 현재 시간 저장

    switch (key) {
    case 'q':
        exit(0); // 프로그램 종료
        break;

    case 32: // 스페이스바 입력시 날개 자동 회전 토글 및 조작 활성화 시작
        isWingAutoRotate = !isWingAutoRotate;
        lastRKeyTime = currentTime;
        isRKeyPressed = true;
        isHelicopterControlEnabled = false;
        std::cout << "Helicopter control will be enabled after 8 seconds." << std::endl;
        if (isWingAutoRotate) {
            wingRotationSpeed = 0.0f; // 초기 속도 설정
            std::cout << "Wing auto rotation enabled with acceleration." << std::endl;
        }
        else {
            std::cout << "Wing auto rotation disabled." << std::endl;
        }
        break;
    case 'v': // V 키를 눌렀을 때 시점 전환
        isFirstPersonView = !isFirstPersonView;
        if (isFirstPersonView) {
            std::cout << "First-person view enabled." << std::endl;
        }
        else {
            std::cout << "Third-person view enabled." << std::endl;
        }
        break;
        // 위로 이동
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

        // 아래로 이동
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
    case 'a': // 왼쪽 회전
    case 'd': // 오른쪽 회전
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
    // 카메라 위치 업데이트
    updateCameraPosition();

    // 화면 갱신 요청
    glutPostRedisplay();
}

GLvoid SpecialKey(int key, int x, int y) {
    const float movementStep = 0.05f; // 이동 간격

    // 시동 여부 확인
    if (!isWingAutoRotate) {
        std::cout << "Cannot move. Start the helicopter by pressing the spacebar." << std::endl;
        return;
    }

    auto currentTime = std::chrono::steady_clock::now(); // 현재 시간 저장
    auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastRKeyTime).count();

    if (!isHelicopterControlEnabled && elapsedTime < 8) {
        std::cout << "Cannot move. Please wait for " << (8 - elapsedTime) << " seconds." << std::endl;
        return;
    }

    if (elapsedTime >= 8) {
        isHelicopterControlEnabled = true; // 조작 활성화
    }

    // 현재 회전 각도에 따라 이동 방향 계산
    glm::vec3 forwardDir = glm::vec3(sin(rotationY), 0.0f, cos(rotationY)); // 전진 방향
    glm::vec3 rightDir = glm::vec3(cos(rotationY), 0.0f, -sin(rotationY));  // 오른쪽 방향

    switch (key) {
    case GLUT_KEY_UP: // 위쪽 화살표 입력 시 (앞으로 전진)
        heliX += forwardDir.x * movementStep;
        heliZ += forwardDir.z * movementStep;
        tiltAngle = glm::radians(30.0f); // 전진 시 앞으로 기울기
        break;

    case GLUT_KEY_DOWN: // 아래쪽 화살표 입력 시 (뒤로 후진)
        heliX -= forwardDir.x * movementStep;
        heliZ -= forwardDir.z * movementStep;
        tiltAngle = glm::radians(-30.0f); // 후진 시 뒤로 기울기
        break;

    case GLUT_KEY_RIGHT: // 오른쪽 화살표 입력 시 (오른쪽 이동)
        heliX -= rightDir.x * movementStep;
        heliZ -= rightDir.z * movementStep;
        rollAngle = glm::radians(30.0f); // 오른쪽으로 기울기
        break;

    case GLUT_KEY_LEFT: // 왼쪽 화살표 입력 시 (왼쪽 이동)
        heliX += rightDir.x * movementStep;
        heliZ += rightDir.z * movementStep;
        rollAngle = glm::radians(-30.0f); // 왼쪽으로 기울기
        break;

    default:
        tiltAngle = glm::radians(0.0f); // 기울기 초기화
        rollAngle = glm::radians(0.0f);
        break;
    }

    // 헬리콥터 현재 위치 출력
    std::cout << "Helicopter position: (" << heliX << ", " << heliY << ", " << heliZ << ")" << std::endl;

    // 경계 충돌 처리
    enforceHelicopterBoundaries();

    // 카메라 위치 업데이트
    updateCameraPosition();

    // 화면 갱신 요청
    glutPostRedisplay();
}

GLvoid SpecialKeyUp(int key, int x, int y) {
    switch (key) {
    case GLUT_KEY_UP:
    case GLUT_KEY_DOWN:
        tiltAngle = 0.0f; // 전진/후진 기울기 초기화
        break;
    case GLUT_KEY_LEFT:
    case GLUT_KEY_RIGHT:
        rollAngle = 0.0f; // 좌우 기울기 초기화
        break;
    default:
        break;
    }
    glutPostRedisplay(); // 화면 갱신 요청
}
void updateHelicopterState() {
    const float returnSpeed = 0.0001f; // 기울기 복귀 속도

    // X축 기울기 복귀
    if (tiltAngle > 0.0f) {
        tiltAngle -= returnSpeed;
        if (tiltAngle < 0.0f) tiltAngle = 0.0f;
    }
    else if (tiltAngle < 0.0f) {
        tiltAngle += returnSpeed;
        if (tiltAngle > 0.0f) tiltAngle = 0.0f;
    }

    // Z축 기울기 복귀
    if (rollAngle > 0.0f) {
        rollAngle -= returnSpeed;
        if (rollAngle < 0.0f) rollAngle = 0.0f;
    }
    else if (rollAngle < 0.0f) {
        rollAngle += returnSpeed;
        if (rollAngle > 0.0f) rollAngle = 0.0f;
    }
    glutPostRedisplay(); // 화면 갱신 요청
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

    // 조작 비활성화 초기 설정
    isHelicopterControlEnabled = false;
    lastRKeyTime = std::chrono::steady_clock::now(); // 프로그램 시작 시 초기 시간 설정


    glutDisplayFunc(display);
    glutReshapeFunc(Reshape);
    glutKeyboardFunc(KeyBoard);
    glutSpecialFunc(SpecialKey);
    glutSpecialUpFunc(SpecialKeyUp); // 키 릴리스 이벤트 등록
    glutIdleFunc([]() {
        if (isWingAutoRotate) {
            // 날개의 회전 속도 증가
            wingRotationSpeed += wingAcceleration;
            if (wingRotationSpeed > 50.0f) { // 최대 속도 제한
                wingRotationSpeed = 50.0f;
            }
            glutPostRedisplay(); // 화면 갱신 요청
        }
        // 헬리콥터 상태 업데이트 (기울기 복귀)
        updateHelicopterState();
        });

    glutMainLoop();
    system("pause"); //반드시 return 0; 위에 추가
    return 0;
}