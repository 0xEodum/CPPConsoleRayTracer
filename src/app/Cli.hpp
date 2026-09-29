#pragma once

namespace crt::app {

/// Entry point shared by main(): parses the command line and runs the interactive application
/// or one of the headless commands (render2d, render3d, bench, list).
int runCli(int argc, char** argv);

}  // namespace crt::app
