#pragma once

#include <cstdint>

// External LoRa RF front end (FEM: PA + LNA between the SX1262 and the
// antenna). Only the Heltec V4 has one (LL_BOARD_LORA_FEM); on every other
// board these are no-ops and radio_power_dbm() returns its input unchanged.
//
// Behavior mirrors Meshtastic upstream src/mesh/LoRaFEMInterface.cpp.

namespace landlink::transport::lora::fem {

// Power the FEM and detect which part is fitted. Call before the radio's
// begin() so the RF path is up for the first RX.
void init();

// Switch the FEM to transmit (PA on) right before a TX, and back to receive
// (LNA on) right after it.
void tx_mode();
void rx_mode();

// SX1262 output power that yields `out_dbm` at the antenna, after the FEM's
// non-linear TX gain. Keeps the radiated power equal to boards without a PA.
int8_t radio_power_dbm(int8_t out_dbm);

} // namespace landlink::transport::lora::fem
