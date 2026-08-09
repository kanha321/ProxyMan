#pragma once

#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>

bool IsElevated();
bool RelaunchElevated(int argc, char* argv[]);
