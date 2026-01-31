#pragma once

#include "wayfire/core.hpp"
#include "wayfire/output.hpp"
#include "wayfire/scene-input.hpp"
#include "wayfire/scene-operations.hpp"
#include "wayfire/scene.hpp"
#include <memory>
#include <string>
#include <wayfire/debug.hpp>
#include <wayfire/seat.hpp>

namespace wf {
namespace scene {
/**
 * A scene node which can be used to implement input grab on a particular
 * output.
 */
class grab_node_t : public node_t {
  std::string name;
  wf::output_t *output;
  keyboard_interaction_t *keyboard = nullptr;
  pointer_interaction_t *pointer = nullptr;
  node_flags_bitmask_t m_flags = 0;

public:
  grab_node_t(std::string name, wf::output_t *output,
              keyboard_interaction_t *keyboard = NULL,
              pointer_interaction_t *pointer = NULL)
      : node_t(false), name(name), output(output), keyboard(keyboard),
        pointer(pointer) {}

  node_flags_bitmask_t flags() const override {
    return node_t::flags() | m_flags;
  }

  void set_additional_flags(node_flags_bitmask_t add_flags) {
    this->m_flags = add_flags;
  }

  std::optional<input_node_t> find_node_at(const wf::pointf_t &at) override {
    if (output->get_layout_geometry() & at) {
      input_node_t result;
      result.node = this;
      result.local_coords = to_local(at);
      return result;
    }

    return {};
  }

  wf::keyboard_focus_node_t keyboard_refocus(wf::output_t *output) override {
    if (output != this->output) {
      return wf::keyboard_focus_node_t{};
    }

    return wf::keyboard_focus_node_t{
        .node = this,
        .importance = focus_importance::REGULAR,
        .allow_focus_below = false,
    };
  }

  std::string stringify() const override {
    return name + "-input-grab " +
           std::string(output ? output->to_string() : "null");
  }

  keyboard_interaction_t &keyboard_interaction() override {
    return keyboard ? *keyboard : node_t::keyboard_interaction();
  }

  pointer_interaction_t &pointer_interaction() override {
    return pointer ? *pointer : node_t::pointer_interaction();
  }
};
} // namespace scene

/**
 * A helper class for managing input grabs on an output.
 */
class input_grab_t {
  wf::output_t *output;
  std::shared_ptr<scene::grab_node_t> grab_node;

public:
  input_grab_t(std::string name, wf::output_t *output,
               keyboard_interaction_t *keyboard = NULL,
               pointer_interaction_t *pointer = NULL) {
    this->output = output;
    grab_node =
        std::make_shared<scene::grab_node_t>(name, output, keyboard, pointer);
  }

  void set_wants_raw_input(bool wants_raw) {
    grab_node->set_additional_flags(
        wants_raw ? (uint64_t)wf::scene::node_flags::RAW_INPUT : 0);
  }

  bool is_grabbed() const { return grab_node->parent() != nullptr; }

  void grab_input(wf::scene::layer layer_below) {
    wf::dassert(grab_node->parent() == nullptr, "Trying to grab twice!");

    auto &root = wf::get_core().scene();
    auto children = root->get_children();

    auto idx = std::find(children.begin(), children.end(),
                         root->layers[(int)layer_below]);

    wf::dassert(idx != children.end(), "Could not find node for a layer: " +
                                           std::to_string((int)layer_below));
    children.insert(idx, grab_node);
    root->set_children_list(children);
    if (output == wf::get_core().seat->get_active_output()) {
      wf::get_core().transfer_grab(grab_node);
    }

    scene::update(root, scene::update_flag::CHILDREN_LIST |
                            scene::update_flag::REFOCUS);

    wf::get_core().set_cursor("default");
  }

  void regrab_input() {
    const auto &check_focus = [&](wf::scene::node_ptr focused) {
      return (focused == nullptr) || (focused == grab_node);
    };

    if ((wf::get_core().seat->get_active_node() == grab_node) &&
        check_focus(wf::get_core().get_cursor_focus())) {
      return;
    }

    if (output == wf::get_core().seat->get_active_output()) {
      wf::get_core().transfer_grab(grab_node);
    }

    scene::update(wf::get_core().scene(), scene::update_flag::REFOCUS);
  }

  void ungrab_input() {
    if (grab_node->parent()) {
      wf::scene::remove_child(grab_node, scene::update_flag::REFOCUS);
    }
  }
};
} // namespace wf
