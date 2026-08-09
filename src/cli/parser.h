#pragma once

#include <string>
#include "config/config.h"

int ParseAndDispatch(int argc, char* argv[], const Config& cfg);
int ParseElevatedCommands(int argc, char* argv[], Config& cfg, const std::string& configPath);
