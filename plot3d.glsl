/* Phong shader adapted from:

   https://github.com/GeertArien/learnopengl-examples/blob/master/src/2-3-materials/2-light.glsl

   using inverse of transpose matrix to transform normals
*/
@ctype mat4 hmm_mat4
@ctype vec3 hmm_vec3

@vs vs
uniform vs_params {
    mat4 mvp;
    mat4 model;
    mat4 model_co;
};

in vec3 position;
in vec3 deriv;

out vec3 frag_pos;
out vec3 normal;

void main() {
    gl_Position = mvp * vec4(position, 1.0);
    frag_pos = vec3(model * vec4(position, 1.0));
    normal = normalize(mat3(model_co) * vec3(-deriv.x, -deriv.y, 1.0));
}
@end

@fs fs
in vec3 frag_pos;
in vec3 normal;

out vec4 frag_color;

uniform fs_params {
    vec3 view_pos;
};

uniform fs_material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
} material;

uniform fs_light {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
} light;

void main() {
    vec3 ambient = light.ambient * material.ambient;

    vec3 norm = normalize(normal);
    vec3 light_dir = normalize(light.position - frag_pos);
    float diff = max(dot(norm, light_dir), 0.0);
    vec3 diffuse = light.diffuse * (diff * material.diffuse);

    vec3 view_dir = normalize(view_pos - frag_pos);
    vec3 reflect_dir = reflect(-light_dir, norm);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), material.shininess);
    vec3 specular = light.specular * (spec * material.specular);

    vec3 result = ambient + diffuse + specular;
    frag_color = vec4(result, 1.0);
}
@end

@program phong vs fs
