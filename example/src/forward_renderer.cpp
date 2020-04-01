#include "forward_renderer.h"

#include <cubez/render.h>
#include <cubez/mesh.h>
#include <map>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <array>

const size_t kMaxDirectionalLights = 4;
const size_t kMaxPointLights = 32;
const size_t kMaxSpotLights = 8;

struct LightPoint {
  // Interleave the brightness and radius to optimize std140 wasted bytes.
  glm::vec3 rgb;
  float brightness;
  glm::vec3 pos;
  float radius;
};

typedef struct LightDirectional {
  glm::vec3 rgb;
  float brightness;
  glm::vec3 dir;
  float _dir_pad;
};

typedef struct LightSpot {
  glm::vec3 rgb;
  float brightness;
  glm::vec3 pos;
  float range;
  glm::vec3 dir;
  float angle_deg;
};

typedef struct qbForwardRenderer_ {
  qbRenderer_ renderer;

  qbRenderPass scene_3d_pass;
  qbRenderPass scene_2d_pass;
  qbRenderPass gui_pass;

  qbFrameBuffer frame_buffer;

  qbGeometryDescriptor supported_geometry = nullptr;  

  qbSystem render_system;

  // The first binding of user textures.
  size_t texture_start_binding;
  size_t texture_units_count;

  // The first binding of user uniforms.
  size_t uniform_start_binding;
  size_t uniform_count;

  uint32_t camera_uniform;
  std::string camera_uniform_name;
  qbGpuBuffer camera_ubo;

  // Created per rendergroup.
  uint32_t model_uniform;
  std::string model_uniform_name;

  // Created per rendergroup.
  uint32_t material_uniform;
  std::string material_uniform_name;

  uint32_t light_uniform;
  std::string light_uniform_name;
  qbGpuBuffer light_ubo;

  std::array<struct LightDirectional, kMaxDirectionalLights> directional_lights;
  std::array<bool, kMaxDirectionalLights> enabled_directional_lights;

  std::array<struct LightPoint, kMaxPointLights> point_lights;
  std::array<bool, kMaxPointLights> enabled_point_lights;

  std::array<struct LightSpot, kMaxSpotLights> spot_lights;
  std::array<bool, kMaxSpotLights> enabled_spot_lights;

  size_t albedo_map_binding;
  size_t normal_map_binding;
  size_t metallic_map_binding;
  size_t roughness_map_binding;
  size_t ao_map_binding;
  size_t emission_map_binding;

  qbImage empty_albedo_map;
  qbImage empty_normal_map;
  qbImage empty_metallic_map;
  qbImage empty_roughness_map;
  qbImage empty_ao_map;
  qbImage empty_emission_map;
} qbForwardRenderer_, *qbForwardRenderer;

struct CameraUniform {
  alignas(16) glm::mat4 vp;
};

struct ModelUniform {
  alignas(16) glm::mat4 m;
  alignas(16) glm::mat4 rot;
};

struct MaterialUniform {
  glm::vec3 albedo;
  float metallic;
  glm::vec3 emission;
  float roughness;
};

struct LightUniform {
  LightDirectional directionals[kMaxDirectionalLights];
  LightPoint points[kMaxPointLights];
  LightSpot spots[kMaxSpotLights];

  glm::vec3 view_pos;
  float _view_pos_pad;
};

void rendergroup_oncreate(struct qbRenderer_* self, qbRenderGroup group) {
  qbForwardRenderer r = (qbForwardRenderer)self;
  
  qbGpuBuffer model;
  {
    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = qbGpuBufferType::QB_GPU_BUFFER_TYPE_UNIFORM;
    attr.size = sizeof(ModelUniform);
    attr.name = r->model_uniform_name.data();
    qb_gpubuffer_create(&model, &attr);
  }
  
  qbGpuBuffer material;
  {
    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = qbGpuBufferType::QB_GPU_BUFFER_TYPE_UNIFORM;
    attr.size = sizeof(ModelUniform);
    attr.name = r->material_uniform_name.data();
    qb_gpubuffer_create(&material, &attr);
  }

  uint32_t bindings[] = { ((qbForwardRenderer)self)->model_uniform,
                          ((qbForwardRenderer)self)->material_uniform };
  qbGpuBuffer uniforms[] = { model, material };
  qb_rendergroup_attachuniforms(group, sizeof(uniforms) / sizeof(qbGpuBuffer),
                                bindings, uniforms);
}

void rendergroup_ondestroy(struct qbRenderer_* self, qbRenderGroup group) {
  qbForwardRenderer r = (qbForwardRenderer)self;
  qb_rendergroup_removeuniform_bybinding(group, r->model_uniform);
}

void model_add(qbRenderer self, qbRenderGroup model) {
  qb_renderpass_append(((qbForwardRenderer)self)->scene_3d_pass, model);
}

void model_remove(qbRenderer self, qbRenderGroup model) {
  qb_renderpass_remove(((qbForwardRenderer)self)->scene_3d_pass, model);
}

void render(struct qbRenderer_* self, const struct qbCamera_* camera, qbRenderEvent event) {
  qbForwardRenderer r = (qbForwardRenderer)self;
  qbRenderPipeline pipeline = self->render_pipeline;
  qbFrameBuffer camera_fbo = qb_camera_fbo(camera);
  qbRenderPass* passes;
  size_t passes_count = qb_renderpipeline_passes(pipeline, &passes);
  for (size_t i = 0; i < passes_count; ++i) {
    *qb_renderpass_frame(passes[i]) = camera_fbo;
  }
  CameraUniform m;
  m.vp = camera->projection_mat * camera->view_mat;
  qb_gpubuffer_update(r->camera_ubo, 0, sizeof(CameraUniform), &m);

  LightUniform l = {};
  for (size_t i = 0; i < r->directional_lights.size(); ++i) {
    if (r->enabled_directional_lights[i]) {
      l.directionals[i] = r->directional_lights[i];
    }
  }

  for (size_t i = 0; i < r->point_lights.size(); ++i) {
    if (r->enabled_point_lights[i]) {
      l.points[i] = r->point_lights[i];
    }
  }

  for (size_t i = 0; i < r->spot_lights.size(); ++i) {
    if (r->enabled_spot_lights[i]) {
      l.spots[i] = r->spot_lights[i];
    }
  }
  l.view_pos = camera->origin;

  qb_gpubuffer_update(r->light_ubo, 0, sizeof(LightUniform), &l);

  qb_renderpipeline_render(pipeline, event);
  qb_renderpipeline_present(pipeline, camera_fbo, event);
}

void render_callback(qbFrame* f) {
  qbRenderEvent event = (qbRenderEvent)f->event;
  render(event->renderer, event->camera, event);
}

qbMeshBuffer meshbuffer_create(qbRenderer self, qbMesh mesh) {
  qbGpuBuffer verts;
  {
    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = qbGpuBufferType::QB_GPU_BUFFER_TYPE_VERTEX;
    attr.data = mesh->vertices;
    attr.elem_size = sizeof(float);
    attr.size = mesh->vertex_count * sizeof(glm::vec3);
    qb_gpubuffer_create(&verts, &attr);
  }

  qbGpuBuffer normals;
  {
    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = qbGpuBufferType::QB_GPU_BUFFER_TYPE_VERTEX;
    attr.data = mesh->normals;
    attr.elem_size = sizeof(float);
    attr.size = mesh->normal_count * sizeof(glm::vec3);
    qb_gpubuffer_create(&normals, &attr);
  }

  qbGpuBuffer uvs;
  {
    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = qbGpuBufferType::QB_GPU_BUFFER_TYPE_VERTEX;
    attr.data = mesh->uvs;
    attr.elem_size = sizeof(float);
    attr.size = mesh->uv_count * sizeof(glm::vec2);
    qb_gpubuffer_create(&uvs, &attr);
  }

  qbGpuBuffer indices;
  {
    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = qbGpuBufferType::QB_GPU_BUFFER_TYPE_INDEX;
    attr.data = mesh->indices;
    attr.elem_size = sizeof(uint32_t);
    attr.size = mesh->index_count * sizeof(uint32_t);
    qb_gpubuffer_create(&indices, &attr);
  }

  qbMeshBufferAttr_ attr = {};
  attr.descriptor = *((qbForwardRenderer)self)->supported_geometry;

  qbMeshBuffer ret;
  qb_meshbuffer_create(&ret, &attr);

  qbGpuBuffer buffers[] = { verts, normals, uvs };
  qb_meshbuffer_attachvertices(ret, buffers);
  qb_meshbuffer_attachindices(ret, indices);
  return ret;
}

void meshbuffer_attach_textures(qbRenderer self, qbMeshBuffer buffer,
                                size_t count,
                                uint32_t texture_units[],
                                qbImage textures[]) {
  uint32_t tex_units[16] = {};

  qbForwardRenderer r = (qbForwardRenderer)self;
  assert(count <= r->texture_units_count && count < 16 && "Unsupported amount of textures");

  for (size_t i = 0; i < count; ++i) {
    uint32_t texture_unit = texture_units[i];
    assert(texture_unit < r->texture_units_count && "Texture unit out of bounds");
    tex_units[i] = texture_unit + r->texture_start_binding;
  }

  qb_meshbuffer_attachimages(buffer, count, tex_units, textures);
}

void meshbuffer_attach_uniforms(qbRenderer self, qbMeshBuffer buffer,
                                size_t count,
                                uint32_t uniform_bindings[],
                                qbGpuBuffer uniforms[]) {
  qbForwardRenderer r = (qbForwardRenderer)self;
  std::vector<uint32_t> bindings;
  bindings.resize(count);

  for (size_t i = 0; i < count; ++i) {
    bindings[i] = uniform_bindings[i] + r->uniform_start_binding;
  }
  qb_meshbuffer_attachuniforms(buffer, count, bindings.data(), uniforms);
}

void rendergroup_attach_material(struct qbRenderer_* self, qbRenderGroup group,
                                 struct qbMaterial_* material) {
  qbForwardRenderer r = (qbForwardRenderer)self;
  
  uint32_t units[6] = {};
  qbImage images[6] = {};

  uint32_t count = 0;
  uint32_t material_tex_units[] = {
    r->albedo_map_binding,
    r->normal_map_binding,
    r->metallic_map_binding,
    r->roughness_map_binding,
    r->ao_map_binding,
    r->emission_map_binding,
  };

  qbImage material_images[] = {
    material->albedo_map ? material->albedo_map : r->empty_albedo_map,
    material->normal_map ? material->normal_map : r->empty_normal_map,
    material->metallic_map ? material->metallic_map : r->empty_metallic_map,
    material->roughness_map ? material->roughness_map : r->empty_roughness_map,
    material->ao_map ? material->ao_map : r->empty_ao_map,
    material->emission_map ? material->emission_map : r->empty_emission_map,
  };

  for (int i = 0; i < 6; ++i) {
    qbImage image = material_images[i];
    if (image) {
      units[count] = material_tex_units[i];
      images[count] = image;
      ++count;
    }
  }

  qb_rendergroup_attachimages(group, count, units, images);
}

void rendergroup_attach_textures(struct qbRenderer_* self, qbRenderGroup group,
                                 size_t count,
                                 uint32_t texture_units[],
                                 qbImage textures[]) {
  uint32_t tex_units[16] = {};

  qbForwardRenderer r = (qbForwardRenderer)self;
  assert(count <= r->texture_units_count && count < 16 && "Unsupported amount of textures");

  for (size_t i = 0; i < count; ++i) {
    uint32_t texture_unit = texture_units[i];
    assert(texture_unit < r->texture_units_count && "Texture unit out of bounds");
    tex_units[i] = texture_unit + r->texture_start_binding;
  }

  qb_rendergroup_attachimages(group, count, tex_units, textures);
}

void rendergroup_attach_uniforms(struct qbRenderer_* self, qbRenderGroup group,
                                 size_t count,
                                 uint32_t uniform_bindings[],
                                 qbGpuBuffer uniforms[]) {
  qbForwardRenderer r = (qbForwardRenderer)self;
  std::vector<uint32_t> bindings;
  bindings.resize(count);

  for (size_t i = 0; i < count; ++i) {
    bindings[i] = uniform_bindings[i] + r->uniform_start_binding;
  }
  qb_rendergroup_attachuniforms(group, count, bindings.data(), uniforms);
}

void set_gui_renderpass(struct qbRenderer_* self, qbRenderPass gui_renderpass) {
  ((qbForwardRenderer)self)->gui_pass = gui_renderpass;
  qb_renderpipeline_append(self->render_pipeline, gui_renderpass);
}

void light_enable(struct qbRenderer_* self, qbId id, qbLightType light_type) {
  qbForwardRenderer r = (qbForwardRenderer)self;
  switch (light_type) {
    case qbLightType::QB_LIGHT_TYPE_DIRECTIONAL:
      if (id < kMaxDirectionalLights) {
        r->enabled_directional_lights[id] = true;;
      }
      break;
    case qbLightType::QB_LIGHT_TYPE_POINT:
      if (id < kMaxPointLights) {
        r->enabled_point_lights[id] = true;
      }
      break;
    case qbLightType::QB_LIGHT_TYPE_SPOTLIGHT:
      if (id < kMaxSpotLights) {
        r->enabled_spot_lights[id] = true;
      }
      break;
  }
}

void light_disable(struct qbRenderer_* self, qbId id, qbLightType light_type) {
  qbForwardRenderer r = (qbForwardRenderer)self;
  switch (light_type) {
    case qbLightType::QB_LIGHT_TYPE_DIRECTIONAL:
      if (id < kMaxDirectionalLights) {
        r->enabled_directional_lights[id] = false;
      }
      break;
    case qbLightType::QB_LIGHT_TYPE_POINT:
      if (id < kMaxPointLights) {
        r->enabled_point_lights[id] = false;
      }
      break;
    case qbLightType::QB_LIGHT_TYPE_SPOTLIGHT:
      if (id < kMaxSpotLights) {
        r->enabled_spot_lights[id] = false;
      }
      break;
  }
}

bool light_isenabled(struct qbRenderer_* self, qbId id, qbLightType light_type) {
  qbForwardRenderer r = (qbForwardRenderer)self;
  switch (light_type) {
    case qbLightType::QB_LIGHT_TYPE_DIRECTIONAL:
      if (id < kMaxDirectionalLights) {
        return r->enabled_directional_lights[id];
      }
      break;
    case qbLightType::QB_LIGHT_TYPE_POINT:
      if (id < kMaxPointLights) {
        return r->enabled_point_lights[id];
      }
      break;
    case qbLightType::QB_LIGHT_TYPE_SPOTLIGHT:
      if (id < kMaxSpotLights) {
        return r->enabled_spot_lights[id];
      }
      break;
  }
  return false;
}

void light_directional(struct qbRenderer_* self, qbId id, glm::vec3 rgb, glm::vec3 dir, float brightness) {
  qbForwardRenderer r = (qbForwardRenderer)self;
  if (id >= kMaxDirectionalLights) {
    return;
  }
  
  r->directional_lights[id] = LightDirectional{ rgb, brightness, dir };
}

void light_point(struct qbRenderer_* self, qbId id, glm::vec3 rgb, glm::vec3 pos, float brightness, float range) {
  qbForwardRenderer r = (qbForwardRenderer)self;
  if (id >= kMaxPointLights) {
    return;
  }

  r->point_lights[id] = LightPoint{ rgb, brightness, pos, range };
}

void light_spot(struct qbRenderer_* self, qbId id, glm::vec3 rgb, glm::vec3 pos, glm::vec3 dir,
                float brightness, float range, float angle_deg) {
  qbForwardRenderer r = (qbForwardRenderer)self;
  if (id >= kMaxSpotLights) {
    return;
  }

  r->spot_lights[id] = LightSpot{ rgb, brightness, pos, range, dir, angle_deg };
}

size_t light_max(struct qbRenderer_* self, qbLightType light_type) {
  qbForwardRenderer r = (qbForwardRenderer)self;
  
  switch (light_type) {
    case qbLightType::QB_LIGHT_TYPE_DIRECTIONAL:
      return kMaxDirectionalLights;
    case qbLightType::QB_LIGHT_TYPE_POINT:
      return kMaxPointLights;
    case qbLightType::QB_LIGHT_TYPE_SPOTLIGHT:
      return kMaxSpotLights;
  }

  return 0;
}

void init_supported_geometry(qbForwardRenderer forward_renderer) {
  // https://stackoverflow.com/questions/40450342/what-is-the-purpose-of-binding-from-vkvertexinputbindingdescription
  qbBufferBinding attribute_bindings = new qbBufferBinding_[3];
  {
    qbBufferBinding binding = attribute_bindings;
    binding->binding = 0;
    binding->stride = 3 * sizeof(float); // Vertex(x, y, z)
    binding->input_rate = QB_VERTEX_INPUT_RATE_VERTEX;
  }
  {
    qbBufferBinding binding = attribute_bindings + 1;
    binding->binding = 1;
    binding->stride = 3 * sizeof(float); // Normal(x, y, z)
    binding->input_rate = QB_VERTEX_INPUT_RATE_VERTEX;
  }
  {
    qbBufferBinding binding = attribute_bindings + 2;
    binding->binding = 2;
    binding->stride = 2 * sizeof(float); // Texture(u, v)
    binding->input_rate = QB_VERTEX_INPUT_RATE_VERTEX;
  }
  /*{
    qbBufferBinding binding = attribute_bindings + 1;
    binding->binding = 3;
    binding->stride = 2 * sizeof(float);
    binding->input_rate = QB_VERTEX_INPUT_RATE_INSTANCE;
  }*/

  qbVertexAttribute attributes = new qbVertexAttribute_[4];
  {
    qbVertexAttribute_* attr = attributes;
    *attr = {};
    attr->binding = 0;
    attr->location = 0;

    attr->count = 3;
    attr->type = QB_VERTEX_ATTRIB_TYPE_FLOAT;
    attr->name = QB_VERTEX_ATTRIB_NAME_VERTEX;
    attr->normalized = false;
    attr->offset = (void*)0;
  }
  {
    qbVertexAttribute_* attr = attributes + 1;
    *attr = {};
    attr->binding = 1;
    attr->location = 1;

    attr->count = 3;
    attr->type = QB_VERTEX_ATTRIB_TYPE_FLOAT;
    attr->name = QB_VERTEX_ATTRIB_NAME_NORMAL;
    attr->normalized = false;
    attr->offset = (void*)(0 * sizeof(float));
  }
  {
    qbVertexAttribute_* attr = attributes + 2;
    *attr = {};
    attr->binding = 2;
    attr->location = 2;

    attr->count = 2;
    attr->type = QB_VERTEX_ATTRIB_TYPE_FLOAT;
    attr->name = QB_VERTEX_ATTRIB_NAME_TEXTURE;
    attr->normalized = false;
    attr->offset = (void*)(0 * sizeof(float));
  }
  /*{
    qbVertexAttribute_* attr = attributes + 3;
    *attr = {};
    attr->binding = 3;
    attr->location = 3;

    attr->count = 2;
    attr->type = QB_VERTEX_ATTRIB_TYPE_FLOAT;
    attr->name = QB_VERTEX_ATTRIB_NAME_PARAM;
    attr->normalized = false;
    attr->offset = (void*)0;
  }*/

  forward_renderer->supported_geometry = new qbGeometryDescriptor_;
  qbGeometryDescriptor supported_geometry = forward_renderer->supported_geometry;
  supported_geometry->attributes = attributes;
  supported_geometry->attributes_count = 3;
  supported_geometry->bindings = attribute_bindings;
  supported_geometry->bindings_count = 3;
}

struct qbRenderer_* qb_forwardrenderer_create(uint32_t width, uint32_t height, struct qbRendererAttr_* args) {
  qbForwardRenderer ret = new qbForwardRenderer_;

  ret->renderer.width = width;
  ret->renderer.height = height;
  ret->renderer.rendergroup_oncreate = rendergroup_oncreate;
  ret->renderer.rendergroup_ondestroy = rendergroup_ondestroy;
  ret->renderer.rendergroup_add = model_add;
  ret->renderer.rendergroup_remove = model_remove;
  ret->renderer.rendergroup_attach_textures = rendergroup_attach_textures;
  ret->renderer.rendergroup_attach_uniforms = rendergroup_attach_uniforms;
  ret->renderer.meshbuffer_create = meshbuffer_create;
  ret->renderer.meshbuffer_attach_textures = meshbuffer_attach_textures;
  ret->renderer.set_gui_renderpass = set_gui_renderpass;
  ret->renderer.rendergroup_attach_material = rendergroup_attach_material;
  
  ret->renderer.light_enable = light_enable;
  ret->renderer.light_disable = light_disable;
  ret->renderer.light_isenabled = light_isenabled;
  ret->renderer.light_directional = light_directional;
  ret->renderer.light_point = light_point;
  ret->renderer.light_spot = light_spot;
  ret->renderer.light_max = light_max;

  ret->renderer.render = render;
  ret->directional_lights = {};
  ret->enabled_directional_lights = {};
  ret->point_lights = {};
  ret->enabled_point_lights = {};
  ret->spot_lights = {};
  ret->enabled_spot_lights = {};
  
  {
    // If there is a sampler defined, there needs to be a corresponding image,
    // otherwise there is undetermined behavior. In the case that there isn't a
    // image given, an empty image is placed in its stead.
    char pixel_0[4] = {};
    qbPixelMap map_0 = qb_pixelmap_create(1, 1, qbPixelFormat::QB_PIXEL_FORMAT_RGBA8, pixel_0);

    char pixel_1[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
    qbPixelMap map_1 = qb_pixelmap_create(1, 1, qbPixelFormat::QB_PIXEL_FORMAT_RGBA8, pixel_1);

    char pixel_n[4] = { 0x80, 0x80, 0xFF, 0xFF };
    qbPixelMap map_n = qb_pixelmap_create(1, 1, qbPixelFormat::QB_PIXEL_FORMAT_RGBA8, pixel_n);

    qbImageAttr_ attr = {};
    attr.type = qbImageType::QB_IMAGE_TYPE_2D;
    
    qb_image_create(&ret->empty_albedo_map, &attr, map_1);
    qb_image_create(&ret->empty_normal_map, &attr, map_n);
    qb_image_create(&ret->empty_metallic_map, &attr, map_0);
    qb_image_create(&ret->empty_roughness_map, &attr, map_0);
    qb_image_create(&ret->empty_ao_map, &attr, map_0);
    qb_image_create(&ret->empty_emission_map, &attr, map_0);
  }
  {
    qbRenderPipelineAttr_ attr = {};
    attr.viewport = { 0.0, 0.0, width, height };
    attr.viewport_scale = 1.0f;
    attr.name = "qbForwardRenderer";

    qb_renderpipeline_create(&ret->renderer.render_pipeline, &attr);
  }

  init_supported_geometry(ret);

  qbShaderModule shader_module;
  
  std::vector<qbShaderResourceInfo_> resources;
  std::vector<uint32_t> uniform_bindings;
  std::vector<std::pair<qbShaderResourceInfo_, qbGpuBuffer>> resource_uniforms;
  std::vector<uint32_t> sampler_bindings;
  std::vector<std::pair<qbShaderResourceInfo_, qbImageSampler>> resource_samplers;

  size_t native_uniform_count;
  size_t native_sampler_count;
  
  size_t uniform_start_binding;
  size_t user_uniform_count = args->uniform_count;

  size_t sampler_start_binding;
  size_t user_sampler_count = args->image_sampler_count;

  ret->camera_uniform_name = "qb_camera";
  ret->model_uniform_name = "qb_model";
  ret->material_uniform_name = "qb_material";
  ret->light_uniform_name = "qb_lights";

  {
    qbShaderResourceInfo_ info = {};
    info.resource_type = QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER;
    info.stages = QB_SHADER_STAGE_VERTEX;
    info.name = ret->camera_uniform_name.data();

    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = QB_GPU_BUFFER_TYPE_UNIFORM;
    attr.data = nullptr;
    attr.size = sizeof(CameraUniform);

    qb_gpubuffer_create(&ret->camera_ubo, &attr);
    resource_uniforms.push_back({ info, ret->camera_ubo });
  }
  {
    qbShaderResourceInfo_ info = {};
    info.resource_type = QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER;
    info.stages = QB_SHADER_STAGE_VERTEX;
    info.name = ret->model_uniform_name.data();

    resource_uniforms.push_back({ info, nullptr });
  }
  {
    qbShaderResourceInfo_ info = {};
    info.resource_type = QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER;
    info.stages = QB_SHADER_STAGE_FRAGMENT;
    info.name = ret->material_uniform_name.data();

    resource_uniforms.push_back({ info, nullptr });
  }
  {
    qbShaderResourceInfo_ info = {};
    info.resource_type = QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER;
    info.stages = QB_SHADER_STAGE_FRAGMENT;
    info.name = ret->light_uniform_name.data();


    qbGpuBufferAttr_ attr;
    attr.buffer_type = QB_GPU_BUFFER_TYPE_UNIFORM;
    attr.data = nullptr;
    attr.size = sizeof(LightUniform);

    qb_gpubuffer_create(&ret->light_ubo, &attr);
    resource_uniforms.push_back({ info, ret->light_ubo });
  }
  {
    qbShaderResourceInfo_ info = {};
    info.resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER;
    info.stages = QB_SHADER_STAGE_FRAGMENT;
    info.name = "qb_texture_unit_1";

    qbImageSamplerAttr_ attr = {};
    attr.image_type = QB_IMAGE_TYPE_2D;
    attr.min_filter = QB_FILTER_TYPE_LIENAR_MIPMAP_LINEAR;
    attr.mag_filter = QB_FILTER_TYPE_LINEAR;

    qbImageSampler sampler;
    qb_imagesampler_create(&sampler, &attr);

    //resource_samplers.push_back({ info, sampler });
  }
  {
    qbShaderResourceInfo_ info = {};
    info.resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER;
    info.stages = QB_SHADER_STAGE_FRAGMENT;
    info.name = "qb_texture_unit_2";

    qbImageSamplerAttr_ attr = {};
    attr.image_type = QB_IMAGE_TYPE_2D;
    attr.min_filter = QB_FILTER_TYPE_LIENAR_MIPMAP_LINEAR;
    attr.mag_filter = QB_FILTER_TYPE_LINEAR;

    qbImageSampler sampler;
    qb_imagesampler_create(&sampler, &attr);

    //resource_samplers.push_back({ info, sampler });
  }

  std::vector<std::string> material_sampler_names = {
    "qb_material_albedo_map",
    "qb_material_normal_map",
    "qb_material_metallic_map",
    "qb_material_roughness_map",
    "qb_material_ao_map",
    "qb_material_emission_map",
  };
  for (auto& sampler_name : material_sampler_names) {
    qbShaderResourceInfo_ info = {};
    info.resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER;
    info.stages = QB_SHADER_STAGE_FRAGMENT;
    info.name = sampler_name.data();

    qbImageSamplerAttr_ attr = {};
    attr.image_type = QB_IMAGE_TYPE_2D;
    attr.min_filter = QB_FILTER_TYPE_LIENAR_MIPMAP_LINEAR;
    attr.mag_filter = QB_FILTER_TYPE_LINEAR;

    qbImageSampler sampler;
    qb_imagesampler_create(&sampler, &attr);

    resource_samplers.push_back({ info, sampler });
  }

  native_uniform_count = resource_uniforms.size();
  native_sampler_count = resource_samplers.size();
  {
    std::vector<std::pair<qbShaderResourceInfo_, qbGpuBuffer>> user_uniforms;
    user_uniforms.resize(args->shader_resource_count);
    for (size_t i = 0; i < args->shader_resource_count; ++i) {
      qbShaderResourceInfo info = args->shader_resources + i;
      uint32_t binding = info->binding;
      user_uniforms[binding].first = *info;
    }
    for (size_t i = 0; i < args->uniform_count; ++i) {
      qbGpuBuffer uniform = args->uniforms[i];
      uint32_t binding = args->uniform_bindings[i];
      user_uniforms[binding].second = uniform;
    }
    for (auto&& e : user_uniforms) {
      resource_uniforms.push_back(e);
    }

    std::vector<std::pair<qbShaderResourceInfo_, qbImageSampler>> user_samplers;
    user_samplers.resize(args->image_sampler_count);
    for (size_t i = 0; i < args->image_sampler_count; ++i) {
      qbImageSampler sampler = args->image_samplers[i];

      qbShaderResourceInfo_ info;
      info.resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER;
      info.stages = QB_SHADER_STAGE_FRAGMENT;
      info.name = qb_imagesampler_name(sampler);

      user_samplers.push_back({ info, sampler });
    }
    for (auto&& e : user_samplers) {
      resource_samplers.push_back(e);
    }
  }

  uniform_bindings.reserve(resource_uniforms.size());
  sampler_bindings.reserve(resource_samplers.size());
  std::vector<qbGpuBuffer> uniforms;
  std::vector<qbImageSampler> samplers;
  {
    uint32_t binding = 0;

    size_t uniform_index = 0;
    for (auto& e : resource_uniforms) {
      e.first.binding = binding++;
      resources.push_back(e.first);

      if (e.second) {
        uniforms.push_back(e.second);
        uniform_bindings.push_back(e.first.binding);
      }

      if (std::string(e.first.name) == ret->camera_uniform_name) {
        ret->camera_uniform = e.first.binding;
      } else if (std::string(e.first.name) == ret->model_uniform_name) {
        ret->model_uniform = e.first.binding;
      } else if (std::string(e.first.name) == ret->material_uniform_name) {
        ret->material_uniform = e.first.binding;
      } else if (std::string(e.first.name) == ret->light_uniform_name) {
        ret->light_uniform = e.first.binding;
      }

      if (uniform_index == native_uniform_count) {
        uniform_start_binding = binding;
      }
      ++uniform_index;
    }

    size_t sampler_index = 0;
    for (auto& e : resource_samplers) {
      e.first.binding = binding;
      resources.push_back(e.first);
      samplers.push_back(e.second);
      sampler_bindings.push_back(binding++);

      if (std::string(e.first.name) == "qb_material_albedo_map") {
        ret->albedo_map_binding = e.first.binding;
      } else if (std::string(e.first.name) == "qb_material_normal_map") {
        ret->normal_map_binding = e.first.binding;
      } else if (std::string(e.first.name) == "qb_material_metallic_map") {
        ret->metallic_map_binding = e.first.binding;
      } else if (std::string(e.first.name) == "qb_material_roughness_map") {
        ret->roughness_map_binding = e.first.binding;
      } else if (std::string(e.first.name) == "qb_material_ao_map") {
        ret->ao_map_binding = e.first.binding;
      } else if (std::string(e.first.name) == "qb_material_emission_map") {
        ret->emission_map_binding = e.first.binding;
      }

      if (sampler_index == native_sampler_count) {
        sampler_start_binding = binding;
      }
      ++sampler_index;
    }
  }


  {   
    qbShaderModuleAttr_ attr = {};
    attr.vs = "resources/pbr.vs";
    attr.fs = "resources/pbr.fs";
    attr.resources = resources.data();
    attr.resources_count = resources.size();

    qb_shadermodule_create(&shader_module, &attr);
    qb_shadermodule_attachuniforms(shader_module,
                                   uniform_bindings.size(),
                                   uniform_bindings.data(),
                                   uniforms.data());

    qb_shadermodule_attachsamplers(shader_module,
                                   sampler_bindings.size(),
                                   sampler_bindings.data(),
                                   samplers.data());
  }

  {
    qbClearValue_ clear = {};
    clear.attachments = (qbFrameBufferAttachment)(qbFrameBufferAttachment::QB_COLOR_ATTACHMENT |
                                                  qbFrameBufferAttachment::QB_DEPTH_ATTACHMENT);
    clear.depth = 0.0;
    clear.color = { 1.0, 1.0, 0.0, 1.0 };

    qbRenderPassAttr_ attr = {};
    attr.frame_buffer = nullptr;
    attr.supported_geometry = *ret->supported_geometry;
    attr.shader = shader_module;

    attr.viewport = { 0.0, 0.0, width, height };
    attr.viewport_scale = 1.0f;
    attr.clear = clear;

    qb_renderpass_create(&ret->scene_3d_pass, &attr);
    qb_renderpipeline_append(ret->renderer.render_pipeline, ret->scene_3d_pass);
  }

  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_setcallback(attr, render_callback);
    qb_systemattr_addconst(attr, qb_renderable());
    qb_systemattr_addconst(attr, qb_material());
    qb_systemattr_addmutable(attr, qb_transform());
    qb_systemattr_setjoin(attr, qbComponentJoin::QB_JOIN_LEFT);
    qb_systemattr_setfunction(attr, [](qbInstance* insts, qbFrame* f) {
      qbRenderEvent event = (qbRenderEvent)f->event;
      qbForwardRenderer renderer = (qbForwardRenderer)event->renderer;

      qbRenderable* renderable;
      qbMaterial* material;
      qbTransform transform;
      qb_instance_getconst(insts[0], &renderable);
      qb_instance_getconst(insts[1], &material);
      qb_instance_getmutable(insts[2], &transform);

      qb_renderable_upload(*renderable, *material);
      qbRenderGroup group = qb_renderable_rendergroup(*renderable);
      qbGpuBuffer model_buffer = qb_rendergroup_finduniform_bybinding(group, renderer->model_uniform);
      qbGpuBuffer material_buffer = qb_rendergroup_finduniform_bybinding(group, renderer->material_uniform);

      ModelUniform model_uniform;
      model_uniform.m = glm::translate(glm::mat4(1), transform->position) *
             glm::translate(transform->orientation, transform->pivot);
      model_uniform.rot = transform->orientation;
      qb_gpubuffer_update(model_buffer, 0, sizeof(ModelUniform), &model_uniform);

      MaterialUniform material_uniform;
      material_uniform.albedo = glm::vec4((*material)->albedo, 1);
      material_uniform.metallic = (*material)->metallic;
      material_uniform.roughness = (*material)->roughness;
      material_uniform.emission = glm::vec4((*material)->emission, 1);
      qb_gpubuffer_update(material_buffer, 0, sizeof(MaterialUniform), &material_uniform);
    });

    qb_systemattr_settrigger(attr, qbTrigger::QB_TRIGGER_EVENT);
    qb_system_create(&ret->render_system, attr);
    qb_systemattr_destroy(&attr);
    
    qb_event_subscribe(qb_render_event(), ret->render_system);
  }

  ret->uniform_start_binding = uniform_start_binding;
  ret->uniform_count = user_uniform_count;
  ret->texture_start_binding = sampler_start_binding;
  ret->texture_units_count = user_sampler_count;

  ret->renderer.state = ret;
  return (qbRenderer)ret;
}

void qb_forwardrenderer_destroy(qbRenderer renderer) {
  qbForwardRenderer r = (qbForwardRenderer)renderer;
  qb_event_unsubscribe(qb_render_event(), r->render_system);
  qb_system_destroy(&r->render_system);
}
