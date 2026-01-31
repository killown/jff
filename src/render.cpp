#include "core/core-impl.hpp"
#include "wayfire/dassert.hpp"
#include "wayfire/nonstd/reverse.hpp"
#include "wayfire/opengl.hpp"
#include <drm_fourcc.h>
#include <wayfire/render.hpp>
#include <wayfire/scene-render.hpp>

wf::render_buffer_t::render_buffer_t(wlr_buffer *buffer,
                                     wf::dimensions_t size) {
  this->buffer = buffer;
  this->size = size;
}

wf::auxilliary_buffer_t::auxilliary_buffer_t(auxilliary_buffer_t &&other) {
  *this = std::move(other);
}

wf::auxilliary_buffer_t &
wf::auxilliary_buffer_t::operator=(auxilliary_buffer_t &&other) {
  if (&other == this) {
    return *this;
  }

  this->texture = std::exchange(other.texture, nullptr);
  this->buffer = std::exchange(other.buffer, {});
  return *this;
}

wf::auxilliary_buffer_t::~auxilliary_buffer_t() { free(); }

static const wlr_drm_format *
choose_format(wlr_renderer *renderer, wf::buffer_allocation_hints_t hints) {
  auto supported_render_formats = wlr_renderer_get_texture_formats(
      wf::get_core().renderer, renderer->render_buffer_caps);

  uint32_t fmt = hints.needs_alpha ? DRM_FORMAT_ARGB8888 : DRM_FORMAT_XRGB8888;
  return wlr_drm_format_set_get(supported_render_formats, fmt);
}

static wlr_box round_fbox_to_containing_box(wlr_fbox fbox) {
  return wlr_box{
      .x = (int)std::floor(fbox.x),
      .y = (int)std::floor(fbox.y),
      .width = (int)std::ceil(fbox.x + fbox.width) - (int)std::floor(fbox.x),
      .height = (int)std::ceil(fbox.y + fbox.height) - (int)std::floor(fbox.y),
  };
}

static wf::dimensions_t sanitize_buffer_size(wf::dimensions_t size,
                                             float max_allowed_size) {
  if ((size.width > max_allowed_size) || (size.height > max_allowed_size)) {
    float scale =
        std::min(max_allowed_size / size.width, max_allowed_size / size.height);
    size.width = std::ceil(size.width * scale);
    size.height = std::ceil(size.height * scale);
  }

  return size;
}

wf::buffer_reallocation_result_t
wf::auxilliary_buffer_t::allocate(wf::dimensions_t size, float scale,
                                  buffer_allocation_hints_t hints) {
  static wf::option_wrapper_t<int> max_buffer_size{
      "workarounds/max_buffer_size"};
  const int FALLBACK_MAX_BUFFER_SIZE = 4096;
  size.width = std::max(1.0f, std::ceil(size.width * scale));
  size.height = std::max(1.0f, std::ceil(size.height * scale));
  size = sanitize_buffer_size(size, max_buffer_size);

  if (buffer.get_size() == size) {
    return buffer_reallocation_result_t::SAME;
  }

  free();

  auto renderer = wf::get_core().renderer;
  auto format = choose_format(renderer, hints);
  if (!format) {
    return buffer_reallocation_result_t::FAILED;
  }

  buffer.buffer = wlr_allocator_create_buffer(wf::get_core_impl().allocator,
                                              size.width, size.height, format);

  if (!buffer.buffer) {
    size = sanitize_buffer_size(size, FALLBACK_MAX_BUFFER_SIZE);
    buffer.buffer = wlr_allocator_create_buffer(
        wf::get_core_impl().allocator, size.width, size.height, format);
  }

  if (!buffer.buffer) {
    return buffer_reallocation_result_t::FAILED;
  }

  buffer.size = size;
  return buffer_reallocation_result_t::REALLOCATED;
}

void wf::auxilliary_buffer_t::free() {
  if (texture) {
    wlr_texture_destroy(texture);
  }

  texture = NULL;

  if (buffer.get_buffer()) {
    wlr_buffer_drop(buffer.get_buffer());
  }

  buffer.buffer = NULL;
  buffer.size = {0, 0};
}

wlr_buffer *wf::auxilliary_buffer_t::get_buffer() const {
  return buffer.get_buffer();
}

wf::dimensions_t wf::auxilliary_buffer_t::get_size() const {
  return buffer.get_size();
}

wlr_texture *wf::auxilliary_buffer_t::get_texture() {
  wf::dassert(buffer.get_buffer(), "No buffer allocated yet!");
  if (!texture) {
    texture =
        wlr_texture_from_buffer(wf::get_core().renderer, buffer.get_buffer());
  }

  return texture;
}

wf::render_buffer_t wf::auxilliary_buffer_t::get_renderbuffer() const {
  return buffer;
}

void wf::render_buffer_t::do_blit(wlr_texture *src_wlr_tex, wlr_fbox src_box,
                                  wf::geometry_t dst_box,
                                  wlr_scale_filter_mode filter_mode) const {
  auto renderer = wf::get_core().renderer;
  auto target_buffer = this->get_buffer();

  if (!target_buffer) {
    return;
  }

  wlr_render_pass *pass =
      wlr_renderer_begin_buffer_pass(renderer, target_buffer, NULL);
  if (!pass) {
    return;
  }

  wlr_render_texture_options opts{};
  opts.texture = src_wlr_tex;
  opts.alpha = NULL;
  opts.blend_mode = WLR_RENDER_BLEND_MODE_NONE;
  opts.filter_mode = filter_mode;
  opts.transform = WL_OUTPUT_TRANSFORM_NORMAL;
  opts.clip = NULL;
  opts.src_box = src_box;
  opts.dst_box = dst_box;
  wlr_render_pass_add_texture(pass, &opts);
  wlr_render_pass_submit(pass);
}

void wf::render_buffer_t::blit(wf::auxilliary_buffer_t &source,
                               wlr_fbox src_box, wf::geometry_t dst_box,
                               wlr_scale_filter_mode filter_mode) const {
  if (wlr_texture *src_wlr_tex = source.get_texture()) {
    do_blit(src_wlr_tex, src_box, dst_box, filter_mode);
  }
}

void wf::render_buffer_t::blit(const wf::render_buffer_t &source,
                               wlr_fbox src_box, wf::geometry_t dst_box,
                               wlr_scale_filter_mode filter_mode) const {
  if (auto tex = wlr_texture_from_buffer(wf::get_core().renderer,
                                         source.get_buffer())) {
    do_blit(tex, src_box, dst_box, filter_mode);
    wlr_texture_destroy(tex);
  }
}

wf::render_target_t::render_target_t(const render_buffer_t &buffer)
    : render_buffer_t(buffer) {}
wf::render_target_t::render_target_t(const auxilliary_buffer_t &buffer)
    : render_buffer_t(buffer.get_buffer(), buffer.get_size()) {}

wf::render_target_t wf::render_target_t::translated(wf::point_t offset) const {
  render_target_t copy = *this;
  copy.geometry = copy.geometry + offset;
  return copy;
}

wlr_fbox
wf::render_target_t::framebuffer_box_from_geometry_box(wlr_fbox box) const {
  box.x -= this->geometry.x;
  box.y -= this->geometry.y;

  box = box * scale;

  wf::dimensions_t size = get_size();
  if (wl_transform & 1) {
    std::swap(size.width, size.height);
  }

  wlr_fbox result;
  wl_output_transform transform =
      wlr_output_transform_invert((wl_output_transform)wl_transform);

  wlr_fbox_transform(&result, &box, transform, size.width, size.height);

  if (subbuffer) {
    result = scale_fbox(
        {0.0, 0.0, (double)get_size().width, (double)get_size().height},
        geometry_to_fbox(subbuffer.value()), result);
  }

  return result;
}

wlr_box
wf::render_target_t::framebuffer_box_from_geometry_box(wlr_box box) const {
  wlr_fbox fbox = geometry_to_fbox(box);
  wlr_fbox scaled_fbox = framebuffer_box_from_geometry_box(fbox);
  return round_fbox_to_containing_box(scaled_fbox);
}

wf::region_t wf::render_target_t::framebuffer_region_from_geometry_region(
    const wf::region_t &region) const {
  wf::region_t result;
  for (const auto &rect : region) {
    result |= framebuffer_box_from_geometry_box(wlr_box_from_pixman_box(rect));
  }

  return result;
}

wlr_fbox
wf::render_target_t::geometry_fbox_from_framebuffer_box(wlr_fbox fb_box) const {
  if (subbuffer) {
    fb_box = scale_fbox(
        geometry_to_fbox(subbuffer.value()),
        {0.0, 0.0, (double)get_size().width, (double)get_size().height},
        fb_box);
  }

  wf::dimensions_t current_fb_dimensions = get_size();
  wlr_fbox result;
  wlr_fbox_transform(&result, &fb_box, (wl_output_transform)wl_transform,
                     current_fb_dimensions.width, current_fb_dimensions.height);

  if (scale != 0.0f) {
    result = result * (1.0 / scale);
  } else {
    return {0, 0, 0, 0};
  }

  result.x += this->geometry.x;
  result.y += this->geometry.y;
  return result;
}

wlr_box
wf::render_target_t::geometry_box_from_framebuffer_box(wlr_box fb_box) const {
  return round_fbox_to_containing_box(
      geometry_fbox_from_framebuffer_box(geometry_to_fbox(fb_box)));
}

wf::region_t wf::render_target_t::geometry_region_from_framebuffer_region(
    const wf::region_t &region) const {
  wf::region_t result;
  for (const auto &rect : region) {
    result |= geometry_box_from_framebuffer_box(wlr_box_from_pixman_box(rect));
  }

  return result;
}

wf::render_pass_t::render_pass_t(const render_pass_params_t &p) {
  this->params = p;
  this->params.renderer = p.renderer ?: wf::get_core().renderer;
  wf::dassert(p.target.get_buffer(),
              "Cannot run a render pass without a valid target!");
}

wf::region_t wf::render_pass_t::run(const wf::render_pass_params_t &params) {
  wf::render_pass_t pass{params};
  auto damage = pass.run_partial();
  pass.submit();
  return damage;
}

wf::region_t wf::render_pass_t::run_partial() {
  auto accumulated_damage = params.damage;

  if (wlr_renderer_is_gles2(params.renderer ?: wf::get_core().renderer)) {
    wf::gles::bind_render_buffer(params.target);
  }

  wf::region_t swap_damage = accumulated_damage;

  std::vector<wf::scene::render_instruction_t> instructions;
  if (params.instances) {
    for (auto &inst : *params.instances) {
      inst->schedule_instructions(instructions, params.target,
                                  accumulated_damage);
    }
  }

  this->pass = wlr_renderer_begin_buffer_pass(
      params.renderer ?: wf::get_core().renderer, params.target.get_buffer(),
      params.pass_opts);

  if (!pass) {
    return accumulated_damage;
  }

  if (params.flags & RPASS_CLEAR_BACKGROUND) {
    clear(accumulated_damage, params.background_color);
  }

  for (auto &instr : wf::reverse(instructions)) {
    instr.pass = this;
    instr.instance->render(instr);
  }

  return swap_damage;
}

wf::render_target_t wf::render_pass_t::get_target() const {
  return params.target;
}

wlr_renderer *wf::render_pass_t::get_wlr_renderer() const {
  return params.renderer;
}

wlr_render_pass *wf::render_pass_t::get_wlr_pass() { return pass; }

void wf::render_pass_t::clear(const wf::region_t &region,
                              const wf::color_t &color) {
  auto box = wf::construct_box({0, 0}, params.target.get_size());
  auto damage = params.target.framebuffer_region_from_geometry_region(region);

  wlr_render_rect_options opts;
  opts.blend_mode = WLR_RENDER_BLEND_MODE_NONE;
  opts.box = box;
  opts.clip = damage.to_pixman();
  opts.color = {
      .r = static_cast<float>(color.r),
      .g = static_cast<float>(color.g),
      .b = static_cast<float>(color.b),
      .a = static_cast<float>(color.a),
  };

  wlr_render_pass_add_rect(pass, &opts);
}

void wf::render_pass_t::add_texture(const wf::texture_t &texture,
                                    const wf::render_target_t &adjusted_target,
                                    const wlr_fbox &geometry,
                                    const wf::region_t &damage, float alpha) {

  wf::region_t fb_damage =
      adjusted_target.framebuffer_region_from_geometry_region(damage);

  wlr_render_texture_options opts{};
  opts.texture = texture.texture;
  opts.alpha = &alpha;
  opts.blend_mode = WLR_RENDER_BLEND_MODE_PREMULTIPLIED;

  const auto preferred_filter =
      (adjusted_target.scale == (int)adjusted_target.scale)
          ? WLR_SCALE_FILTER_NEAREST
          : WLR_SCALE_FILTER_BILINEAR;
  opts.filter_mode = texture.filter_mode.value_or(preferred_filter);
  opts.transform = wlr_output_transform_compose(
      wlr_output_transform_invert(texture.transform),
      adjusted_target.wl_transform);
  opts.clip = fb_damage.to_pixman();
  opts.src_box = texture.source_box.value_or(wlr_fbox{0, 0, 0, 0});
  opts.dst_box = fbox_to_geometry(
      adjusted_target.framebuffer_box_from_geometry_box(geometry));
  wlr_render_pass_add_texture(get_wlr_pass(), &opts);
}

void wf::render_pass_t::add_rect(const wf::color_t &color,
                                 const wf::render_target_t &adjusted_target,
                                 const wlr_fbox &geometry,
                                 const wf::region_t &damage) {
  wf::region_t fb_damage =
      adjusted_target.framebuffer_region_from_geometry_region(damage);
  wlr_render_rect_options opts;
  opts.color = {
      .r = static_cast<float>(color.r),
      .g = static_cast<float>(color.g),
      .b = static_cast<float>(color.b),
      .a = static_cast<float>(color.a),
  };
  opts.blend_mode = WLR_RENDER_BLEND_MODE_PREMULTIPLIED;
  opts.clip = fb_damage.to_pixman();
  opts.box = fbox_to_geometry(
      adjusted_target.framebuffer_box_from_geometry_box(geometry));
  wf::dassert(opts.box.width >= 0);
  wf::dassert(opts.box.height >= 0);
  wlr_render_pass_add_rect(pass, &opts);
}

void wf::render_pass_t::add_texture(const wf::texture_t &texture,
                                    const wf::render_target_t &adjusted_target,
                                    const wf::geometry_t &geometry,
                                    const wf::region_t &damage, float alpha) {
  add_texture(texture, adjusted_target, geometry_to_fbox(geometry), damage,
              alpha);
}

void wf::render_pass_t::add_rect(const wf::color_t &color,
                                 const wf::render_target_t &adjusted_target,
                                 const wf::geometry_t &geometry,
                                 const wf::region_t &damage) {
  add_rect(color, adjusted_target, geometry_to_fbox(geometry), damage);
}

bool wf::render_pass_t::submit() {
  bool status = wlr_render_pass_submit(pass);
  this->pass = NULL;
  return status;
}

wf::render_pass_t::~render_pass_t() {
  if (this->pass) {
    LOGW("Dropping unsubmitted render pass!");
  }
}

wf::render_pass_t::render_pass_t(render_pass_t &&other) {
  *this = std::move(other);
}

wf::render_pass_t &wf::render_pass_t::operator=(render_pass_t &&other) {
  if (this == &other) {
    return *this;
  }

  this->pass = other.pass;
  other.pass = NULL;
  this->params = other.params;
  return *this;
}

bool wf::render_pass_t::prepare_gles_subpass() {
  return prepare_gles_subpass(params.target);
}

bool wf::render_pass_t::prepare_gles_subpass(
    const wf::render_target_t &target) {
  bool is_gles = wf::gles::run_in_context_if_gles([&] {
    GL_CALL(glEnable(GL_BLEND));
    GL_CALL(glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
    wf::gles::bind_render_buffer(target);
  });

  return is_gles;
}

void wf::render_pass_t::finish_gles_subpass() {
  wf::gles::bind_render_buffer(params.target);
  GL_CALL(glDisable(GL_SCISSOR_TEST));
}
