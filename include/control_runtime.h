#pragma once

#include "board_config.h"
#include "protocol.h"

// Kontrollprogram for én ESP32.
// Transportlaget bruker dette grensesnittet, ikke regulatorens interne variabler.
namespace control_runtime
{
    constexpr uint16_t SnapshotSize = proto::HeaderSize + 4 * cfg::SensorCount;

    // Opprett maskinvare, tidsplanlegger og watchdog. Kalles én gang fra setup().
    void begin();

    // Behandle stopp/lease og kjør én kontrollrunde hvis tidspunktet er nådd.
    void tick();

    // Bakgrunnsarbeid får bare starte når det finnes dette tidsrommet før neste runde.
    bool hasTimeFor(uint32_t requiredUs);

    // Legg en kommando i kø. Returnerer false hvis køen allerede er opptatt.
    bool enqueueCommand(proto::Command command,
                        uint16_t sequence,
                        const uint8_t* payload,
                        uint8_t payloadSize);

    void keepAlive();

    // Egen prioritert stoppkanal: overstyrer en vanlig kommando som venter.
    void requestStop(uint16_t sequence);

    // Frys målebildet. Pekeren er gyldig frem til neste captureSnapshot().
    const uint8_t* captureSnapshot();
} // namespace control_runtime
