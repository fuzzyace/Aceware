// Forced include for the Aceware build.
// Supplies the compile-time defines the Geode SDK would normally pass through
// the CMake target, kept in one header so quoting never gets mangled by a
// response file.
#pragma once

#ifndef GEODE_MOD_ID
#define GEODE_MOD_ID "ace.aceware"
#endif

#ifndef GEODE_COMP_GD_VERSION
#define GEODE_COMP_GD_VERSION 22081
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif
