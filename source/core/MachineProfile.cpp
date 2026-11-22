#include "core/MachineProfile.h"

namespace bdmcs48::core {

MachineProfile MachineProfile::genericMcs48() {
    MachineProfile profile;
    profile.profileId = "generic-mcs48";
    profile.displayName = "Generic MCS-48 Architecture Profile";
    profile.cpuVariant = "8049";
    profile.defaultClockHz = 6'000'000;
    profile.romSizeBytes = 2048;

    profile.regions = {
        {"reset_vector", RegionType::ResetVector, 0x0000, 0x0002, "Reset Vector (PC=0x000)"},
        {"ext_interrupt", RegionType::InterruptVector, 0x0003, 0x0006, "External Interrupt Vector (/INT)"},
        {"timer_interrupt", RegionType::TimerVector, 0x0007, 0x000A, "Timer/Counter Interrupt Vector"},
        {"program_space", RegionType::Code, 0x000B, 0x07FF, "General Purpose Program Code Space"}
    };

    return profile;
}

MachineProfile MachineProfile::juno6() {
    MachineProfile profile;
    profile.profileId = "roland-juno6-8049";
    profile.displayName = "Roland Juno-6 Controller (NEC uPD8049C-238)";
    profile.cpuVariant = "8049";
    profile.defaultClockHz = 6'000'000;
    profile.romSizeBytes = 2048;

    profile.regions = {
        {"reset_vector", RegionType::ResetVector, 0x0000, 0x0002, "Reset Vector (CLR A -> JMP 0x00A0)"},
        {"ext_interrupt", RegionType::InterruptVector, 0x0003, 0x0006, "External Interrupt Vector (SEL RB1)"},
        {"timer_interrupt", RegionType::TimerVector, 0x0007, 0x000A, "Timer Interrupt Vector (OUTL P2,A)"},
        {"driver_code", RegionType::Code, 0x000B, 0x00F7, "Init, I/O and Main State Machine"},
        {"bitmask_table", RegionType::DataTable, 0x00F8, 0x00FF, "Autotest Bit Mask Table (01 02 04 08 10 20 40 80)"},
        {"step_table", RegionType::DataTable, 0x0100, 0x0108, "Autotest Step Sequence Table (00 00 08 10 18 20 28 30 38)"},
        {"keyboard_scan", RegionType::Code, 0x0109, 0x01DD, "Keyboard and Control Surface Matrix Scanner"},
        {"padding_1", RegionType::ReservedOrPadding, 0x01DE, 0x01FC, "Alignment Padding"},
        {"jump_trans", RegionType::Code, 0x01FD, 0x01FE, "Transition Branch Instruction"},
        {"padding_2", RegionType::ReservedOrPadding, 0x01FF, 0x01FF, "Alignment Byte"},
        {"arpeggiator", RegionType::Code, 0x0200, 0x02DB, "Arpeggiator and DCO Pitch Update Dispatcher"},
        {"padding_3", RegionType::ReservedOrPadding, 0x02DC, 0x02FF, "Alignment Padding"},
        {"pitch_conv", RegionType::Code, 0x0300, 0x0307, "Pitch Value Converter"},
        {"pitch_const", RegionType::DataTable, 0x0308, 0x0308, "Calibration Constant"},
        {"dispatch_logic", RegionType::Code, 0x0309, 0x0310, "Voice Event Dispatch Logic"},
        {"tuning_data", RegionType::DataTable, 0x0311, 0x0375, "Key Tuning Offsets and Inter-block Tables"},
        {"voice_assign", RegionType::Code, 0x0376, 0x0383, "Polyphonic Voice Allocator Subroutine"},
        {"assign_data", RegionType::DataTable, 0x0384, 0x03DB, "Voice Memory and Latch Buffer Descriptors"},
        {"freq_calc", RegionType::Code, 0x03DC, 0x03E6, "Timer Frequency Scaling Calculator"},
        {"padding_4", RegionType::ReservedOrPadding, 0x03E7, 0x03EC, "Alignment Padding"},
        {"poly_engine", RegionType::Code, 0x03ED, 0x05B4, "6-Voice Polyphony Driver and 8243 Port Expander Bridge"},
        {"env_tables", RegionType::DataTable, 0x05B5, 0x05F8, "VCF/VCA Contour Calibration Tables"},
        {"vcf_vca_ctrl", RegionType::Code, 0x05F9, 0x06D2, "Analog Filter and Amplifier Control Loop"},
        {"padding_5", RegionType::ReservedOrPadding, 0x06D3, 0x06FF, "Alignment Padding"},
        {"mod_ctrl", RegionType::Code, 0x0700, 0x0791, "LFO/PWM Modulation and Final Signal Output"},
        {"rom_padding", RegionType::ReservedOrPadding, 0x0792, 0x07FF, "End of ROM Unused Reserved Space"}
    };

    return profile;
}

} // namespace bdmcs48::core
