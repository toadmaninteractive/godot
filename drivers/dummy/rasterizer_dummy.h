/*************************************************************************/
/*  rasterizer_dummy.h                                                   */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#ifndef RASTERIZER_DUMMY_H
#define RASTERIZER_DUMMY_H

#include "core/math/camera_matrix.h"
#include "core/rid_owner.h"
#include "core/self_list.h"
#include "scene/resources/mesh.h"
#include "servers/visual/rasterizer.h"
#include "servers/visual_server.h"

class RasterizerSceneDummy : public RasterizerScene {
public:
	/* SHADOW ATLAS API */

	virtual RID shadow_atlas_create() { return RID(); }
	virtual void shadow_atlas_set_size(RID p_atlas, int p_size) {}
	virtual void shadow_atlas_set_quadrant_subdivision(RID p_atlas, int p_quadrant, int p_subdivision) {}
	virtual bool shadow_atlas_update_light(RID p_atlas, RID p_light_intance, float p_coverage, uint64_t p_light_version) { return false; }

	virtual void directional_shadow_atlas_set_size(int p_size) {}
	virtual int get_directional_light_shadow_size(RID p_light_intance) { return 0; }
	virtual void set_directional_shadow_count(int p_count) {}

	/* SKY API */

	virtual RID sky_create() { return RID(); }
	virtual void sky_set_radiance_size(RID p_sky, int p_radiance_size) {}
	virtual void sky_set_mode(RID p_sky, VS::SkyMode p_samples) {}
	virtual void sky_set_texture(RID p_sky, RID p_panorama) {}

	/* ENVIRONMENT API */

	virtual RID environment_create() { return RID(); }

	virtual void environment_set_background(RID p_env, VS::EnvironmentBG p_bg) {}
	virtual void environment_set_sky(RID p_env, RID p_sky) {}
	virtual void environment_set_sky_custom_fov(RID p_env, float p_scale) {}
	virtual void environment_set_sky_orientation(RID p_env, const Basis &p_orientation) {}
	virtual void environment_set_bg_color(RID p_env, const Color &p_color) {}
	virtual void environment_set_bg_energy(RID p_env, float p_energy) {}
	virtual void environment_set_canvas_max_layer(RID p_env, int p_max_layer) {}
	virtual void environment_set_ambient_light(RID p_env, const Color &p_color, VS::EnvironmentAmbientSource p_ambient = VS::ENV_AMBIENT_SOURCE_BG, float p_energy = 1.0, float p_sky_contribution = 0.0, VS::EnvironmentReflectionSource p_reflection_source = VS::ENV_REFLECTION_SOURCE_BG) {}
// FIXME: Disabled during Vulkan refactoring, should be ported.
#if 0
	virtual void environment_set_camera_feed_id(RID p_env, int p_camera_feed_id) {};
#endif

	virtual void environment_set_dof_blur_near(RID p_env, bool p_enable, float p_distance, float p_transition, float p_far_amount, VS::EnvironmentDOFBlurQuality p_quality) {}
	virtual void environment_set_dof_blur_far(RID p_env, bool p_enable, float p_distance, float p_transition, float p_far_amount, VS::EnvironmentDOFBlurQuality p_quality) {}
	virtual void environment_set_glow(RID p_env, bool p_enable, int p_level_flags, float p_intensity, float p_strength, float p_bloom_threshold, VS::EnvironmentGlowBlendMode p_blend_mode, float p_hdr_bleed_threshold, float p_hdr_bleed_scale, float p_hdr_luminance_cap, bool p_bicubic_upscale) {}
	virtual void environment_set_fog(RID p_env, bool p_enable, float p_begin, float p_end, RID p_gradient_texture) {}

	virtual void environment_set_ssr(RID p_env, bool p_enable, int p_max_steps, float p_fade_int, float p_fade_out, float p_depth_tolerance, bool p_roughness) {}
	virtual void environment_set_ssao(RID p_env, bool p_enable, float p_radius, float p_intensity, float p_radius2, float p_intensity2, float p_bias, float p_light_affect, float p_ao_channel_affect, const Color &p_color, VS::EnvironmentSSAOQuality p_quality, VS::EnvironmentSSAOBlur p_blur, float p_bilateral_sharpness) {}

	virtual void environment_set_tonemap(RID p_env, VS::EnvironmentToneMapper p_tone_mapper, float p_exposure, float p_white, bool p_auto_exposure, float p_min_luminance, float p_max_luminance, float p_auto_exp_speed, float p_auto_exp_scale) {}

	virtual void environment_set_adjustment(RID p_env, bool p_enable, float p_brightness, float p_contrast, float p_saturation, RID p_ramp) {}

	virtual void environment_set_fog(RID p_env, bool p_enable, const Color &p_color, const Color &p_sun_color, float p_sun_amount) {}
	virtual void environment_set_fog_depth(RID p_env, bool p_enable, float p_depth_begin, float p_depth_end, float p_depth_curve, bool p_transmit, float p_transmit_curve) {}
	virtual void environment_set_fog_height(RID p_env, bool p_enable, float p_min_height, float p_max_height, float p_height_curve) {}

	virtual bool is_environment(RID p_env) const { return false; }
	virtual VS::EnvironmentBG environment_get_background(RID p_env) const { return VS::ENV_BG_KEEP; }
	virtual int environment_get_canvas_max_layer(RID p_env) const { return 0; }

	virtual RID light_instance_create(RID p_light) { return RID(); }
	virtual void light_instance_set_transform(RID p_light_instance, const Transform &p_transform) {}
	virtual void light_instance_set_shadow_transform(RID p_light_instance, const CameraMatrix &p_projection, const Transform &p_transform, float p_far, float p_split, int p_pass, float p_bias_scale = 1.0) {}
	virtual void light_instance_mark_visible(RID p_light_instance) {}

	virtual RID reflection_atlas_create() { return RID(); }
	virtual void reflection_atlas_set_size(RID p_ref_atlas, int p_reflection_size, int p_reflection_count) {}

	virtual RID reflection_probe_instance_create(RID p_probe) { return RID(); }
	virtual void reflection_probe_instance_set_transform(RID p_instance, const Transform &p_transform) {}
	virtual void reflection_probe_release_atlas_index(RID p_instance) {}
	virtual bool reflection_probe_instance_needs_redraw(RID p_instance) { return false; }
	virtual bool reflection_probe_instance_has_reflection(RID p_instance) { return false; }
	virtual bool reflection_probe_instance_begin_render(RID p_instance, RID p_reflection_atlas) { return false; }
	virtual bool reflection_probe_instance_postprocess_step(RID p_instance) { return true; }

	virtual RID gi_probe_instance_create(RID p_gi_probe) { return RID(); }
	virtual void gi_probe_instance_set_transform_to_data(RID p_probe, const Transform &p_xform) {}
	virtual bool gi_probe_needs_update(RID p_probe) const { return false; }
	virtual void gi_probe_update(RID p_probe, bool p_update_light_instances, const Vector<RID> &p_light_instances, int p_dynamic_object_count, InstanceBase **p_dynamic_objects) {}

	virtual void render_scene(RID p_render_buffers, const Transform &p_cam_transform, const CameraMatrix &p_cam_projection, bool p_cam_ortogonal, InstanceBase **p_cull_result, int p_cull_count, RID *p_light_cull_result, int p_light_cull_count, RID *p_reflection_probe_cull_result, int p_reflection_probe_cull_count, RID *p_gi_probe_cull_result, int p_gi_probe_cull_count, RID p_environment, RID p_shadow_atlas, RID p_reflection_atlas, RID p_reflection_probe, int p_reflection_probe_pass) {}

	virtual void render_shadow(RID p_light, RID p_shadow_atlas, int p_pass, InstanceBase **p_cull_result, int p_cull_count) {}
	virtual void render_material(const Transform &p_cam_transform, const CameraMatrix &p_cam_projection, bool p_cam_ortogonal, InstanceBase **p_cull_result, int p_cull_count, RID p_framebuffer, const Rect2i &p_region) {}

	virtual void set_scene_pass(uint64_t p_pass) {}
	virtual void set_time(double p_time) {}
	virtual void set_debug_draw_mode(VS::ViewportDebugDraw p_debug_draw) {}

	virtual RID render_buffers_create() { return RID(); }
	virtual void render_buffers_configure(RID p_render_buffers, RID p_render_target, int p_width, int p_height, VS::ViewportMSAA p_msaa) {}

	virtual bool free(RID p_rid) { return true; }

	virtual void update() {}

	RasterizerSceneDummy() {}
	~RasterizerSceneDummy() {}
};

class RasterizerStorageDummy : public RasterizerStorage {
public:
	/* TEXTURE API */
	struct DummyTexture {
		int width;
		int height;
		Image::Format format;
		Ref<Image> image;
		Vector<Ref<Image> > layers;
		String path;
	};

	struct DummySurface {
		uint32_t format;
		VS::PrimitiveType primitive;
		PoolVector<uint8_t> array;
		int vertex_count;
		PoolVector<uint8_t> index_array;
		int index_count;
		AABB aabb;
		Vector<PoolVector<uint8_t> > blend_shapes;
		Vector<AABB> bone_aabbs;
	};

	struct DummyMesh {
		Vector<VS::SurfaceData> surfaces;
		int blend_shape_count;
		VS::BlendShapeMode blend_shape_mode;
	};

	mutable RID_PtrOwner<DummyTexture> texture_owner;
	mutable RID_PtrOwner<DummyMesh> mesh_owner;

	/* TEXTURE API */

	virtual RID texture_2d_create(const Ref<Image> &p_image) override {
		DummyTexture *texture = memnew(DummyTexture);
		ERR_FAIL_COND_V(!texture, RID());
		
		texture->width = p_image->get_width();
		texture->height = p_image->get_height();
		texture->format = p_image->get_format();
		texture->image.instance();
		texture->image->create(texture->width, texture->height, false, texture->format, p_image->get_data());

		return texture_owner.make_rid(texture);
	}

	virtual RID texture_2d_layered_create(const Vector<Ref<Image> > &p_layers, VS::TextureLayeredType p_layered_type) override {
		DummyTexture *texture = memnew(DummyTexture);
		ERR_FAIL_COND_V(!texture, RID());

		texture->width = p_layers[0]->get_width();
		texture->height = p_layers[0]->get_height();
		texture->format = p_layers[0]->get_format();

		for (int i = 0; i < p_layers.size(); ++i) {
			Ref<Image> layer;
			layer.instance();
			layer->create(texture->width, texture->height, false, texture->format, p_layers[i]->get_data());
			texture->layers.push_back(layer);
		}

		return texture_owner.make_rid(texture);
	}

	virtual RID texture_3d_create(const Vector<Ref<Image> > &p_slices) override {
		DummyTexture *texture = memnew(DummyTexture);
		ERR_FAIL_COND_V(!texture, RID());

		texture->width = p_slices[0]->get_width();
		texture->height = p_slices[0]->get_height();
		texture->format = p_slices[0]->get_format();

		for (int i = 0; i < p_slices.size(); ++i) {
			Ref<Image> layer;
			layer.instance();
			layer->create(texture->width, texture->height, false, texture->format, p_slices[i]->get_data());
			texture->layers.push_back(layer);
		}

		return texture_owner.make_rid(texture);
	}

	virtual RID texture_proxy_create(RID p_base) override {
		DummyTexture *texture = memnew(DummyTexture);
		ERR_FAIL_COND_V(!texture, RID());
		return texture_owner.make_rid(texture);
	}

	virtual void texture_2d_update_immediate(RID p_texture, const Ref<Image> &p_image, int p_layer = 0) override {}
	virtual void texture_2d_update(RID p_texture, const Ref<Image> &p_image, int p_layer = 0) override {}
	virtual void texture_3d_update(RID p_texture, const Ref<Image> &p_image, int p_depth, int p_mipmap) override {}
	virtual void texture_proxy_update(RID p_proxy, RID p_base) override {}

	//these two APIs can be used together or in combination with the others.
	virtual RID texture_2d_placeholder_create() override {
		DummyTexture *texture = memnew(DummyTexture);
		ERR_FAIL_COND_V(!texture, RID());
		return texture_owner.make_rid(texture);
	}

	virtual RID texture_2d_layered_placeholder_create() override {
		DummyTexture *texture = memnew(DummyTexture);
		ERR_FAIL_COND_V(!texture, RID());
		return texture_owner.make_rid(texture);
	}

	virtual RID texture_3d_placeholder_create() override {
		DummyTexture *texture = memnew(DummyTexture);
		ERR_FAIL_COND_V(!texture, RID());
		return texture_owner.make_rid(texture);
	}

	virtual Ref<Image> texture_2d_get(RID p_texture) const override {
		DummyTexture *t = texture_owner.getornull(p_texture);
		ERR_FAIL_COND_V(!t, Ref<Image>());
		return t->image;
	}

	virtual Ref<Image> texture_2d_layer_get(RID p_texture, int p_layer) const override {
		DummyTexture *t = texture_owner.getornull(p_texture);
		ERR_FAIL_COND_V(!t, Ref<Image>());
		return t->layers[p_layer];
	}

	virtual Ref<Image> texture_3d_slice_get(RID p_texture, int p_depth, int p_mipmap) const override {
		DummyTexture *t = texture_owner.getornull(p_texture);
		ERR_FAIL_COND_V(!t, Ref<Image>());
		return t->layers[p_depth];
	}

	virtual void texture_replace(RID p_texture, RID p_by_texture) override {}
	virtual void texture_set_size_override(RID p_texture, int p_width, int p_height) override {}

// FIXME: Disabled during Vulkan refactoring, should be ported.
#if 0
	virtual void texture_bind(RID p_texture, uint32_t p_texture_no) = 0;
#endif

	virtual void texture_set_path(RID p_texture, const String &p_path) override {
		DummyTexture *t = texture_owner.getornull(p_texture);
		ERR_FAIL_COND(!t);
		t->path = p_path;
	}

	virtual String texture_get_path(RID p_texture) const override {
		DummyTexture *t = texture_owner.getornull(p_texture);
		ERR_FAIL_COND_V(!t, String());
		return t->path;
	}

	virtual void texture_set_detect_3d_callback(RID p_texture, VS::TextureDetectCallback p_callback, void *p_userdata) override {}
	virtual void texture_set_detect_normal_callback(RID p_texture, VS::TextureDetectCallback p_callback, void *p_userdata) override {}
	virtual void texture_set_detect_roughness_callback(RID p_texture, VS::TextureDetectRoughnessCallback p_callback, void *p_userdata) override {}

	virtual void texture_debug_usage(List<VS::TextureInfo> *r_info) override {}

	virtual void texture_set_force_redraw_if_visible(RID p_texture, bool p_enable) override {}

	virtual Size2 texture_size_with_proxy(RID p_proxy) override { return Size2(); }

	/* SHADER API */

	virtual RID shader_create() override { return RID(); }

	virtual void shader_set_code(RID p_shader, const String &p_code) override {}
	virtual String shader_get_code(RID p_shader) const override { return ""; }
	virtual void shader_get_param_list(RID p_shader, List<PropertyInfo> *p_param_list) const override {}

	virtual void shader_set_default_texture_param(RID p_shader, const StringName &p_name, RID p_texture) override {}
	virtual RID shader_get_default_texture_param(RID p_shader, const StringName &p_name) const override { return RID(); }
	virtual Variant shader_get_param_default(RID p_material, const StringName &p_param) const override { return Variant(); }

	/* COMMON MATERIAL API */

	virtual RID material_create() override { return RID(); }

	virtual void material_set_render_priority(RID p_material, int priority) override {}
	virtual void material_set_shader(RID p_shader_material, RID p_shader) override {}

	virtual void material_set_param(RID p_material, const StringName &p_param, const Variant &p_value) override {}
	virtual Variant material_get_param(RID p_material, const StringName &p_param) const override { return Variant(); }

	virtual void material_set_next_pass(RID p_material, RID p_next_material) override {}

	virtual bool material_is_animated(RID p_material) override { return false; }
	virtual bool material_casts_shadows(RID p_material) override { return false; }

	virtual void material_update_dependency(RID p_material, RasterizerScene::InstanceBase *p_instance) override {}

	/* MESH API */

	virtual RID mesh_create() override {
		DummyMesh *mesh = memnew(DummyMesh);
		ERR_FAIL_COND_V(!mesh, RID());
		mesh->blend_shape_count = 0;
		mesh->blend_shape_mode = VS::BLEND_SHAPE_MODE_NORMALIZED;
		return mesh_owner.make_rid(mesh);
	}

	/// Returns stride
	virtual void mesh_add_surface(RID p_mesh, const VS::SurfaceData &p_surface) override {
		DummyMesh *m = mesh_owner.getornull(p_mesh);
		ERR_FAIL_COND(!m);

		m->surfaces.push_back(p_surface);
	}

	virtual int mesh_get_blend_shape_count(RID p_mesh) const override {
		DummyMesh *m = mesh_owner.getornull(p_mesh);
		ERR_FAIL_COND_V(!m, 0);
		return m->blend_shape_count;
	}

	virtual void mesh_set_blend_shape_mode(RID p_mesh, VS::BlendShapeMode p_mode) override {
		DummyMesh *m = mesh_owner.getornull(p_mesh);
		ERR_FAIL_COND(!m);
		m->blend_shape_mode = p_mode;
	}

	virtual VS::BlendShapeMode mesh_get_blend_shape_mode(RID p_mesh) const override {
		DummyMesh *m = mesh_owner.getornull(p_mesh);
		ERR_FAIL_COND_V(!m, VS::BLEND_SHAPE_MODE_NORMALIZED);
		return m->blend_shape_mode;
	}

	virtual void mesh_surface_update_region(RID p_mesh, int p_surface, int p_offset, const PoolVector<uint8_t> &p_data) override {}

	virtual void mesh_surface_set_material(RID p_mesh, int p_surface, RID p_material) override {}
	virtual RID mesh_surface_get_material(RID p_mesh, int p_surface) const override { return RID(); }

	virtual VS::SurfaceData mesh_get_surface(RID p_mesh, int p_surface) const override {
		DummyMesh *m = mesh_owner.getornull(p_mesh);
		ERR_FAIL_COND_V(!m, VS::SurfaceData());
		return m->surfaces[p_surface];
	}

	virtual int mesh_get_surface_count(RID p_mesh) const override {
		DummyMesh *m = mesh_owner.getornull(p_mesh);
		ERR_FAIL_COND_V(!m, 0);
		return m->surfaces.size();
	};

	virtual void mesh_set_custom_aabb(RID p_mesh, const AABB &p_aabb) override{};
	virtual AABB mesh_get_custom_aabb(RID p_mesh) const override { return AABB(); }

	virtual AABB mesh_get_aabb(RID p_mesh, RID p_skeleton = RID()) override { return AABB(); }

	virtual void mesh_clear(RID p_mesh) override{};

	/* MULTIMESH API */

	virtual RID multimesh_create() override { return RID(); }

	virtual void multimesh_allocate(RID p_multimesh, int p_instances, VS::MultimeshTransformFormat p_transform_format, bool p_use_colors = false, bool p_use_custom_data = false) override {}

	virtual int multimesh_get_instance_count(RID p_multimesh) const override { return 0; }

	virtual void multimesh_set_mesh(RID p_multimesh, RID p_mesh) override {}
	virtual void multimesh_instance_set_transform(RID p_multimesh, int p_index, const Transform &p_transform) override {}
	virtual void multimesh_instance_set_transform_2d(RID p_multimesh, int p_index, const Transform2D &p_transform) override {}
	virtual void multimesh_instance_set_color(RID p_multimesh, int p_index, const Color &p_color) override {}
	virtual void multimesh_instance_set_custom_data(RID p_multimesh, int p_index, const Color &p_color) override {}

	virtual RID multimesh_get_mesh(RID p_multimesh) const override { return RID(); }

	virtual Transform multimesh_instance_get_transform(RID p_multimesh, int p_index) const override { return Transform(); }
	virtual Transform2D multimesh_instance_get_transform_2d(RID p_multimesh, int p_index) const override { return Transform2D(); }
	virtual Color multimesh_instance_get_color(RID p_multimesh, int p_index) const override { return Color(); }
	virtual Color multimesh_instance_get_custom_data(RID p_multimesh, int p_index) const override { return Color(); }

	virtual void multimesh_set_buffer(RID p_multimesh, const PoolVector<float> &p_buffer) override {}
	virtual PoolVector<float> multimesh_get_buffer(RID p_multimesh) const override { return PoolVector<float>(); }

	virtual void multimesh_set_visible_instances(RID p_multimesh, int p_visible) override {}
	virtual int multimesh_get_visible_instances(RID p_multimesh) const override { return 0; }

	virtual AABB multimesh_get_aabb(RID p_multimesh) const override { return AABB(); }

	/* IMMEDIATE API */

	virtual RID immediate_create() override { return RID(); }
	virtual void immediate_begin(RID p_immediate, VS::PrimitiveType p_rimitive, RID p_texture = RID()) override {}
	virtual void immediate_vertex(RID p_immediate, const Vector3 &p_vertex) override {}
	virtual void immediate_normal(RID p_immediate, const Vector3 &p_normal) override {}
	virtual void immediate_tangent(RID p_immediate, const Plane &p_tangent) override {}
	virtual void immediate_color(RID p_immediate, const Color &p_color) override {}
	virtual void immediate_uv(RID p_immediate, const Vector2 &tex_uv) override {}
	virtual void immediate_uv2(RID p_immediate, const Vector2 &tex_uv) override {}
	virtual void immediate_end(RID p_immediate) override {}
	virtual void immediate_clear(RID p_immediate) override {}
	virtual void immediate_set_material(RID p_immediate, RID p_material) override {}
	virtual RID immediate_get_material(RID p_immediate) const override { return RID(); }
	virtual AABB immediate_get_aabb(RID p_immediate) const override { return AABB(); }

	/* SKELETON API */

	virtual RID skeleton_create() override { return RID(); }
	virtual void skeleton_allocate(RID p_skeleton, int p_bones, bool p_2d_skeleton = false) override {}
	virtual int skeleton_get_bone_count(RID p_skeleton) const override { return 0; }
	virtual void skeleton_bone_set_transform(RID p_skeleton, int p_bone, const Transform &p_transform) override {}
	virtual Transform skeleton_bone_get_transform(RID p_skeleton, int p_bone) const override { return Transform(); }
	virtual void skeleton_bone_set_transform_2d(RID p_skeleton, int p_bone, const Transform2D &p_transform) override {}
	virtual Transform2D skeleton_bone_get_transform_2d(RID p_skeleton, int p_bone) const override { return Transform2D(); }
	virtual void skeleton_set_base_transform_2d(RID p_skeleton, const Transform2D &p_base_transform) override {}

	/* Light API */

	virtual RID light_create(VS::LightType p_type) override { return RID(); }

	virtual void light_set_color(RID p_light, const Color &p_color) override {}
	virtual void light_set_param(RID p_light, VS::LightParam p_param, float p_value) override {}
	virtual void light_set_shadow(RID p_light, bool p_enabled) override {}
	virtual void light_set_shadow_color(RID p_light, const Color &p_color) override {}
	virtual void light_set_projector(RID p_light, RID p_texture) override {}
	virtual void light_set_negative(RID p_light, bool p_enable) override {}
	virtual void light_set_cull_mask(RID p_light, uint32_t p_mask) override {}
	virtual void light_set_reverse_cull_face_mode(RID p_light, bool p_enabled) override {}
	virtual void light_set_use_gi(RID p_light, bool p_enable) override {}

	virtual void light_omni_set_shadow_mode(RID p_light, VS::LightOmniShadowMode p_mode) override {}

	virtual void light_directional_set_shadow_mode(RID p_light, VS::LightDirectionalShadowMode p_mode) override {}
	virtual void light_directional_set_blend_splits(RID p_light, bool p_enable) override {}
	virtual bool light_directional_get_blend_splits(RID p_light) const override { return false; }
	virtual void light_directional_set_shadow_depth_range_mode(RID p_light, VS::LightDirectionalShadowDepthRangeMode p_range_mode) override {}
	virtual VS::LightDirectionalShadowDepthRangeMode light_directional_get_shadow_depth_range_mode(RID p_light) const override { return VS::LIGHT_DIRECTIONAL_SHADOW_DEPTH_RANGE_STABLE; }

	virtual VS::LightDirectionalShadowMode light_directional_get_shadow_mode(RID p_light) override { return VS::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL; }
	virtual VS::LightOmniShadowMode light_omni_get_shadow_mode(RID p_light) override { return VS::LIGHT_OMNI_SHADOW_DUAL_PARABOLOID; }

	virtual bool light_has_shadow(RID p_light) const override { return false; }

	virtual VS::LightType light_get_type(RID p_light) const override { return VS::LIGHT_OMNI; }
	virtual AABB light_get_aabb(RID p_light) const override { return AABB(); }
	virtual float light_get_param(RID p_light, VS::LightParam p_param) override { return 0.0f; }
	virtual Color light_get_color(RID p_light) override { return Color(); }
	virtual bool light_get_use_gi(RID p_light) override { return false; }
	virtual uint64_t light_get_version(RID p_light) const override { return 0; }

	/* PROBE API */

	virtual RID reflection_probe_create() override { return RID(); }

	virtual void reflection_probe_set_update_mode(RID p_probe, VS::ReflectionProbeUpdateMode p_mode) override {}
	virtual void reflection_probe_set_resolution(RID p_probe, int p_resolution) override {}
	virtual void reflection_probe_set_intensity(RID p_probe, float p_intensity) override {}
	virtual void reflection_probe_set_interior_ambient(RID p_probe, const Color &p_ambient) override {}
	virtual void reflection_probe_set_interior_ambient_energy(RID p_probe, float p_energy) override {}
	virtual void reflection_probe_set_interior_ambient_probe_contribution(RID p_probe, float p_contrib) override {}
	virtual void reflection_probe_set_max_distance(RID p_probe, float p_distance) override {}
	virtual void reflection_probe_set_extents(RID p_probe, const Vector3 &p_extents) override {}
	virtual void reflection_probe_set_origin_offset(RID p_probe, const Vector3 &p_offset) override {}
	virtual void reflection_probe_set_as_interior(RID p_probe, bool p_enable) override {}
	virtual void reflection_probe_set_enable_box_projection(RID p_probe, bool p_enable) override {}
	virtual void reflection_probe_set_enable_shadows(RID p_probe, bool p_enable) override {}
	virtual void reflection_probe_set_cull_mask(RID p_probe, uint32_t p_layers) override {}

	virtual AABB reflection_probe_get_aabb(RID p_probe) const override { return AABB(); }
	virtual VS::ReflectionProbeUpdateMode reflection_probe_get_update_mode(RID p_probe) const override { return VisualServer::REFLECTION_PROBE_UPDATE_ONCE; }
	virtual uint32_t reflection_probe_get_cull_mask(RID p_probe) const override { return 0; }
	virtual Vector3 reflection_probe_get_extents(RID p_probe) const override { return Vector3(); }
	virtual Vector3 reflection_probe_get_origin_offset(RID p_probe) const override { return Vector3(); }
	virtual float reflection_probe_get_origin_max_distance(RID p_probe) const override { return 0.0f; }
	virtual bool reflection_probe_renders_shadows(RID p_probe) const override { return false; }

	virtual void base_update_dependency(RID p_base, RasterizerScene::InstanceBase *p_instance) override {}
	virtual void skeleton_update_dependency(RID p_base, RasterizerScene::InstanceBase *p_instance) override {}

	/* GI PROBE API */

	virtual RID gi_probe_create() override { return RID(); }

	virtual void gi_probe_allocate(RID p_gi_probe, const Transform &p_to_cell_xform, const AABB &p_aabb, const Vector3i &p_octree_size, const PoolVector<uint8_t> &p_octree_cells, const PoolVector<uint8_t> &p_data_cells, const PoolVector<uint8_t> &p_distance_field, const PoolVector<int> &p_level_counts) override {}

	virtual AABB gi_probe_get_bounds(RID p_gi_probe) const override { return AABB(); }
	virtual Vector3i gi_probe_get_octree_size(RID p_gi_probe) const override { return Vector3i(); }
	virtual PoolVector<uint8_t> gi_probe_get_octree_cells(RID p_gi_probe) const override { return PoolVector<uint8_t>(); }
	virtual PoolVector<uint8_t> gi_probe_get_data_cells(RID p_gi_probe) const override { return PoolVector<uint8_t>(); }
	virtual PoolVector<uint8_t> gi_probe_get_distance_field(RID p_gi_probe) const override { return PoolVector<uint8_t>(); }

	virtual PoolVector<int> gi_probe_get_level_counts(RID p_gi_probe) const override { return PoolVector<int>(); }
	virtual Transform gi_probe_get_to_cell_xform(RID p_gi_probe) const override { return Transform(); }

	virtual void gi_probe_set_dynamic_range(RID p_gi_probe, float p_range) override {}
	virtual float gi_probe_get_dynamic_range(RID p_gi_probe) const override { return 0.0f; }

	virtual void gi_probe_set_propagation(RID p_gi_probe, float p_range) override {}
	virtual float gi_probe_get_propagation(RID p_gi_probe) const override { return 0.0f; }

	virtual void gi_probe_set_energy(RID p_gi_probe, float p_energy) override {}
	virtual float gi_probe_get_energy(RID p_gi_probe) const override { return 0.0f; }

	virtual void gi_probe_set_ao(RID p_gi_probe, float p_ao) override {}
	virtual float gi_probe_get_ao(RID p_gi_probe) const override { return 0.0f; }

	virtual void gi_probe_set_ao_size(RID p_gi_probe, float p_strength) override {}
	virtual float gi_probe_get_ao_size(RID p_gi_probe) const override { return 0.0f; }

	virtual void gi_probe_set_bias(RID p_gi_probe, float p_bias) override {}
	virtual float gi_probe_get_bias(RID p_gi_probe) const override { return 0.0f; }

	virtual void gi_probe_set_normal_bias(RID p_gi_probe, float p_range) override {}
	virtual float gi_probe_get_normal_bias(RID p_gi_probe) const override { return 0.0f; }

	virtual void gi_probe_set_interior(RID p_gi_probe, bool p_enable) override {}
	virtual bool gi_probe_is_interior(RID p_gi_probe) const override { return false; }

	virtual void gi_probe_set_use_two_bounces(RID p_gi_probe, bool p_enable) override {}
	virtual bool gi_probe_is_using_two_bounces(RID p_gi_probe) const override { return false; }

	virtual void gi_probe_set_anisotropy_strength(RID p_gi_probe, float p_strength) override {}
	virtual float gi_probe_get_anisotropy_strength(RID p_gi_probe) const override { return 0.0f; }

	virtual uint32_t gi_probe_get_version(RID p_probe) override { return 0; }

	/* LIGHTMAP CAPTURE */
	struct LightmapCapture {
		PoolVector<LightmapCaptureOctree> octree;
		AABB bounds;
		Transform cell_xform;
		int cell_subdiv;
		float energy;
	};

	mutable RID_PtrOwner<LightmapCapture> lightmap_capture_data_owner;

	virtual RID lightmap_capture_create() override {
		LightmapCapture *capture = memnew(LightmapCapture);
		ERR_FAIL_COND_V(!capture, RID());
		return lightmap_capture_data_owner.make_rid(capture);
	}

	virtual void lightmap_capture_set_bounds(RID p_capture, const AABB &p_bounds) override {}
	virtual AABB lightmap_capture_get_bounds(RID p_capture) const override { return AABB(); }
	virtual void lightmap_capture_set_octree(RID p_capture, const PoolVector<uint8_t> &p_octree) override {}
	virtual PoolVector<uint8_t> lightmap_capture_get_octree(RID p_capture) const override {
		const LightmapCapture *capture = lightmap_capture_data_owner.getornull(p_capture);
		ERR_FAIL_COND_V(!capture, PoolVector<uint8_t>());
		return PoolVector<uint8_t>();
	}
	virtual void lightmap_capture_set_octree_cell_transform(RID p_capture, const Transform &p_xform) override {}
	virtual Transform lightmap_capture_get_octree_cell_transform(RID p_capture) const override { return Transform(); }
	virtual void lightmap_capture_set_octree_cell_subdiv(RID p_capture, int p_subdiv) override {}
	virtual int lightmap_capture_get_octree_cell_subdiv(RID p_capture) const override { return 0; }
	virtual void lightmap_capture_set_energy(RID p_capture, float p_energy) override {}
	virtual float lightmap_capture_get_energy(RID p_capture) const override { return 0.0f; }
	virtual const PoolVector<LightmapCaptureOctree> *lightmap_capture_get_octree_ptr(RID p_capture) const override {
		const LightmapCapture *capture = lightmap_capture_data_owner.getornull(p_capture);
		ERR_FAIL_COND_V(!capture, NULL);
		return &capture->octree;
	}

	/* PARTICLES */

	virtual RID particles_create() override { return RID(); }

	virtual void particles_set_emitting(RID p_particles, bool p_emitting) override {}
	virtual bool particles_get_emitting(RID p_particles) override { return false; }

	virtual void particles_set_amount(RID p_particles, int p_amount) override {}
	virtual void particles_set_lifetime(RID p_particles, float p_lifetime) override {}
	virtual void particles_set_one_shot(RID p_particles, bool p_one_shot) override {}
	virtual void particles_set_pre_process_time(RID p_particles, float p_time) override {}
	virtual void particles_set_explosiveness_ratio(RID p_particles, float p_ratio) override {}
	virtual void particles_set_randomness_ratio(RID p_particles, float p_ratio) override {}
	virtual void particles_set_custom_aabb(RID p_particles, const AABB &p_aabb) override {}
	virtual void particles_set_speed_scale(RID p_particles, float p_scale) override {}
	virtual void particles_set_use_local_coordinates(RID p_particles, bool p_enable) override {}
	virtual void particles_set_process_material(RID p_particles, RID p_material) override {}
	virtual void particles_set_fixed_fps(RID p_particles, int p_fps) override {}
	virtual void particles_set_fractional_delta(RID p_particles, bool p_enable) override {}
	virtual void particles_restart(RID p_particles) override {}

	virtual bool particles_is_inactive(RID p_particles) const override { return false; }

	virtual void particles_set_draw_order(RID p_particles, VS::ParticlesDrawOrder p_order) override {}

	virtual void particles_set_draw_passes(RID p_particles, int p_count) override {}
	virtual void particles_set_draw_pass_mesh(RID p_particles, int p_pass, RID p_mesh) override {}

	virtual void particles_request_process(RID p_particles) override {}
	virtual AABB particles_get_current_aabb(RID p_particles) override { return AABB(); }
	virtual AABB particles_get_aabb(RID p_particles) const override { return AABB(); }

	virtual void particles_set_emission_transform(RID p_particles, const Transform &p_transform) override {}

	virtual int particles_get_draw_passes(RID p_particles) const override { return 0; }
	virtual RID particles_get_draw_pass_mesh(RID p_particles, int p_pass) const override { return RID(); }

	/* RENDER TARGET */
	virtual RID render_target_create() override { return RID(); }
	virtual void render_target_set_position(RID p_render_target, int p_x, int p_y) override {}
	virtual void render_target_set_size(RID p_render_target, int p_width, int p_height) override {}
	virtual RID render_target_get_texture(RID p_render_target) override { return RID(); }
	virtual void render_target_set_external_texture(RID p_render_target, unsigned int p_texture_id) override {}
	virtual void render_target_set_flag(RID p_render_target, RenderTargetFlags p_flag, bool p_value) override {}
	virtual bool render_target_was_used(RID p_render_target) override { return false; }
	virtual void render_target_set_as_unused(RID p_render_target) override {}

	virtual void render_target_request_clear(RID p_render_target, const Color &p_clear_color) override {}
	virtual bool render_target_is_clear_requested(RID p_render_target) override { return false; }
	virtual Color render_target_get_clear_request_color(RID p_render_target) override { return Color(); }
	virtual void render_target_disable_clear_request(RID p_render_target) override {}
	virtual void render_target_do_clear_request(RID p_render_target) override {}

	virtual VS::InstanceType get_base_type(RID p_rid) const override {
		if (mesh_owner.owns(p_rid)) {
			return VS::INSTANCE_MESH;
		}

		return VS::INSTANCE_NONE;
	}

	virtual bool free(RID p_rid) override {
		if (texture_owner.owns(p_rid)) {
			// delete the texture
			DummyTexture *texture = texture_owner.getornull(p_rid);
			if (texture) {
				texture_owner.free(p_rid);
				memdelete(texture);
			}
		}
		return true;
	}

	virtual bool has_os_feature(const String &p_feature) const override { return false; }

	virtual void update_dirty_resources() override {}

	virtual void set_debug_generate_wireframes(bool p_generate) override {}

	virtual void render_info_begin_capture() override {}
	virtual void render_info_end_capture() override {}
	virtual int get_captured_render_info(VS::RenderInfo p_info) override { return 0; }

	virtual int get_render_info(VS::RenderInfo p_info) override { return 0; }
	String get_video_adapter_name() const { return String(); }
	String get_video_adapter_vendor() const { return String(); }

	virtual void capture_timestamps_begin() {}
	virtual void capture_timestamp(const String &p_name) {}
	virtual uint32_t get_captured_timestamps_count() const { return 0; }
	virtual uint64_t get_captured_timestamps_frame() const { return 0; }
	virtual uint64_t get_captured_timestamp_gpu_time(uint32_t p_index) const { return 0; }
	virtual uint64_t get_captured_timestamp_cpu_time(uint32_t p_index) const { return 0; }
	virtual String get_captured_timestamp_name(uint32_t p_index) const { return String(); }

	RasterizerStorageDummy() {}
	~RasterizerStorageDummy() {}
};

class RasterizerCanvasDummy : public RasterizerCanvas {
public:
	virtual TextureBindingID request_texture_binding(RID p_texture, RID p_normalmap, RID p_specular, VS::CanvasItemTextureFilter p_filter, VS::CanvasItemTextureRepeat p_repeat, RID p_multimesh) { return TextureBindingID(); }
	virtual void free_texture_binding(TextureBindingID p_binding) {}

	virtual PolygonID request_polygon(const Vector<int> &p_indices, const Vector<Point2> &p_points, const Vector<Color> &p_colors, const Vector<Point2> &p_uvs = Vector<Point2>(), const Vector<int> &p_bones = Vector<int>(), const Vector<float> &p_weights = Vector<float>()) { return PolygonID(); }
	virtual void free_polygon(PolygonID p_polygon) {}

	virtual void canvas_render_items(RID p_to_render_target, Item *p_item_list, const Color &p_modulate, Light *p_light_list, const Transform2D &p_canvas_transform) {}
	virtual void canvas_debug_viewport_shadows(Light *p_lights_with_shadow) {}

	virtual RID light_create() { return RID(); };
	virtual void light_set_texture(RID p_rid, RID p_texture) {}
	virtual void light_set_use_shadow(RID p_rid, bool p_enable, int p_resolution) {}
	virtual void light_update_shadow(RID p_rid, const Transform2D &p_light_xform, int p_light_mask, float p_near, float p_far, LightOccluderInstance *p_occluders) {}

	virtual RID occluder_polygon_create() { return RID(); }
	virtual void occluder_polygon_set_shape_as_lines(RID p_occluder, const PoolVector<Vector2> &p_lines) {}
	virtual void occluder_polygon_set_cull_mode(RID p_occluder, VS::CanvasOccluderPolygonCullMode p_mode) {}

	virtual void draw_window_margins(int *p_margins, RID *p_margin_textures) {}

	virtual bool free(RID p_rid) { return true; }
	virtual void update() {}

	RasterizerCanvasDummy() {}
	~RasterizerCanvasDummy() {}
};

class RasterizerDummy : public Rasterizer {
protected:
	RasterizerCanvasDummy canvas;
	RasterizerStorageDummy storage;
	RasterizerSceneDummy scene;

public:
	virtual RasterizerStorage *get_storage() { return &storage; }
	virtual RasterizerCanvas *get_canvas() { return &canvas; }
	virtual RasterizerScene *get_scene() { return &scene; }

	virtual void set_boot_image(const Ref<Image> &p_image, const Color &p_color, bool p_scale, bool p_use_filter = true) {}

	virtual void initialize() {}
	virtual void begin_frame(double frame_step) {}

	virtual void prepare_for_blitting_render_targets() {}
	virtual void blit_render_targets_to_screen(int p_screen, const BlitToScreen *p_render_targets, int p_amount) {}

	virtual void end_frame(bool p_swap_buffers) { OS::get_singleton()->swap_buffers(); }
	virtual void finalize() {}

	virtual bool is_low_end() const { return true; }

	static Error is_viable() {
		return OK;
	}

	static Rasterizer *_create_current() {
		return memnew(RasterizerDummy);
	}

	static void make_current() {
		_create_func = _create_current;
	}

	RasterizerDummy() {}
	~RasterizerDummy() {}
};

#endif // RASTERIZER_DUMMY_H
