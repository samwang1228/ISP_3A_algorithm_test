#pragma once

#include <cstdint>

/**
 * @file simulation.h
 * @brief Entry point for the simulation (no-image) 3A demo.
 */

namespace isp3a {

/**
 * @brief Run the simulation-mode 3A loop.
 *
 * Simulation mode uses a toy scene + sensor model to produce frame-to-frame
 * statistics, then runs AE/AWB/AF controllers to show convergence.
 *
 * @param iterations Number of frames to simulate.
 * @param seed Random seed for noise in the simulator.
 * @param csv Whether to print CSV instead of human-readable table.
 * @param header Whether to print the header line(s).
 * @return Process exit code (0 on success).
 */
int runSimulation(int iterations, uint32_t seed, bool csv, bool header);

} // namespace isp3a
