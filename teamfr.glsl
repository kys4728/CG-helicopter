#version 330 core

struct Light {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};

uniform Light light;
uniform Material material;
uniform vec3 viewPos;
uniform sampler2D outTexture1; // �ؽ�ó ���� 1
uniform sampler2D outTexture2; // �ؽ�ó ���� 2
uniform float uAlpha;          // ���� ����
uniform float mixRatio;        // �ؽ�ó ȥ�� ���� [0.0 ~ 1.0]
uniform bool uAmbientLightOn;  // ���� on/off ����

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
out vec4 FragColor;

void main() {
    if (!uAmbientLightOn) {
        // ������ �������� ��� ������ �˰� ǥ�� (�ؽ�ó ����)
        FragColor = vec4(0.0, 0.0, 0.0, uAlpha);
    } else {
        // ������ �������� ��� ���� ���� ��� ����
        vec3 texColor1 = texture(outTexture1, TexCoord).rgb;
        vec3 texColor2 = texture(outTexture2, TexCoord).rgb;
        vec3 combinedTexColor = mix(texColor1, texColor2, mixRatio);

        vec3 norm = normalize(Normal);
        vec3 lightDir = normalize(light.position - FragPos);
        vec3 viewDir = normalize(viewPos - FragPos);

        // Ambient
        vec3 ambient = light.ambient * material.ambient;

        // Diffuse
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = light.diffuse * (diff * material.diffuse);

        // Specular
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
        vec3 specular = light.specular * (spec * material.specular);

        vec3 result = (ambient + diffuse + specular) * combinedTexColor;

        // ���� ���� (�ʿ��ϴٸ� ����)
        vec3 gammaCorrected = pow(result, vec3(1.0 / 2.2));

        FragColor = vec4(gammaCorrected, uAlpha);
    }
}
