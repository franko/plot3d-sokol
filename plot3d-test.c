#include <HandmadeMath.h>
#include <SDL.h>
#include <sokol_gfx.h>

#include "plot3d.glsl.h"

typedef struct {
    float *vertices;
    int vertices_len;
    uint16_t *indices;
    int indices_len;
} plot_geom;

typedef struct {
    float x1, x2, y1, y2;
    int nx, ny;
    int groupx, groupy;
} plot_domain;

static struct {
    float rx, ry;
    sg_pipeline pip;
    sg_bindings bind;
    SDL_Window *window;
    SDL_GLContext *ctx;
    plot_geom geom;
    hmm_vec3 light_pos;
} state;


// Normally what is said is that to transform a normal vector for a linear transform
// the inverse transpose is needed but afterward the resulting vector needs to be
// normalized.
// We consider that the inverse transpose matrix is not needed but only the cofactor
// matrix. The two matrices are proportional and normalization is needed in any case.
//
// The cofactor matrix has the nice geometrical interpretation that it trasforms
// bivectors in their representation as normal bector.
//
// https://www.reedbeta.com/blog/normals-inverse-transpose-part-1/
// https://en.wikipedia.org/wiki/Minor_(linear_algebra)#Inverse_of_a_matrix
hmm_mat4 HMM_Cofactor(hmm_mat4 m) {
    hmm_mat4 r;
    r.Elements[0][0] =  (m.Elements[1][1] * m.Elements[2][2] - m.Elements[1][2] * m.Elements[2][1]);
    r.Elements[1][0] = -(m.Elements[0][1] * m.Elements[2][2] - m.Elements[0][2] * m.Elements[2][1]);
    r.Elements[2][0] =  (m.Elements[0][1] * m.Elements[1][2] - m.Elements[0][2] * m.Elements[1][1]);
    r.Elements[0][1] = -(m.Elements[1][0] * m.Elements[2][2] - m.Elements[1][2] * m.Elements[2][0]);
    r.Elements[1][1] =  (m.Elements[0][0] * m.Elements[2][2] - m.Elements[0][2] * m.Elements[2][0]);
    r.Elements[2][1] = -(m.Elements[0][0] * m.Elements[1][2] - m.Elements[0][2] * m.Elements[1][0]);
    r.Elements[0][2] =  (m.Elements[1][0] * m.Elements[2][1] - m.Elements[1][1] * m.Elements[2][0]);
    r.Elements[1][2] = -(m.Elements[0][0] * m.Elements[2][1] - m.Elements[0][1] * m.Elements[2][0]);
    r.Elements[2][2] =  (m.Elements[0][0] * m.Elements[1][1] - m.Elements[0][1] * m.Elements[1][0]);

    r.Elements[3][0] = 0.0;
    r.Elements[3][1] = 0.0;
    r.Elements[3][2] = 0.0;
    r.Elements[0][3] = 0.0;
    r.Elements[1][3] = 0.0;
    r.Elements[2][3] = 0.0;
    r.Elements[3][3] = 1.0;
    return r;
}

float plot_fn(float x, float y, hmm_vec2 *deriv) {
    const float r = HMM_SquareRootF(x * x + y * y);
    const float rq = r * r;
    if (r < 1e-6) {
        deriv->X = - 2 * x / 3;
        deriv->Y = - 2 * y / 3;
        return (1 - rq / 6 + rq * rq / 120);
    } else {
        const float der = (r * HMM_CosF(r) - HMM_SinF(r)) / rq;
        deriv->X = der * x / r;
        deriv->Y = der * y / r;
    }
    return HMM_SinF(r) / r;
}

#define PLOT3D_VS_MULT (3 + 2 + 2)

static void do_plot3d_geometry(plot_geom *geom, const plot_domain domain) {
    const double dx = (domain.x2 - domain.x1) / domain.nx;
    const double dy = (domain.y2 - domain.y1) / domain.ny;
    geom->vertices_len = (domain.nx + 1) * (domain.ny + 1); /* Nb of points */
    geom->vertices = malloc(sizeof(float) * geom->vertices_len * PLOT3D_VS_MULT);
    for (int i = 0; i <= domain.nx; i++) {
        const float x = domain.x1 + i * dx;
        const float x_grid = domain.x1 + (i / domain.groupx) * domain.groupx * dx;
        const float x_bar = (x - x_grid) / (domain.groupx * dx);
        for (int j = 0; j <= domain.ny; j++) {
            const float y = domain.y1 + j * dy;
            const float y_grid = domain.y1 + (j / domain.groupy) * domain.groupy * dy;
            const float y_bar = (y - y_grid) / (domain.groupy * dy);
            hmm_vec2 deriv;
            const float z = plot_fn(x, y, &deriv);
            float line[] = {x, y, z, x_bar, y_bar, deriv.X, deriv.Y};
            float *v_ptr = geom->vertices + PLOT3D_VS_MULT * (i * (domain.ny + 1) + j);
            memcpy(v_ptr, line, sizeof(line));
        }
    }
    geom->indices_len = 2 * domain.nx * domain.ny; /* Nb of triangles */
    geom->indices = malloc(sizeof(uint16_t) * 3 * geom->indices_len);
    for (int i = 0; i < domain.nx; i++) {
        for (int j = 0; j < domain.ny; j++) {
            uint16_t *i_ptr = geom->indices + 3 * 2 * (i * domain.ny + j);
            /* First triangle */
            i_ptr[0] = i       * (domain.ny + 1) + j;
            i_ptr[1] = (i + 1) * (domain.ny + 1) + j;
            i_ptr[2] = i       * (domain.ny + 1) + j + 1;

            /* Second triangle */
            i_ptr[3] = i       * (domain.ny + 1) + j + 1;
            i_ptr[4] = (i + 1) * (domain.ny + 1) + j;
            i_ptr[5] = (i + 1) * (domain.ny + 1) + j + 1;
        }
    }
}

void init() {
    const plot_domain domain = {-8.0f, 8.0f, -8.0f, 8.0f, 50, 50, 5, 5};
    plot_geom *surf = &state.geom;
    state.light_pos = HMM_Vec3(5.0f, 5.0f, 4.0f);

    do_plot3d_geometry(surf, domain);

    sg_setup(&(sg_desc){ 0 });

    sg_buffer vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = {surf->vertices, sizeof(float) * PLOT3D_VS_MULT * surf->vertices_len},
        .label = "cube-vertices"
    });

    sg_buffer ibuf = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data = {surf->indices, sizeof(uint16_t) * 3 * surf->indices_len},
        .label = "cube-indices"
    });

    /* create shader */
    sg_shader shd = sg_make_shader(phong_shader_desc(sg_query_backend()));

    /* create pipeline object */
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            /* test to provide buffer stride, but no attr offsets */
            .buffers[0].stride = sizeof(float) * PLOT3D_VS_MULT,
            .attrs = {
                [ATTR_vs_position].format = SG_VERTEXFORMAT_FLOAT3,
                [ATTR_vs_pos_bar].format  = SG_VERTEXFORMAT_FLOAT2,
                [ATTR_vs_deriv].format    = SG_VERTEXFORMAT_FLOAT2
            }
        },
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_NONE,
        .depth = {
            .write_enabled = true,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
        },
        .label = "plot3d-pipeline"
    });

    /* setup resource bindings */
    state.bind = (sg_bindings) {
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf
    };
}

void frame(const plot_geom *geom, int w, int h, Uint32 frame_duration) {
    /* NOTE: the vs_params_t struct has been code-generated by the shader-code-gen */
    vs_params_t vs_params;
    const float t = frame_duration * 0.03;
    hmm_mat4 proj = HMM_Perspective(60.0f, (float)w / (float)h, 0.01f, 10.0f);
    hmm_mat4 view = HMM_LookAt(HMM_Vec3(0.0f, 1.5f, 6.0f), HMM_Vec3(0.0f, 0.0f, 0.0f), HMM_Vec3(0.0f, 0.0f, 1.0f));
    hmm_mat4 view_proj = HMM_MultiplyMat4(proj, view);
    state.rx += 1.0f * t; state.ry += 2.0f * t;
    hmm_mat4 scalem = HMM_Scale(HMM_Vec3(0.2f, 0.2f, 1.0f));
    hmm_mat4 rxm = HMM_Rotate(state.rx, HMM_Vec3(1.0f, 0.0f, 0.0f));
    hmm_mat4 rym = HMM_Rotate(state.ry, HMM_Vec3(0.0f, 1.0f, 0.0f));
    hmm_mat4 rotm = HMM_MultiplyMat4(rxm, rym);
    hmm_mat4 model = HMM_MultiplyMat4(rotm, scalem);
    vs_params.mvp = HMM_MultiplyMat4(view_proj, model);
    vs_params.model = model;
    vs_params.model_co = HMM_Cofactor(model);

    fs_params_t fs_params = { .view_pos = HMM_Vec3(0.0f, 3.0f, 2.0f) };

    fs_material_t fs_material = {
        .ambient       = HMM_Vec3(0.4f, 0.4f, 1.0f),
        .diffuse_front = HMM_Vec3(0.4f, 0.4f, 1.0f),
        .diffuse_back  = HMM_Vec3(1.0f, 0.4f, 0.4f),
        .specular      = HMM_Vec3(0.5f, 0.5f, 0.5f),
        .shininess = 32.0f,
    };

    fs_light_t fs_light = {
        .position = state.light_pos,
        .ambient  = HMM_Vec3(0.2f, 0.2f, 0.2f),
        .diffuse  = HMM_Vec3(0.5f, 0.5f, 0.5f),
        .specular = HMM_Vec3(1.0f, 1.0f, 1.0f)
    };

    sg_pass_action pass_action = {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.25f, 0.5f, 0.75f, 1.0f } }
    };
    sg_begin_default_pass(&pass_action, w, h);
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params,   &SG_RANGE(vs_params));
    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_fs_params,   &SG_RANGE(fs_params));
    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_fs_material, &SG_RANGE(fs_material));
    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_fs_light,    &SG_RANGE(fs_light));
    sg_draw(0, geom->indices_len * 3, 1);
    sg_end_pass();
    sg_commit();
}

int main(int argc, char **argv) {
    SDL_Init(SDL_INIT_EVERYTHING);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  
    state.window = SDL_CreateWindow("Window",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        720, 720, SDL_WINDOW_RESIZABLE|SDL_WINDOW_OPENGL|SDL_WINDOW_ALLOW_HIGHDPI);
    state.ctx = SDL_GL_CreateContext(state.window);

    init();

    const Uint32 frame_duration = 1000 / 60;
    while(!SDL_QuitRequested()){
        int w, h;
        SDL_GL_GetDrawableSize(state.window, &w, &h);
        frame(&state.geom, w, h, frame_duration);
        SDL_GL_SwapWindow(state.window);
        SDL_Delay(frame_duration);
    }

    sg_shutdown();
    SDL_GL_DeleteContext(state.ctx);
    SDL_DestroyWindow(state.window);
    SDL_Quit();
    return 0;
}

