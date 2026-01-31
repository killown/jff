# jff

**jff** stands for **“just for fun”**.

jff is a Wayland compositor forked from Wayfire, created as an experimental project focused on simplification and exploration.

This project is not intended for daily use.

## Motivation

Wayfire supports a wide range of hardware, rendering paths, and plugins. That flexibility introduces complexity and long-term maintenance cost.

jff explores what the compositor looks like when legacy constraints are removed and minimalism is prioritized.

The name reflects the intent: this is a personal project built just for fun.

## Goals

- Remove GLES2 completely
- Keep the codebase as small as possible
- Keep only what is personally used
- Favor clarity over compatibility
- Make refactoring easy and cheap
- Enable experimentation with modern rendering paths

## Non-goals

jff explicitly does not aim to:

- Be a daily driver
- Replace Wayfire
- Support legacy hardware
- Preserve plugin compatibility
- Be stable or distribution-ready

If you want a stable, feature-complete compositor, use Wayfire.

## Intended use

- Experimentation
- Refactoring
- Learning
- Personal hacking

Nothing more.

## Project status

Experimental and unstable.

Breaking changes are expected.  
Do not rely on this compositor for daily use.

## Upstream

jff is based on Wayfire.

All credit for the original architecture and implementation belongs to the Wayfire developers. This fork exists solely for experimentation.

## License

Same license as Wayfire.
