#include "Debugger.h"

#include "EmCPU68K.h"
#include "EmMemory.h"
#include "EmSession.h"
#include "UAE.h"

Debugger gDebugger;

Debugger::BreakState Debugger::GetBreakState() const { return breakState; }

bool Debugger::IsStopped() const { return breakState != BreakState::none; }

void Debugger::Reset() {
    breakState = BreakState::none;
    stepping = false;
    lastBreakAtPc = 0xffffffff;
}

void Debugger::NotificyPc(emuptr pc) {
    EmAssert(gSession);
    if (gSession->IsNested() || breakState != BreakState::none) return;

    if (stepping && pc != lastBreakAtPc) breakState = BreakState::step;

    if (breakState != BreakState::none) lastBreakAtPc = regs.pc;
}

void Debugger::NotifyMemoryRead(emuptr address) {}

void Debugger::NotifyMemoryWrite(emuptr address) {}

void Debugger::SetBreakpoint(emuptr pc) {}

void Debugger::ClearBreakpoint(emuptr pc) {}

void Debugger::SetWatchpoint(emuptr address) {}

void Debugger::ClearWatchpoint(emuptr address) {}

void Debugger::Interrupt() {
    breakState = BreakState::externalInterrupt;
    lastBreakAtPc = regs.pc;
}

void Debugger::Continue() {
    breakState = BreakState::none;
    stepping = false;
}

void Debugger::Step() {
    stepping = true;
    breakState = BreakState::none;
}

uint8 Debugger::MemoryRead(emuptr addr) {
    CEnableFullAccess munge;

    return EmMemGet8(addr);
}

const array<uint32, Debugger::REGISTER_COUNT>& Debugger::ReadRegisters() {
    // straight from the horse's mouth:
    //
    // static const char * const m68k_register_names[] = {
    //     "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
    //     "a0", "a1", "a2", "a3", "a4", "a5", "fp", "sp",
    //     "ps", "pc"

    EmAssert(gCPU68K);
    gCPU68K->UpdateSRFromRegisters();

    for (int i = 0; i < 8; i++) {
        registers[i] = m68k_dreg(regs, i);
        registers[8 + i] = m68k_areg(regs, i);
    }

    registers[16] = regs.sr;
    registers[17] = regs.pc;

    return registers;
}
