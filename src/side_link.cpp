#include <Arduino.h>
#include <Wire.h>
#include "side_config.h"
#include "side_link.h"

namespace
{
    sideproto::Sample latestSample;
    sideproto::Sample pendingSample;
    uint32_t nextPollUs = 0;
    uint32_t transferStartedUs = 0;
    uint32_t lastTransferStartedUs = 0;
    uint16_t sequence = 0;
    // Hvert kall gjør ett steg, slik at hovedsløyfen kan prioritere reguleringen.
    enum class TransferState : uint8_t
    {
        Idle,
        SelectPage,
        ReadPage
    };

    TransferState transferState = TransferState::Idle;
    uint8_t requestedPage = 0;
    constexpr uint32_t TransferDeadlineUs = 50000;
    bool haveSample = false;

    bool send(const uint8_t* data, uint8_t size)
    {
        Wire.beginTransmission(sidecfg::Address);
        Wire.write(data, size);
        bool ok = Wire.endTransmission() == 0;
        if (Wire.getWireTimeoutFlag())
        {
            Wire.clearWireTimeoutFlag();
            ok = false;
        }
        return ok;
    }
} // namespace

namespace side_link
{
    const sideproto::Sample& latest()
    {
        return latestSample;
    }

    uint16_t ageMs(uint32_t now)
    {
        return haveSample ? sideproto::age(latestSample, lastTransferStartedUs, now) : 65535;
    }

    bool fresh(uint32_t now)
    {
        return haveSample && latestSample.valid && ageMs(now) <= sidecfg::MaxAgeMs;
    }

    void service(uint32_t now)
    {
        if (!sidecfg::Enabled)
        {
            return;
        }
        if (transferState != TransferState::Idle &&
            uint32_t(now - transferStartedUs) > TransferDeadlineUs)
        {
            transferState = TransferState::Idle;
        }
        if (transferState == TransferState::Idle)
        {
            if (int32_t(now - nextPollUs) < 0)
            {
                return;
            }
            uint8_t command[3] = {sideproto::Latch};
            proto::put16(command + 1, ++sequence);
            transferStartedUs = now;
            nextPollUs = now + sidecfg::PollPeriodUs;
            requestedPage = 0;
            if (send(command, 3))
            {
                transferState = TransferState::SelectPage;
            }
        }
        else if (transferState == TransferState::SelectPage)
        {
            uint8_t select[2] = {sideproto::Select, requestedPage};
            transferState = send(select, 2) ? TransferState::ReadPage : TransferState::Idle;
        }
        else
        {
            uint8_t packet[32];
            uint8_t n = Wire.requestFrom(sidecfg::Address, uint8_t(32));
            uint8_t i = 0;
            while (Wire.available() && i < 32)
            {
                packet[i++] = Wire.read();
            }
            bool timeout = Wire.getWireTimeoutFlag();
            Wire.clearWireTimeoutFlag();
            if (timeout || n != 32 ||
                !sideproto::accept(packet, i, requestedPage, sequence, pendingSample))
            {
                transferState = TransferState::Idle;
                return;
            }
            if (pendingSample.count != sidecfg::Count)
            {
                transferState = TransferState::Idle;
                return;
            }
            if (++requestedPage >=
                (pendingSample.count + sideproto::PerPage - 1) / sideproto::PerPage)
            {
                latestSample = pendingSample;
                lastTransferStartedUs = transferStartedUs;
                haveSample = true;
                transferState = TransferState::Idle;
            }
            else
            {
                transferState = TransferState::SelectPage;
            }
        }
    }
} // namespace side_link
