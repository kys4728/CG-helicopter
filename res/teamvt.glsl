#version 330 core
layout (location = 0) in vec3 vPos;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec2 vTexCoord;
out vec3 FragPos; //--- 객체의 위치값을 프래그먼트 세이더로 보낸다.
out vec3 Normal; //--- 노멀값을 프래그먼트 세이더로 보낸다.
out vec2 TexCoord;

uniform mat4 model; //--- 모델링 변환값
uniform mat4 view; //--- 뷰잉 변환값
uniform mat4 projection; //--- 투영 변환값
uniform float uvScale; // UV 스케일링 값

void main()
{
gl_Position = projection * view * model * vec4(vPos, 1.0);
FragPos = vec3(model * vec4(vPos, 1.0));
TexCoord = vTexCoord* uvScale;
Normal = mat3(transpose(inverse(model))) * vNormal;
}
