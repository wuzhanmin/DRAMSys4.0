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

#include "ControllerRecordable.h"

#include "DRAMSys/controller/scheduler/SchedulerIF.h"

using namespace sc_core;
using namespace tlm;

namespace DRAMSys
{

ControllerRecordable::ControllerRecordable(const sc_module_name& name, const Configuration& config,
                                           const AddressDecoder& addressDecoder, TlmRecorder& tlmRecorder)
    : Controller(name, config, addressDecoder), tlmRecorder(tlmRecorder),
    activeTimeMultiplier(config.memSpec->tCK / config.memSpec->dataRate), enableWindowing(config.enableWindowing),
    windowSizeTime(config.windowSize * memSpec.tCK)
{
    if (enableWindowing)
    {
        sensitive << windowEvent;
        slidingAverageBufferDepth = std::vector<sc_time>(scheduler->getBufferDepth().size());
        windowAverageBufferDepth = std::vector<double>(scheduler->getBufferDepth().size());
        windowEvent.notify(windowSizeTime);
        nextWindowEventTime = windowSizeTime;
    }
}

tlm_sync_enum ControllerRecordable::nb_transport_fw(tlm_generic_payload& trans,
                              tlm_phase& phase, sc_time& delay)
{
    tlmRecorder.recordPhase(trans, phase, delay);
    return Controller::nb_transport_fw(trans, phase, delay);
}

tlm_sync_enum ControllerRecordable::nb_transport_bw(tlm_generic_payload&,
                              tlm_phase&, sc_time&)
{
    SC_REPORT_FATAL("Controller", "nb_transport_bw of controller must not be called");
    return TLM_ACCEPTED;
}

void ControllerRecordable::sendToFrontend(tlm_generic_payload& payload, tlm_phase& phase, sc_time& delay)
{
    tlmRecorder.recordPhase(payload, phase, delay);
    tSocket->nb_transport_bw(payload, phase, delay);
}

void ControllerRecordable::controllerMethod()
{
    if (enableWindowing)
    {
        sc_time timeDiff = sc_time_stamp() - lastTimeCalled;
        lastTimeCalled = sc_time_stamp();
        const std::vector<unsigned> &bufferDepth = scheduler->getBufferDepth();

        for (std::size_t index = 0; index < slidingAverageBufferDepth.size(); index++)
            slidingAverageBufferDepth[index] += bufferDepth[index] * timeDiff;

        if (sc_time_stamp() == nextWindowEventTime)
        {
            windowEvent.notify(windowSizeTime);
            nextWindowEventTime += windowSizeTime;

            for (std::size_t index = 0; index < slidingAverageBufferDepth.size(); index++)
            {
                windowAverageBufferDepth[index] = slidingAverageBufferDepth[index] / windowSizeTime;
                slidingAverageBufferDepth[index] = SC_ZERO_TIME;
            }

            tlmRecorder.recordBufferDepth(sc_time_stamp().to_seconds(), windowAverageBufferDepth);

            Controller::controllerMethod();

            uint64_t windowNumberOfBeatsServed = numberOfBeatsServed - lastNumberOfBeatsServed;
            lastNumberOfBeatsServed = numberOfBeatsServed;
            sc_time windowActiveTime = activeTimeMultiplier * static_cast<double>(windowNumberOfBeatsServed);
            double windowAverageBandwidth = windowActiveTime / windowSizeTime;
            tlmRecorder.recordBandwidth(sc_time_stamp().to_seconds(), windowAverageBandwidth);
        }
        else
        {
            Controller::controllerMethod();
        }
    }
    else
    {
        Controller::controllerMethod();
    }
}

} // namespace DRAMSys
