#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;

uniform sampler2D floorTexture;
uniform vec3 lightPos;
uniform vec3 viewPos;

void main()
{
    vec3 texColor = texture(floorTexture, TexCoord).rgb;

    // Lighting
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    vec3 viewDir = normalize(viewPos - FragPos);

    // Diffuse
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * texColor;

    // Ambient
    vec3 ambient = 0.3 * texColor;

    // Specular (optional highlight)
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16.0);
    vec3 specular = 0.2 * spec * vec3(1.0);

    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, 1.0);
}
