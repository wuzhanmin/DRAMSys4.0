/*
 * Copyright (c) 2015, RPTU Kaiserslautern-Landau
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors:
 *    Janik Schlemminger
 *    Matthias Jung
 *    Lukas Steiner
 */
#ifndef COMMAND_H
#define COMMAND_H

#ifdef DRAMPOWER
#include "MemCommand.h"
#endif

#include <string>
#include <vector>
#include <tuple>
#include <systemc>
#include <tlm>

namespace DRAMSys
{

// DO NOT CHANGE THE ORDER!

//                     BEGIN_REQ      // 1
//                     END_REQ        // 2
//                     BEGIN_RESP     // 3
//                     END_RESP       // 4

DECLARE_EXTENDED_PHASE(BEGIN_NOP);    // 5
DECLARE_EXTENDED_PHASE(BEGIN_RD);     // 6
DECLARE_EXTENDED_PHASE(BEGIN_WR);     // 7
DECLARE_EXTENDED_PHASE(BEGIN_RDA);    // 8
DECLARE_EXTENDED_PHASE(BEGIN_WRA);    // 9
DECLARE_EXTENDED_PHASE(BEGIN_ACT);    // 10
DECLARE_EXTENDED_PHASE(BEGIN_PREPB);  // 11
DECLARE_EXTENDED_PHASE(BEGIN_REFPB);  // 12
DECLARE_EXTENDED_PHASE(BEGIN_RFMPB);  // 13
DECLARE_EXTENDED_PHASE(BEGIN_REFP2B); // 14
DECLARE_EXTENDED_PHASE(BEGIN_RFMP2B); // 15
DECLARE_EXTENDED_PHASE(BEGIN_PRESB);  // 16
DECLARE_EXTENDED_PHASE(BEGIN_REFSB);  // 17
DECLARE_EXTENDED_PHASE(BEGIN_RFMSB);  // 18
DECLARE_EXTENDED_PHASE(BEGIN_PREAB);  // 19
DECLARE_EXTENDED_PHASE(BEGIN_REFAB);  // 20
DECLARE_EXTENDED_PHASE(BEGIN_RFMAB);  // 21

DECLARE_EXTENDED_PHASE(BEGIN_PDNA);   // 22
DECLARE_EXTENDED_PHASE(BEGIN_PDNP);   // 23
DECLARE_EXTENDED_PHASE(BEGIN_SREF);   // 24

DECLARE_EXTENDED_PHASE(END_PDNA);     // 25
DECLARE_EXTENDED_PHASE(END_PDNP);     // 26
DECLARE_EXTENDED_PHASE(END_SREF);     // 27

#ifdef DRAMPOWER
DRAMPower::MemCommand::cmds phaseToDRAMPowerCommand(tlm::tlm_phase);
#endif

bool phaseHasDataStrobe(tlm::tlm_phase phase);
bool isPowerDownEntryPhase(tlm::tlm_phase phase);
bool isPowerDownExitPhase(tlm::tlm_phase phase);
bool isFixedCommandPhase(tlm::tlm_phase phase);
bool isRefreshCommandPhase(tlm::tlm_phase phase);

class Command
{
public:
    enum Type : uint8_t
    {
        NOP = 0, // 0
        RD,      // 1
        WR,      // 2
        RDA,     // 3
        WRA,     // 4
        ACT,     // 5
        PREPB,   // 6
        REFPB,   // 7
        RFMPB,   // 8
        REFP2B,  // 9
        RFMP2B,  // 10
        PRESB,   // 11
        REFSB,   // 12
        RFMSB,   // 13
        PREAB,   // 14
        REFAB,   // 15
        RFMAB,   // 16
        PDEA,    // 17
        PDEP,    // 18
        SREFEN,  // 19
        PDXA,    // 20
        PDXP,    // 21
        SREFEX,  // 22
        END_ENUM // 23, To mark the end of this enumeration
    };

private:
    Type type;

public:
    Command() = default;
    Command(Type type);
    Command(tlm::tlm_phase phase);

    [[nodiscard]] std::string toString() const;
    [[nodiscard]] tlm::tlm_phase toPhase() const;
    static unsigned numberOfCommands();
    [[nodiscard]] bool isBankCommand() const;
    [[nodiscard]] bool is2BankCommand() const;
    [[nodiscard]] bool isGroupCommand() const;
    [[nodiscard]] bool isRankCommand() const;
    [[nodiscard]] bool isCasCommand() const;
    [[nodiscard]] bool isRasCommand() const;

    constexpr operator uint8_t() const
    {
        return type;
    }
};

struct CommandTuple
{
    using Type = std::tuple<DRAMSys::Command, tlm::tlm_generic_payload*, sc_core::sc_time>;
    enum Accessor
    {
        Command = 0,
        Payload = 1,
        Timestamp = 2
    };
};

using ReadyCommands = std::vector<CommandTuple::Type>;

} // namespace DRAMSys

#endif // COMMAND_H
