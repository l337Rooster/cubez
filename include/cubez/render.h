/**
* Author: Samuel Rohde (rohde.samuel@cubez.io)
*
* Copyright 2020 Samuel Rohde
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#ifndef CUBEZ_RENDER__H
#define CUBEZ_RENDER__H

#include <cubez/cubez.h>
#include <cubez/render_pipeline.h>
#include <cglm/cglm.h>
#include <cglm/types-struct.h>

typedef struct qbRenderable_* qbRenderable;

typedef struct qbRenderer_ {
  void(*render)(struct qbRenderer_* self, const struct qbCamera_* camera, qbRenderEvent event);
  
  void(*rendergroup_oncreate)(struct qbRenderer_* self, qbRenderGroup);
  void(*rendergroup_ondestroy)(struct qbRenderer_* self, qbRenderGroup);

  // Thread-safe
  // Adds the given model to the background qbRenderPipeline.
  void(*rendergroup_add)(struct qbRenderer_* self, qbRenderGroup model);

  // Thread-safe
  // Removes the given model from the background qbRenderPipeline.
  void(*rendergroup_remove)(struct qbRenderer_* self, qbRenderGroup model);

  size_t(*max_texture_units)(struct qbRenderer_* self);
  size_t(*max_uniform_units)(struct qbRenderer_* self);
  size_t(*max_lights)(struct qbRenderer_* self);

  qbMeshBuffer(*meshbuffer_create)(struct qbRenderer_* self, struct qbMesh_* mesh);
  void(*meshbuffer_attach_material)(struct qbRenderer_* self, qbMeshBuffer buffer,
                                    struct qbMaterial_* material);
  void(*meshbuffer_attach_textures)(struct qbRenderer_* self, qbMeshBuffer buffer,
                                    size_t count,
                                    uint32_t texture_units[],
                                    qbImage textures[]);
  void(*meshbuffer_attach_uniforms)(struct qbRenderer_* self, qbMeshBuffer buffer,
                                    size_t count,
                                    uint32_t uniform_bindings[],
                                    qbGpuBuffer uniforms[]);
  void(*rendergroup_attach_material)(struct qbRenderer_* self, qbRenderGroup group,
                                     struct qbMaterial_* material);
  void(*rendergroup_attach_textures)(struct qbRenderer_* self, qbRenderGroup group,
                                     size_t count,
                                     uint32_t texture_units[],
                                     qbImage textures[]);
  void(*rendergroup_attach_uniforms)(struct qbRenderer_* self, qbRenderGroup group,
                                     size_t count,
                                     uint32_t uniform_bindings[],
                                     qbGpuBuffer uniforms[]);

  void(*light_enable)(struct qbRenderer_* self, qbId id, enum qbLightType type);
  void(*light_disable)(struct qbRenderer_* self, qbId id, enum qbLightType type);
  bool(*light_isenabled)(struct qbRenderer_* self, qbId id, enum qbLightType type);
  void(*light_directional)(struct qbRenderer_* self, qbId id, vec3s rgb,
                           vec3s dir, float brightness);
  void(*light_point)(struct qbRenderer_* self, qbId id, vec3s rgb,
                     vec3s pos, float brightness, float range);
  void(*light_spot)(struct qbRenderer_* self, qbId id, vec3s rgb,
                    vec3s pos, vec3s dir, float brightness,
                    float range, float angle_deg);
  size_t(*light_max)(struct qbRenderer_* self, qbLightType light_type);

  qbFrameBuffer(*camera_framebuffer_create)(struct qbRenderer_* self, uint32_t width, uint32_t height);

  const char* title;
  int width;
  int height;
  qbRenderPipeline render_pipeline;

  void* state;
} qbRenderer_, *qbRenderer;

typedef struct qbRendererAttr_ {
  // A list of any new uniforms to be used in the shader. The bindings should
  // start at 0. These should not include any texture sampler uniforms. For
  // those, use the image_samplers value.
  qbShaderResourceInfo shader_resources;
  size_t shader_resource_count;

  // The bindings should start at 0. These should not include any texture
  // sampler uniforms. For those, use the image_samplers value.
  // Unimplemented.
  qbGpuBuffer* uniforms;
  uint32_t* uniform_bindings;
  size_t uniform_count;

  // A list of any new texture samplers to be used in the shader. This will
  // automatically create all necessary qbShaderResourceInfos. Do not create
  // individual qbShaderResourceInfos for the given samplers.
  qbImageSampler* image_samplers;
  size_t image_sampler_count;

  // An optional renderpass to draw the gui.
  qbRenderPass opt_gui_renderpass;

  // An optional present pass to draw the final frame.
  qbRenderPass opt_present_renderpass;

  // Optional arguments to pass to the create_renderer function.
  void* opt_args;

} qbRendererAttr_, *qbRendererAttr;

typedef struct qbCameraAttr_ {
  uint32_t width;
  uint32_t height;
  float fov;
  float znear;
  float zfar;

  mat4s rotation_mat;
  vec3s origin;
} qbCameraAttr_, *qbCameraAttr;

typedef struct qbCamera_ {
  uint32_t width;
  uint32_t height;
  float ratio;
  float fov;
  float znear;
  float zfar;

  mat4s view_mat;
  mat4s projection_mat;
  mat4s rotation_mat;
  vec3s origin;
  vec3s up;
  vec3s forward;
} qbCamera_;
typedef const qbCamera_* qbCamera;

typedef struct qbRenderEvent_ {
  double alpha;
  uint64_t frame;

  qbRenderer renderer;
  qbCamera camera;
} qbRenderEvent_, *qbRenderEvent;

enum qbLightType {
  QB_LIGHT_TYPE_DIRECTIONAL,
  QB_LIGHT_TYPE_SPOTLIGHT,
  QB_LIGHT_TYPE_POINT,
};

QB_API uint32_t qb_window_width();
QB_API uint32_t qb_window_height();
QB_API void qb_window_resize(uint32_t width, uint32_t height);

QB_API void qb_camera_create(qbCamera* camera, qbCameraAttr attr);
QB_API void qb_camera_destroy(qbCamera* camera);
QB_API void qb_camera_activate(qbCamera camera);
QB_API void qb_camera_deactivate(qbCamera camera);
QB_API qbCamera qb_camera_active();

QB_API void qb_camera_resize(qbCamera camera, uint32_t width, uint32_t height);
QB_API void qb_camera_fov(qbCamera camera, float fov);
QB_API void qb_camera_clip(qbCamera camera, float znear, float zfar);
QB_API void qb_camera_rotation(qbCamera camera, mat4s rotation);
QB_API void qb_camera_origin(qbCamera camera, vec3s origin);
QB_API qbFrameBuffer qb_camera_fbo(qbCamera camera);

QB_API void qb_camera_screentoworld(qbCamera camera, vec2s screen, vec3s out);
QB_API void qb_camera_worldtoscreen(qbCamera camera, vec3s world, vec2s out);

QB_API void qb_light_enable(qbId id, qbLightType light_type);
QB_API void qb_light_disable(qbId id, qbLightType light_type);
QB_API bool qb_light_isenabled(qbId id, qbLightType light_type);

QB_API void qb_light_directional(qbId id, vec3s rgb, vec3s dir, float brightness);
QB_API void qb_light_point(qbId id, vec3s rgb, vec3s pos, float brightness, float range);

QB_API size_t qb_light_max(qbLightType light_type);

QB_API qbResult qb_render(qbRenderEvent event);
QB_API qbEvent qb_render_event();

QB_API qbRenderer qb_renderer();
QB_API qbResult qb_render_swapbuffers();
QB_API qbResult qb_render_makecurrent();
QB_API qbResult qb_render_makenull();

typedef struct qbTransform_ {
  vec3s pivot;
  vec3s position;
  mat4s orientation;
} qbTransform_, *qbTransform;

QB_API void qb_renderable_create(qbRenderable* renderable, struct qbModel_* model);
QB_API void qb_renderable_destroy(qbRenderable* renderable);

// Frees from GPU.
QB_API void qb_renderable_free(qbRenderable renderable);

// Updates model in RAM.
QB_API void qb_renderable_update(qbRenderable renderable, struct qbModel_* model);

// Uploads model and material to GPU.
QB_API void qb_renderable_upload(qbRenderable renderable, struct qbMaterial_* material);

QB_API struct qbModel_* qb_renderable_model(qbRenderable renderable);
QB_API qbRenderGroup qb_renderable_rendergroup(qbRenderable renderable);

QB_API qbComponent qb_renderable();
QB_API qbComponent qb_material();
QB_API qbComponent qb_transform();

#endif  // CUBEZ_RENDER__H
