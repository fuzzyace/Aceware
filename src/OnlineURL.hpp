// Aceware - online counter disabled.
// The upstream project keeps the server URL in this private header; Aceware
// ships with the online counter switched off entirely, so the URL is empty and
// every network call in main.cpp compiles out at runtime.
#pragma once
constexpr char const* ONLINE_URL = "";
