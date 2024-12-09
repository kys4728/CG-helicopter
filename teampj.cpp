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

#define TINYOBJLOADER_IMPLEMENTATION

GLuint shaderID;
GLuint objVAOs[20], objVBOs[20], objEBOs[20]; // 20개로 증가
GLuint objIndexCounts[20];
std::vector<int> objMaterialIndices[20]; // 각 OBJ 파일의 재질 인덱스 배열
std::vector<tinyobj::material_t> globalMaterials[20];

glm::vec3 cameraPos = glm::vec3(0.0f, 2.0f, 10.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
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

char currentShape = 'p';// 'c': 육면체, 'p': 사각뿔
float rotationX = 0.0f; // x축 회전 각도
float rotationY = 0.0f; // y축 회전 각도

float lightScale = 1.5f; // 기본값

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
    textures[0] = loadTexture("cube.png"); // 앞면
    textures[1] = loadTexture("cube.png"); // 뒷면
    textures[2] = loadTexture("cube.png"); // 윗면
    textures[3] = loadTexture("cube.png"); // 아랫면
    textures[4] = loadTexture("cube.png"); // 왼쪽면
    textures[5] = loadTexture("cube.png"); // 오른쪽면

    // 사각뿔 텍스처 (4면 + 밑면)
    pyramidTextures[0] = loadTexture("pyramid.png"); // 앞면
    pyramidTextures[1] = loadTexture("pyramid.png"); // 뒷면
    pyramidTextures[2] = loadTexture("pyramid.png"); // 왼쪽면
    pyramidTextures[3] = loadTexture("pyramid.png"); // 오른쪽면
    pyramidTextures[4] = loadTexture("pyramid.png"); // 밑면

    bgtextures[0] = loadTexture("sky.png");
    bgtextures[1] = loadTexture("sky.png");
    bgtextures[2] = loadTexture("sky.png");
    bgtextures[3] = loadTexture("sky.png");
    bgtextures[4] = loadTexture("sky.png");
    bgtextures[5] = loadTexture("sky.png");

    helitextures[0] = loadTexture("helitexture.png");
}

void updateLightPosition() {
    if (isLightOrbiting) {
        float orbitSpeed = 0.005f;

        // 공전 각도 업데이트
        lightOrbitAngle += isOrbitingClockwise ? orbitSpeed : -orbitSpeed;

        // 각도 정상화
        if (lightOrbitAngle > glm::two_pi<float>()) lightOrbitAngle -= glm::two_pi<float>();
        else if (lightOrbitAngle < 0.0f) lightOrbitAngle += glm::two_pi<float>();

        // XZ 평면에서의 공전 위치 계산 (Y값 유지)
        lightPos.x = lightOrbitRadius * cos(lightOrbitAngle);
        lightPos.z = lightOrbitRadius * sin(lightOrbitAngle);
    }

    glutPostRedisplay();
}

void rotateCameraAroundCenter(float angle) {
    // 현재 카메라가 바라보고 있는 y좌표를 저장
    float targetY = cameraPos.y;

    // 카메라와 바라보는 중심 간의 벡터 계산 (xz 평면에서만 회전)
    glm::vec3 toCamera = cameraPos - glm::vec3(0.0f, targetY, 0.0f);
    float distance = glm::length(toCamera); // 카메라와 중심 간의 거리
    float currentAngle = atan2(toCamera.z, toCamera.x); // 현재 각도 계산

    // 새로운 각도 계산
    currentAngle += angle;

    // 새로운 위치 계산 (y값은 고정)
    cameraPos.x = distance * cos(currentAngle);
    cameraPos.z = distance * sin(currentAngle);

    // 카메라 방향 업데이트 (현재 바라보는 y좌표를 유지)
    cameraFront = glm::normalize(glm::vec3(0.0f, targetY, 0.0f) - cameraPos);
}

void drawScene() {
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    glUseProgram(shaderID);

    // 카메라 및 투영 행렬 설정
    glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
    glUniformMatrix4fv(glGetUniformLocation(shaderID, "view"), 1, GL_FALSE, glm::value_ptr(view));

    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
    glUniformMatrix4fv(glGetUniformLocation(shaderID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    // 화면 기준 회전을 독립적으로 적용
    glm::mat4 rotationXMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(rotationX), glm::vec3(1.0f, 0.0f, 0.0f)); // X축 회전
    glm::mat4 rotationYMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(rotationY), glm::vec3(0.0f, 1.0f, 0.0f)); // Y축 회전

    // 회전을 병합 (순서 상관없이 동작)
    glm::mat4 model = rotationYMatrix * rotationXMatrix;
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

    // 조명 렌더링

    glDepthMask(GL_FALSE);
    glBindVertexArray(objVAOs[0]);
    for (int i = 0; i < 6; ++i) {
        glm::mat4 bgModel = glm::mat4(1.0f);
        bgModel = glm::scale(bgModel, glm::vec3(-11.0f));
        glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, glm::value_ptr(bgModel));
        applyMaterial(0, objMaterialIndices[0][i]);
        glBindTexture(GL_TEXTURE_2D, bgtextures[i]);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)(i * 6 * sizeof(unsigned int)));
    }
    glDepthMask(GL_TRUE);

    glBindVertexArray(objVAOs[1]);
    // 헬리콥터 그리기
    glm::mat4 heliModel = glm::mat4(1.0f);

    // 헬리콥터 위치 설정
    heliModel = glm::translate(heliModel, glm::vec3(0.0f, 2.0f, 5.0f));

    // 키보드 입력에 따라 회전 적용 (X축, Y축)
    heliModel = glm::rotate(heliModel, glm::radians(rotationX), glm::vec3(1.0f, 0.0f, 0.0f)); // X축 회전
    heliModel = glm::rotate(heliModel, glm::radians(rotationY), glm::vec3(0.0f, 1.0f, 0.0f)); // Y축 회전

    // 크기 조정
    heliModel = glm::scale(heliModel, glm::vec3(1.0f));

    // 쉐이더에 모델 행렬 전송
    glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, glm::value_ptr(heliModel));
    applyMaterial(4, 0); // 헬리콥터 재질 적용
    glBindTexture(GL_TEXTURE_2D, helitextures[0]);
    // 헬리콥터 그리기
    glDrawElements(GL_TRIANGLES, objIndexCounts[1], GL_UNSIGNED_INT, 0);



    glBindVertexArray(0);
    glutSwapBuffers();
}


void display() {
    drawScene();
}

GLvoid Reshape(int w, int h) {
    glViewport(0, 0, w, h);
}

GLvoid KeyBoard(unsigned char key, int x, int y) {
    const float rotationSpeed = 5.0f; // 회전 속도 (각도)
    switch (key) {
    case 'c':
        currentShape = 'c'; // 육면체 선택
        break;
    case 'p':
        currentShape = 'p'; // 사각뿔 선택
        break;
    case 'q':
        exit(0); // 프로그램 종료
        break;
    case 's': // 초기화
        rotationX = 0.0f;
        rotationY = 0.0f;
        std::cout << "Rotation reset to initial state." << std::endl;
        break;
    case 'x': // x축을 기준으로 시계 방향 회전
        rotationX += rotationSpeed;
        break;
    case 'X': // x축을 기준으로 반시계 방향 회전
        rotationX -= rotationSpeed;
        break;
    case 'y': // y축을 기준으로 시계 방향 회전
        rotationY += rotationSpeed;
        break;
    case 'Y': // y축을 기준으로 반시계 방향 회전
        rotationY -= rotationSpeed;
        break;

    default:
        break;
    }
    // 화면 갱신 요청
    glutPostRedisplay();
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

    InitLightObj("cube.obj");

    glutDisplayFunc(display);
    glutReshapeFunc(Reshape);
    glutKeyboardFunc(KeyBoard);
    glutIdleFunc([]() {
        updateLightPosition();
        });
    glutMainLoop();
    return 0;
}
