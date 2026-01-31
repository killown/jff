#pragma once

#include "wayfire/bindings-repository.hpp"
#include "wayfire/signal-definitions.hpp"
#include <memory>
#include <vector>
#include <wayfire/debug.hpp>

namespace wf {
template <class Option, class Callback> struct binding_t {
  wf::option_sptr_t<Option> activated_by; // Renamed from 'option' to match .cpp
  Callback *callback;
  std::vector<std::any> tags; // Added missing 'tags' member
};

template <class Option, class Callback>
using binding_container_t =
    std::vector<std::unique_ptr<binding_t<Option, Callback>>>;

struct bindings_repository_t::impl {
  void reparse_extensions();

  binding_container_t<wf::keybinding_t, key_callback> keys;
  binding_container_t<wf::keybinding_t, axis_callback> axes;
  binding_container_t<wf::buttonbinding_t, button_callback> buttons;
  binding_container_t<wf::activatorbinding_t, activator_callback> activators;

  wf::signal::connection_t<wf::reload_config_signal> on_config_reload =
      [=](wf::reload_config_signal *ev) { reparse_extensions(); };

  wf::wl_idle_call idle_reparse_bindings;
  int enabled = 1;
};
} // namespace wf
