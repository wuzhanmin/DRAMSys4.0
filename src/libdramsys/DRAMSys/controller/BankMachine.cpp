/*
 * Copyright (c) 2019, RPTU Kaiserslautern-Landau
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
 * Author: Lukas Steiner
 */

#include "BankMachine.h"

#include "DRAMSys/configuration/Configuration.h"

#include <algorithm>

using namespace sc_core;
using namespace tlm;

namespace DRAMSys
{

BankMachine::BankMachine(const Configuration& config, const SchedulerIF& scheduler, Bank bank)
    : scheduler(scheduler), memSpec(*config.memSpec), bank(bank),
    bankgroup(BankGroup(bank.ID() / memSpec.banksPerGroup)), rank(Rank(bank.ID() / memSpec.banksPerRank)),
    refreshManagement(config.refreshManagement)
{}

CommandTuple::Type BankMachine::getNextCommand()
{
    return {nextCommand, currentPayload, SC_ZERO_TIME};
}

void BankMachine::update(Command command)
{
    switch (command)
    {
    case Command::ACT:
        state = State::Activated;
        openRow = ControllerExtension::getRow(*currentPayload);
        keepTrans = true;
        refreshManagementCounter++;
        break;
    case Command::PREPB: case Command::PRESB: case Command::PREAB:
        state = State::Precharged;
        keepTrans = false;
        break;
    case Command::RD: case Command::WR:
        currentPayload = nullptr;
        keepTrans = false;
        break;
    case Command::RDA: case Command::WRA:
        state = State::Precharged;
        currentPayload = nullptr;
        keepTrans = false;
        break;
    case Command::PDEA: case Command::PDEP: case Command::SREFEN:
        assert(!keepTrans);
        sleeping = true;
        break;
    case Command::REFPB: case Command::REFP2B: case Command::REFSB: case Command::REFAB:
        sleeping = false;
        blocked = false;

        if (refreshManagement)
        {
            if (refreshManagementCounter > memSpec.getRAADEC())
                refreshManagementCounter -= memSpec.getRAADEC();
            else
                refreshManagementCounter = 0;
        }
        break;
    case Command::RFMPB: case Command::RFMP2B: case Command::RFMSB: case Command::RFMAB:
        assert(!keepTrans);
        sleeping = false;
        blocked = false;

        if (refreshManagement)
        {
            if (refreshManagementCounter > memSpec.getRAAIMT())
                refreshManagementCounter -= memSpec.getRAAIMT();
            else
                refreshManagementCounter = 0;
        }
        break;
    case Command::PDXA: case Command::PDXP:
        assert(!keepTrans);
        sleeping = false;
        break;
    default:
        break;
    }
}

uint64_t BankMachine::getRefreshManagementCounter() const
{
    return refreshManagementCounter;
}

void BankMachine::block()
{
    blocked = true;
    nextCommand = Command::NOP;
}

Rank BankMachine::getRank() const
{
    return rank;
}

BankGroup BankMachine::getBankGroup() const
{
    return bankgroup;
}

Bank BankMachine::getBank() const
{
    return bank;
}

Row BankMachine::getOpenRow() const
{
    return openRow;
}

bool BankMachine::isIdle() const
{
    return (currentPayload == nullptr);
}

bool BankMachine::isActivated() const
{
    return state == State::Activated;
}

bool BankMachine::isPrecharged() const
{
    return state == State::Precharged;
}

BankMachineOpen::BankMachineOpen(const Configuration& config, const SchedulerIF& scheduler, Bank bank)
    : BankMachine(config, scheduler, bank) {}

void BankMachineOpen::evaluate()
{
    nextCommand = Command::NOP;

    if (!(sleeping || blocked))
    {
        tlm_generic_payload* newPayload = scheduler.getNextRequest(*this);
        if (newPayload == nullptr)
        {
            return;
        }
        else
        {
            assert(!keepTrans || currentPayload != nullptr);
            if (keepTrans)
            {
                if (ControllerExtension::getRow(*newPayload) == openRow)
                    currentPayload = newPayload;
            }
            else
            {
                currentPayload = newPayload;
            }

            if (state == State::Precharged) // bank precharged
                nextCommand = Command::ACT;
            else if (state == State::Activated)
            {
                if (ControllerExtension::getRow(*currentPayload) == openRow) // row hit
                {
                    assert(currentPayload->is_read() || currentPayload->is_write());
                    if (currentPayload->is_read())
                        nextCommand = Command::RD;
                    else
                        nextCommand = Command::WR;
                }
                else // row miss
                    nextCommand = Command::PREPB;
            }
        }
    }
}

BankMachineClosed::BankMachineClosed(const Configuration& config, const SchedulerIF& scheduler, Bank bank)
    : BankMachine(config, scheduler, bank) {}

void BankMachineClosed::evaluate()
{
    nextCommand = Command::NOP;

    if (!(sleeping || blocked))
    {
        tlm_generic_payload* newPayload = scheduler.getNextRequest(*this);
        if (newPayload == nullptr)
        {
            return;
        }
        else
        {
            assert(!keepTrans || currentPayload != nullptr);
            if (keepTrans)
            {
                if (ControllerExtension::getRow(*newPayload) == openRow)
                    currentPayload = newPayload;
            }
            else
            {
                currentPayload = newPayload;
            }

            if (state == State::Precharged) // bank precharged
                nextCommand = Command::ACT;
            else if (state == State::Activated)
            {
                assert(currentPayload->is_read() || currentPayload->is_write());
                if (currentPayload->is_read())
                    nextCommand = Command::RDA;
                else
                    nextCommand = Command::WRA;
            }
        }
    }
}

BankMachineOpenAdaptive::BankMachineOpenAdaptive(const Configuration& config, const SchedulerIF& scheduler, Bank bank)
    : BankMachine(config, scheduler, bank) {}

void BankMachineOpenAdaptive::evaluate()
{
    nextCommand = Command::NOP;

    if (!(sleeping || blocked))
    {
        tlm_generic_payload* newPayload = scheduler.getNextRequest(*this);
        if (newPayload == nullptr)
        {
            return;
        }
        else
        {
            assert(!keepTrans || currentPayload != nullptr);
            if (keepTrans)
            {
                if (ControllerExtension::getRow(*newPayload) == openRow)
                    currentPayload = newPayload;
            }
            else
            {
                currentPayload = newPayload;
            }

            if (state == State::Precharged) // bank precharged
                nextCommand = Command::ACT;
            else if (state == State::Activated)
            {
                if (ControllerExtension::getRow(*currentPayload) == openRow) // row hit
                {
                    if (scheduler.hasFurtherRequest(bank, currentPayload->get_command())
                        && !scheduler.hasFurtherRowHit(bank, openRow, currentPayload->get_command()))
                    {
                        assert(currentPayload->is_read() || currentPayload->is_write());
                        if (currentPayload->is_read())
                            nextCommand = Command::RDA;
                        else
                            nextCommand = Command::WRA;
                    }
                    else
                    {
                        assert(currentPayload->is_read() || currentPayload->is_write());
                        if (currentPayload->is_read())
                            nextCommand = Command::RD;
                        else
                            nextCommand = Command::WR;
                    }
                }
                else // row miss
                    nextCommand = Command::PREPB;
            }
        }
    }
}

BankMachineClosedAdaptive::BankMachineClosedAdaptive(const Configuration& config, const SchedulerIF& scheduler,
                                                     Bank bank)
    : BankMachine(config, scheduler, bank) {}

void BankMachineClosedAdaptive::evaluate()
{
    nextCommand = Command::NOP;

    if (!(sleeping || blocked))
    {
        tlm_generic_payload* newPayload = scheduler.getNextRequest(*this);
        if (newPayload == nullptr)
        {
            return;
        }
        else
        {
            assert(!keepTrans || currentPayload != nullptr);
            if (keepTrans)
            {
                if (ControllerExtension::getRow(*newPayload) == openRow)
                    currentPayload = newPayload;
            }
            else
            {
                currentPayload = newPayload;
            }

            if (state == State::Precharged) // bank precharged
                nextCommand = Command::ACT;
            else if (state == State::Activated)
            {
                if (ControllerExtension::getRow(*currentPayload) == openRow) // row hit
                {
                    if (scheduler.hasFurtherRowHit(bank, openRow, currentPayload->get_command()))
                    {
                        assert(currentPayload->is_read() || currentPayload->is_write());
                        if (currentPayload->is_read())
                            nextCommand = Command::RD;
                        else
                            nextCommand = Command::WR;
                    }
                    else
                    {
                        assert(currentPayload->is_read() || currentPayload->is_write());
                        if (currentPayload->is_read())
                            nextCommand = Command::RDA;
                        else
                            nextCommand = Command::WRA;
                    }
                }
                else // row miss, can happen when RD/WR mode is switched
                    nextCommand = Command::PREPB;
            }
        }
    }
}

} // namespace DRAMSys
