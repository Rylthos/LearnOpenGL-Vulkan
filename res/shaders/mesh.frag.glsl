#version 450
#extension GL_GOOGLE_include_directive : require

#include "light.glsl"
#include "material.glsl"

layout (location = 0) in vec2 v_UV;
layout (location = 1) in vec4 v_Colour;
layout (location = 2) in vec3 v_Normal;
layout (location = 3) in vec3 v_FragPos;
layout (location = 4) in vec3 v_CameraPos;
layout (location = 5) in flat int v_MaterialIndex;

layout (location = 0) out vec4 f_Colour;

layout(set=1, binding = 0) uniform sampler2D u_BoxSampler;
layout(set=1, binding = 1) uniform sampler2D u_FaceSampler;

void main()
{
    vec4 box = texture(u_BoxSampler, v_UV);
    vec4 face = texture(u_FaceSampler, v_UV);

    MaterialData material = u_Materials.materials[v_MaterialIndex];

    vec3 colour = vec3(0);
    vec3 ambient = u_Lights.ambient.rgb * material.ambient;

    vec3 diffuse = vec3(0.0);
    vec3 specular = vec3(0.0);

    vec3 norm = normalize(v_Normal);
    for (int i = 0; i < u_Lights.lightCount; i++)
    {
        LightData light = u_Lights.lights[i];
        vec3 lightPos = light.position;

        vec3 lightDir = normalize(lightPos - v_FragPos);

        float diff = max(dot(norm, lightDir), 0.0);
        diffuse += light.diffuse * diff;

        vec3 viewDir = normalize(v_CameraPos - v_FragPos);
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.specular.a);
        specular += light.specular * spec;
    }

    colour += ambient * material.ambient;
    colour += diffuse * material.diffuse;
    colour += specular * material.specular.rgb;

    colour = mix(box, face, 0.5).rgb * colour;

    // f_Colour = mix(box, face, 0.5);
    f_Colour = vec4(colour, 1.0);
}
