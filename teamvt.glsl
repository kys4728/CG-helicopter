#version 330 core
layout (location = 0) in vec3 vPos;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec2 vTexCoord;
out vec3 FragPos; //--- ��ü�� ��ġ���� �����׸�Ʈ ���̴��� ������.
out vec3 Normal; //--- ��ְ��� �����׸�Ʈ ���̴��� ������.
out vec2 TexCoord;

uniform mat4 model; //--- �𵨸� ��ȯ��
uniform mat4 view; //--- ���� ��ȯ��
uniform mat4 projection; //--- ���� ��ȯ��
uniform float uvScale; // UV �����ϸ� ��

void main()
{
gl_Position = projection * view * model * vec4(vPos, 1.0);
FragPos = vec3(model * vec4(vPos, 1.0));
TexCoord = vTexCoord* uvScale;
Normal = mat3(transpose(inverse(model))) * vNormal;
}
