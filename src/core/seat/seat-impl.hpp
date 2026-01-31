#pragma once

#include <set>
#include <wayfire/config/section.hpp>
#include <wayfire/nonstd/wlroots-full.hpp>
#include <wayfire/seat.hpp>
#include <wayfire/signal-definitions.hpp>

#include "wayfire/input-device.hpp"
#include "wayfire/output.hpp"
#include "wayfire/scene-input.hpp"
#include "wayfire/scene.hpp"
#include "wayfire/signal-provider.hpp"
#include "wayfire/toplevel-view.hpp"
#include "wayfire/util.hpp"

namespace wf {
struct cursor_t;
class keyboard_t;

class input_device_impl_t : public wf::input_device_t {
public:
  input_device_impl_t(wlr_input_device *dev);
  virtual ~input_device_impl_t();

  wf::wl_listener_wrapper on_destroy;
  virtual void update_options() {}

  bool is_im_keyboard = false;

  virtual void
  reconfigure_device(std::shared_ptr<wf::config::section_t> device_section) {}

  void map_to_output(std::shared_ptr<wf::config::section_t> device_section);
};

class pointer_t;
class drag_icon_t;

struct seat_t::impl {
  uint32_t get_modifiers();
  void break_mod_bindings();

  void set_keyboard_focus(wf::scene::node_ptr keyboard_focus,
                          wf::keyboard_focus_reason reason);
  wf::scene::node_ptr keyboard_focus;
  std::multiset<uint32_t> pressed_keys;
  void transfer_grab(wf::scene::node_ptr new_focus);

  void set_keyboard(wf::keyboard_t *kbd);

  wlr_seat *seat = nullptr;
  std::unique_ptr<cursor_t> cursor;
  std::unique_ptr<pointer_t> lpointer;

  std::unique_ptr<wf::drag_icon_t> drag_icon;
  bool drag_active = false;

  void update_drag_icon();

  wf::keyboard_t *current_keyboard = nullptr;

  wf::wl_listener_wrapper request_start_drag, start_drag, end_drag,
      request_set_selection, request_set_primary_selection;

  wf::signal::connection_t<wf::input_device_added_signal> on_new_device;
  wf::signal::connection_t<wf::input_device_removed_signal> on_remove_device;

  std::vector<std::unique_ptr<wf::keyboard_t>> keyboards;

  void validate_drag_request(wlr_seat_request_start_drag_event *ev);

  void update_capabilities();

  void force_release_keys();

  wf::wl_listener_wrapper on_wlr_keyboard_grab_end;
  wf::wl_listener_wrapper on_wlr_pointer_grab_end;

  uint64_t last_timestamp = 0;
  wf::output_t *active_output = nullptr;
  std::weak_ptr<wf::toplevel_view_interface_t> _last_active_toplevel;
  std::weak_ptr<wf::view_interface_t> _last_active_view;
  wf::signal::connection_t<wf::view_minimized_signal> on_view_minimized;
  wf::signal::connection_t<scene::root_node_update_signal> on_root_node_updated;

  void check_update_active_view(wf::scene::node_ptr new_focus);
  void set_activated_view(wayfire_toplevel_view view);

  uint32_t last_press_release_serial = 0;
};
} // namespace wf

wf::pointf_t get_node_local_coords(wf::scene::node_t *node,
                                   const wf::pointf_t &point);

bool is_grabbed_node_alive(wf::scene::node_ptr node);
