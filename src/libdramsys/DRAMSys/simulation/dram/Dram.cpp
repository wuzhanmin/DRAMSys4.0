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
 *    Robert Gernhardt
 *    Matthias Jung
 *    Peter Ehses
 *    Eder F. Zulian
 *    Felipe S. Prado
 *    Derek Christ
 */

#include "Dram.h"

#include "DRAMSys/common/DebugManager.h"

#ifdef DRAMPOWER
#include "LibDRAMPower.h"
#endif

#include <cassert>
#include <cstdint>
#include <cstdlib>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/mman.h>
#endif

using namespace sc_core;
using namespace tlm;

#ifdef DRAMPOWER
using namespace DRAMPower;
#endif

namespace DRAMSys
{


Dram::Dram(const sc_module_name& name, const Configuration& config)
    : sc_module(name), memSpec(*config.memSpec), tSocket("socket"), storeMode(config.storeMode),
    powerAnalysis(config.powerAnalysis), useMalloc(config.useMalloc)
{
    uint64_t channelSize = memSpec.getSimMemSizeInBytes() / memSpec.numberOfChannels;
    if (storeMode == Configuration::StoreMode::Store)
    {
        if (useMalloc)
        {
            memory = (unsigned char *)malloc(channelSize);
            if (!memory)
                SC_REPORT_FATAL(this->name(), "Memory allocation failed");
        }
        else
        {
            // allocate and model storage of one DRAM channel using memory map
            #ifdef _WIN32
                SC_REPORT_FATAL("Dram", "On Windows Storage is not yet supported");
                memory = 0; // FIXME
            #else
                memory = (unsigned char *)mmap(nullptr, channelSize,
                        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_NORESERVE, -1, 0);
            #endif
        }
    }

    tSocket.register_nb_transport_fw(this, &Dram::nb_transport_fw);
    tSocket.register_b_transport(this, &Dram::b_transport);
    tSocket.register_transport_dbg(this, &Dram::transport_dbg);
}

Dram::~Dram()
{
    if (useMalloc)
        free(memory);
}

void Dram::reportPower()
{
#ifdef DRAMPOWER
    DRAMPower->calcEnergy();

    // Print the final total energy and the average power for
    // the simulation:
    std::cout << name() << std::string("  Total Energy:   ")
         << std::fixed << std::setprecision( 2 )
         << DRAMPower->getEnergy().total_energy
         * memSpec.devicesPerRank
         << std::string(" pJ")
         << std::endl;

    std::cout << name() << std::string("  Average Power:  ")
         << std::fixed << std::setprecision( 2 )
         << DRAMPower->getPower().average_power
         * memSpec.devicesPerRank
         << std::string(" mW") << std::endl;
#endif
}

tlm_sync_enum Dram::nb_transport_fw(tlm_generic_payload& trans, tlm_phase& phase, sc_time& delay)
{
    assert(phase >= BEGIN_RD && phase <= END_SREF);

#ifdef DRAMPOWER
    if (powerAnalysis)
    {
        int bank = static_cast<int>(ControllerExtension::getBank(trans).ID());
        int64_t cycle = std::lround((sc_time_stamp() + delay) / memSpec.tCK);
        DRAMPower->doCommand(phaseToDRAMPowerCommand(phase), bank, cycle);
    }
#endif

    if (storeMode == Configuration::StoreMode::Store)
    {
        if (phase == BEGIN_RD || phase == BEGIN_RDA)
        {
            unsigned char* phyAddr = memory + trans.get_address();
            memcpy(trans.get_data_ptr(), phyAddr, trans.get_data_length());
        }
        else if (phase == BEGIN_WR || phase == BEGIN_WRA)
        {
            unsigned char* phyAddr = memory + trans.get_address();
            memcpy(phyAddr, trans.get_data_ptr(), trans.get_data_length());
        }
    }

    return TLM_ACCEPTED;
}

unsigned int Dram::transport_dbg(tlm_generic_payload& trans)
{
    PRINTDEBUGMESSAGE(name(), "transport_dgb");

    // TODO: This part is not tested yet, neither with traceplayers nor with GEM5 coupling
    if (storeMode == Configuration::StoreMode::NoStorage)
    {
        SC_REPORT_FATAL("DRAM", "Debug Transport is used in combination with NoStorage");
    }
    else
    {
        tlm_command cmd = trans.get_command();
        unsigned char* ptr = trans.get_data_ptr();
        unsigned int len = trans.get_data_length();

        if (cmd == TLM_READ_COMMAND)
        {
            if (storeMode == Configuration::StoreMode::Store)
            {
                unsigned char* phyAddr = memory + trans.get_address();
                memcpy(ptr, phyAddr, trans.get_data_length());
            }
            else
            {
                //ememory[bank]->load(trans);
                SC_REPORT_FATAL("DRAM", "Debug transport not supported with error model yet.");
            }
        }
        else if (cmd == TLM_WRITE_COMMAND)
        {
            if (storeMode == Configuration::StoreMode::Store)
            {
                unsigned char* phyAddr = memory + trans.get_address();
                memcpy(phyAddr, ptr, trans.get_data_length());
            }
            else
            {
                //ememory[bank]->store(trans);
                SC_REPORT_FATAL("DRAM", "Debug transport not supported with error model yet.");
            }
        }
        return len;
    }
    return 0;
}

void Dram::b_transport(tlm_generic_payload& trans, sc_time& delay)
{
    static bool printedWarning = false;

    if (!printedWarning)
    {
        SC_REPORT_WARNING("DRAM", BLOCKING_WARNING.data());
        printedWarning = true;
    }

    if (storeMode == Configuration::StoreMode::Store)
    {
        if (trans.is_read())
        {
            unsigned char* phyAddr = memory + trans.get_address();
            memcpy(trans.get_data_ptr(), phyAddr, trans.get_data_length());
        }
        else
        {
            unsigned char* phyAddr = memory + trans.get_address();
            memcpy(phyAddr, trans.get_data_ptr(), trans.get_data_length());
        }
    }
    else if (storeMode != Configuration::StoreMode::NoStorage)
    {
        SC_REPORT_FATAL("DRAM", "Blocking transport not supported with error model yet.");
    }
}

} // namespace DRAMSys
