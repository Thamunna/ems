# EMS_FW_v1.0 — Energy Management System Firmware

Industrial-grade firmware for an Energy Management System (EMS) built on the **STM32F411CEU6** microcontroller, interfacing with the **LTC6811-2** battery monitor IC via the **Analog Devices DC2260A** evaluation board.

---

## Overview

This firmware monitors up to **12 lithium-ion cells**, performs temperature measurement via NTC thermistors, detects fault conditions (over-voltage, under-voltage, open wire), implements runtime voltage calibration, and executes passive cell balancing — all through a clean, modular architecture designed for production-quality embedded systems.

---

## Hardware

| Component | Description |
|-----------|-------------|
| **MCU** | STM32F411CEU6 (ARM Cortex-M4, 100 MHz, 512 KB Flash) |
| **Battery Monitor IC** | LTC6811-2 (Analog Devices) |
| **Evaluation Board** | DC2260A |
| **Communication** | SPI (MCU ↔ LTC6811), USB CDC (debug output), USART1 (Modbus RTU) |
| **IDE** | STM32CubeIDE |

### Pin Configuration

| Function | Pin | Port |
|----------|-----|------|
| SPI1_SCK | PA5 | GPIOA |
| SPI1_MISO | PA6 | GPIOA |
| SPI1_MOSI | PA7 | GPIOA |
| SPI1_CS (Manual) | PA4 | GPIOA |
| USB_DM | PA11 | GPIOA |
| USB_DP | PA12 | GPIOA |
| USART1_TX | PA9 | GPIOA |
| USART1_RX | PA10 | GPIOA |
| LED | PC13 | GPIOC |
| User Button | PA0 | GPIOA |

---

## Features

- ✅ **LTC6811 Communication** — Full SPI driver with PEC15 integrity checking
- ✅ **Cell Voltage Monitoring** — Read all 12 cell voltages via ADC conversion
- ✅ **Status Monitoring** — Internal temperature, analog/digital supply, reference voltage
- ✅ **GPIO Temperature** — NTC thermistor reading via GPIO1 auxiliary ADC
- ✅ **Over-Voltage Detection** — Per-cell OV fault flagging (threshold: 4.20V)
- ✅ **Under-Voltage Detection** — Per-cell UV fault flagging (threshold: 0.50V)
- ✅ **Open Wire Detection** — Pull-up/pull-down ADOW comparison test
- ✅ **Passive Cell Balancing** — Automatic DCC discharge with 50mV imbalance threshold
- ✅ **Dynamic Calibration** — Runtime single-point and two-point voltage calibration
- ✅ **Modbus RTU Integration** — Maps 86 input registers (voltages, temperatures, faults) served over USART1 DMA
- ✅ **USB CDC Debug Output** — Real-time diagnostic printing via USB virtual COM port
- ✅ **Configurable Thresholds** — All parameters defined as named macros with calculation formulas

---

## Project Structure

```
EMS_FW_v1.0/
│
├── Core/
│   ├── Inc/
│   │   ├── main.h                  # CubeMX generated — pin defines
│   │   ├── ems_data.h              # Centralized EMS data structure + all config macros
│   │   ├── stm32f4xx_hal_conf.h    # HAL configuration
│   │   ├── stm32f4xx_it.h          # Interrupt handler declarations
│   │   ├── modbusSlave.h           # Modbus RTU slave implementation
│   │   └── modbus_crc.h            # Modbus CRC16 lookup table
│   ├── Src/
│   │   ├── main.c                  # Application entry point + main loop + _write() USB redirect
│   │   ├── stm32f4xx_hal_msp.c     # CubeMX generated — peripheral MSP
│   │   ├── stm32f4xx_it.c          # CubeMX generated — interrupt handlers
│   │   ├── stm32f4xx_hal_timebase_tim.c
│   │   ├── modbusSlave.c           # Modbus RTU slave implementation
│   │   ├── modbus_crc.c            # Modbus CRC16 lookup table
│   │   ├── syscalls.c              # Newlib system calls
│   │   ├── sysmem.c                # Newlib memory management
│   │   └── system_stm32f4xx.c      # CMSIS system initialization
│   └── Startup/
│       └── startup_stm32f411ceux.s # Vector table + reset handler
│
├── Drivers/
│   ├── LTC6811/
│   │   ├── ltc6811.h               # LTC6811 driver API
│   │   └── ltc6811.c               # LTC6811 SPI communication driver
│   ├── STM32F4xx_HAL_Driver/       # ST HAL library
│   └── CMSIS/                      # ARM CMSIS headers
│
├── EMS/
│   ├── ems_monitor.h               # Measurement orchestration API
│   ├── ems_monitor.c               # Cell/status/GPIO measurement sequence
│   ├── ems_temperature.h           # Temperature calculation API
│   ├── ems_temperature.c           # NTC Beta equation implementation
│   ├── ems_fault.h                 # Fault detection API
│   ├── ems_fault.c                 # OV/UV/open wire detection
│   ├── ems_balancing.h             # Cell balancing API
│   ├── ems_balancing.c             # Passive balancing algorithm
│   ├── ems_calibration.h           # Dynamic calibration API
│   ├── ems_calibration.c           # Single-point & two-point calibration engine
│   ├── ems_modbus.h                # Modbus RTU register mapping API
│   ├── ems_modbus.c                # Modbus RTU register mapping implementation
│   ├── crc15.h                     # PEC15 CRC API
│   ├── crc15.c                     # PEC15 CRC implementation (LTC6811 polynomial 0x4599)
│   ├── usb_debug.h                 # USB CDC debug print API
│   └── usb_debug.c                 # Formatted diagnostic output via USB
│
├── USB_DEVICE/                     # USB CDC device (CubeMX generated)
├── Middlewares/                    # ST USB middleware (CubeMX generated)
│
├── EMS_FW_v1.0.ioc                # STM32CubeMX project file
├── STM32F411CEUX_FLASH.ld          # Linker script (Flash)
├── STM32F411CEUX_RAM.ld            # Linker script (RAM)
├── .cproject                       # Eclipse/STM32CubeIDE project config
├── .project                        # Eclipse project descriptor
├── IMPLEMENTATION_GUIDE.md         # Detailed technical implementation guide
└── README.md                       # This file
```

---

## Architecture

### Data Flow

```
┌─────────────────────────────────────────────────────┐
│                    main.c (Loop)                    │
│                                                     │
│  EMS_UpdateMeasurements()  ──► EMS_Data_t ◄──┐      │
│  EMS_UpdateTemperature()   ──►              │      │
│  EMS_CheckFaults()         ──►              │      │
│  EMS_OpenWireTest()        ──►              │      │
│  EMS_CellBalancing()       ──►              │      │
│                                              │      │
│  USB_PrintCellVoltages()  ◄─────────────────┘      │
│  USB_PrintStatusA/B()     ◄──────────────────      │
│  USB_PrintGPIO1Temp()     ◄──────────────────      │
└─────────────────────────────────────────────────────┘
```

### Calibration Data Flow

```
                  ┌──────────────────┐
                  │  ems_calibration │
                  │  SinglePoint()   │
                  │  TwoPoint()      │
                  └────────┬─────────┘
                           │ writes
                           ▼
          EMS_Data.CellGain[i]  &  EMS_Data.CellOffset[i]
                           │
                           │ applied during
                           ▼
              LTC6811_DecodeCellGroup()
              CellVoltage = (Raw * 0.0001 * Gain) + Offset
```

### Module Responsibilities

| Module | Responsibility | Does NOT |
|--------|---------------|----------|
| **ltc6811.c** | SPI communication, command generation, PEC, register R/W, ADC commands, apply calibration coefficients during decode | Calculate temperature, detect faults, control balancing |
| **ems_monitor.c** | Orchestrate measurement sequence, call driver functions with correct delays | Perform calculations, detect faults |
| **ems_temperature.c** | Convert GPIO voltage → NTC resistance → temperature (°C) via Beta equation | Communicate over SPI |
| **ems_fault.c** | OV/UV detection, open wire test via ADOW pull-up/pull-down comparison | Redesign detection algorithms |
| **ems_balancing.c** | Select cells for discharge based on voltage imbalance, write DCC bits | Redesign balancing algorithm |
| **ems_calibration.c** | Compute and store Gain/Offset calibration coefficients at runtime | Persist calibration to flash |
| **crc15.c** | PEC15 CRC calculation (polynomial 0x4599, initial remainder 16) | Anything else |
| **usb_debug.c** | Format and print diagnostic data via USB CDC | Contain application logic |

### Centralized Data Structure

All modules read from and write to a single global `EMS_Data_t` structure defined in `ems_data.h`:

```c
typedef struct
{
    /* Cell Voltage Measurements */
    float    CellVoltage[12];         /* Calibrated cell voltages (V)       */
    uint16_t RawCell[12];             /* Raw ADC values (uncalibrated)      */

    /* Calibration Coefficients (applied during ADC decode) */
    float    CellGain[12];            /* Per-cell gain  (default: 1.0)      */
    float    CellOffset[12];          /* Per-cell offset (default: 0.0V)    */

    /* GPIO / Auxiliary Measurements */
    uint16_t RawGPIO[5];              /* Raw GPIO ADC values                */
    float    GPIO1Voltage;            /* GPIO1 voltage (V)                  */

    /* Status Register Data */
    float    StackVoltage;            /* Sum of cells (V)                   */
    float    InternalTemp;            /* IC die temperature (°C)            */
    float    AnalogSupply;            /* VREG analog supply voltage (V)     */
    float    DigitalSupply;           /* VD digital supply voltage (V)      */
    float    ReferenceVoltage2;       /* REF2 voltage (V)                   */

    /* Temperature */
    float    Temperature;             /* GPIO1 NTC temperature (°C)         */
    float    NTCResistance;           /* Calculated NTC resistance (Ω)      */

    /* Thresholds (decoded from config register) */
    float    UVThreshold;             /* Under-voltage threshold (V)        */
    float    OVThreshold;             /* Over-voltage threshold (V)         */

    /* Fault Flags */
    uint8_t  OVFlag[12];              /* Over-voltage flags                 */
    uint8_t  UVFlag[12];              /* Under-voltage flags                */
    uint8_t  OpenWireFlag[12];        /* Open wire flags                    */

    /* Balancing */
    uint8_t  CellBalanceStatus[12];   /* Per-cell balance status (0/1)      */
    uint16_t DCCBits;                 /* Current DCC bitmask (12-bit)       */

    /* Communication Health */
    uint32_t PECErrorCount;           /* PEC error counter                  */
    uint8_t  CommunicationStatus;     /* 0=OK, non-zero=error               */
    uint32_t SystemFaultFlags;        /* Bitfield for system faults         */

} EMS_Data_t;
```

---

## Configuration

All configurable parameters are defined in [`ems_data.h`](Core/Inc/ems_data.h) with calculation formulas:

### Voltage Thresholds

| Parameter | Default | Calculation / Notes |
|-----------|:-------:|---------------------|
| `EMS_TOTAL_CELLS` | 12 | Number of cell channels monitored |
| `CELL_OV_THRESHOLD` | 4.20 V | Over-voltage detection limit for Li-ion |
| `CELL_UV_THRESHOLD` | 0.50 V | Under-voltage threshold — cells below this are treated as "unused" |
| `CELL_MIN_VALID_VOLTAGE` | 1.00 V | Minimum voltage to consider a cell connected/valid |
| `CELL_BALANCE_THRESHOLD` | 50 mV | Imbalance delta that triggers passive discharge |
| `IC_TEMP_MAX_BALANCING` | 60 °C | IC die temperature safety limit for balancing |
| `OPEN_WIRE_THRESHOLD` | 0.40 V | Pull-up vs pull-down voltage difference for open wire detection |

### LTC6811 Register Value Calculations

| Parameter | Default | Formula |
|-----------|:-------:|---------|
| `LTC6811_VUV_VALUE` | 437 | `UV_Voltage / (16 × 100µV) = 0.7V / 0.0016 = 437.5 ≈ 437` |
| `LTC6811_VOV_VALUE` | 2625 | `OV_Voltage / (16 × 100µV) = 4.2V / 0.0016 = 2625` |

**To set custom thresholds:**  `Register_Value = Desired_Voltage / 0.0016`

Example: For 3.0V UV threshold → `3.0 / 0.0016 = 1875`

### ADC Conversion Factors

| Parameter | Value | Derivation |
|-----------|:-----:|------------|
| `LTC6811_CELL_LSB` | 0.0001 V | Cell ADC has 100µV resolution per LSB |
| `LTC6811_GPIO_LSB` | 0.0001 V | Auxiliary ADC has 100µV resolution per LSB |
| `LTC6811_SOC_LSB` | 0.002 V | Sum of Cells register has 2mV resolution per LSB |
| `LTC6811_ITEMP_SCALE` | 0.013333 | `100µV_per_LSB / 7.5mV_per_°C = 0.013333 °C/LSB` |
| `LTC6811_ITEMP_OFFSET` | 273.0 | Subtract to convert from Kelvin to Celsius |
| `LTC6811_SUPPLY_LSB` | 0.0001 V | Supply voltage ADC has 100µV resolution per LSB |
| `LTC6811_UVOV_LSB` | 0.0016 V | UV/OV threshold: `16 × 100µV = 1.6mV per LSB` |

### NTC Parameters

| Parameter | Default | Notes |
|-----------|:-------:|-------|
| `NTC_BETA` | 3950 | Beta coefficient for the specific NTC thermistor |
| `NTC_R_NOMINAL` | 10 kΩ | Nominal resistance at T₀ (25°C) |
| `NTC_T_NOMINAL` | 25 °C | Reference temperature for nominal resistance |
| `NTC_R_PULLUP_GPIO` | 1 MΩ | External pull-up resistor on GPIO1 line |

---

## Dynamic Calibration

The firmware supports runtime voltage calibration to correct for ADC offset and gain errors. Calibration coefficients are stored in `EMS_Data.CellGain[]` and `EMS_Data.CellOffset[]` and are applied automatically during every ADC decode cycle.

### Calibration Formula

Every cell voltage is calculated as:

```
CellVoltage[i] = (RawADC × 0.0001 × CellGain[i]) + CellOffset[i]
```

**Default values:** `Gain = 1.0`, `Offset = 0.0` (no correction applied)

### Single-Point Calibration

Corrects a constant offset error. Best when the ADC gain is accurate but has a DC offset.

```c
/* Example: Cell 3 reads 2.10V but the actual voltage is 2.60V */
EMS_CalibrateCellSinglePoint(2, 2.60f);  /* cell_index=2 (0-based), actual=2.60V */
```

After calibration: `Offset = 2.60 - 2.10 = +0.50V`, `Gain = 1.0`

### Two-Point Calibration

Corrects both gain and offset errors. Requires two known reference voltages.

```c
/* Example: Cell 5 reads 1.48V at actual 1.50V and 3.92V at actual 4.00V */
EMS_CalibrateCellTwoPoint(4, 1.50f, 1.48f, 4.00f, 3.92f);
/* Gain  = (4.00 - 1.50) / (3.92 - 1.48) = 1.0246 */
/* Offset = 4.00 - (1.0246 × 3.92) = -0.0164V      */
```

### Batch Calibration

Calibrate all 12 cells at once:

```c
/* Single-point batch: array of 12 actual voltages */
float actuals[12] = {2.60f, 2.60f, 2.60f, 0, 0, 0, 0, 0, 0, 0, 0, 0};
EMS_CalibrateAllCellsSinglePoint(actuals);

/* Two-point batch: arrays of 12 low and high reference pairs */
EMS_CalibrateAllCellsTwoPoint(low_actuals, low_raws, high_actuals, high_raws);
```

### When to Calibrate

| Scenario | Recommended Method |
|----------|--------------------|
| Constant offset on all cells | Single-point batch |
| One cell reads differently | Single-point for that cell |
| Non-linear gain error | Two-point per cell |
| After changing LTC6811 supply voltage | Re-calibrate all cells |
| Reference voltage (VREF2) ≠ 3.000V | Investigate V+ supply headroom first |

> **Important:** The LTC6811 requires ≥ 11V at the V+ pin for the internal LDO to produce a stable 5V VREG and 3V VREF2. Operating below this voltage causes reference sagging and measurement offsets that should be fixed at the hardware level, not through calibration.

---

## Siemens TIA Portal Modbus Integration

To integrate the EMS firmware's Modbus RTU interface with a Siemens S7-1200 or S7-1500 PLC in TIA Portal, follow the structured programming pattern detailed below.

### 1. Register Mapping & Scaling Reference
Because Modbus registers are 0-indexed in the firmware code but typical Siemens Modbus client arrays (`Array[1..N] of Word`) are 1-indexed, register `0` maps to array index `1`. 

The following table defines how the 86 Modbus registers map to the raw input array and how they must be scaled in the PLC:

| Modbus Reg | Raw Array Index | Parameter | C scaling | PLC Formula |
| :--- | :--- | :--- | :---: | :--- |
| **0 - 11** | `1 - 12` | Cell Voltages 1 - 12 | ×10000 | `Value / 10000.0` (V) |
| **12 - 23** | `13 - 24` | Raw ADC values 1 - 12 | None | Direct copy to `Int` |
| **24** | `25` | Stack Voltage | ×1000 | `Value / 1000.0` (V) |
| **25** | `26` | Internal Die Temp | ×100 | `Value / 100.0` (°C) |
| **26** | `27` | Analog Supply (VREG) | ×10000 | `Value / 10000.0` (V) |
| **27** | `28` | Digital Supply (VD) | ×10000 | `Value / 10000.0` (V) |
| **28** | `29` | Reference Voltage 2 | ×10000 | `Value / 10000.0` (V) |
| **29** | `30` | GPIO1 Voltage | ×10000 | `Value / 10000.0` (V) |
| **30** | `31` | NTC Temperature | ×100 | `Value / 100.0` (°C) |
| **31** | `32` | NTC Resistance | ÷100 | `Value * 100.0` (Ω) |
| **32** | `33` | UV Threshold | ×10000 | `Value / 10000.0` (V) |
| **33** | `34` | OV Threshold | ×10000 | `Value / 10000.0` (V) |
| **34 - 45** | `35 - 46` | OV Flags 1 - 12 | 0 or 1 | Boolean: `Value <> 0` |
| **46 - 57** | `47 - 58` | UV Flags 1 - 12 | 0 or 1 | Boolean: `Value <> 0` |
| **58 - 69** | `59 - 70` | Open Wire Flags 1 - 12 | 0 or 1 | Boolean: `Value <> 0` |
| **70 - 81** | `71 - 82` | Cell Balance Status 1 - 12 | 0 or 1 | Boolean: `Value <> 0` |
| **82** | `83` | DCC Bitmask | None | Direct copy to `Word` |
| **83** | `84` | PEC Error Count | None | Direct copy to `DWord` |
| **84** | `85` | Communication Status | None | Direct copy to `Int` |
| **85** | `86` | System Fault Flags | None | Direct copy to `Word` |

---

### 2. User-Defined PLC Data Type (UDT)
Create a new PLC data type named `typeEMS_ActualData` in TIA Portal:
* **CellVoltage**: `Array[1..12] of Real`
* **RawCell**: `Array[1..12] of Int`
* **StackVoltage**: `Real`
* **InternalTemp**: `Real`
* **AnalogSupply**: `Real`
* **DigitalSupply**: `Real`
* **ReferenceVoltage2**: `Real`
* **GPIO1Voltage**: `Real`
* **Temperature**: `Real`
* **NTCResistance**: `Real`
* **UVThreshold**: `Real`
* **OVThreshold**: `Real`
* **OVFlag**: `Array[1..12] of Bool`
* **UVFlag**: `Array[1..12] of Bool`
* **OpenWireFlag**: `Array[1..12] of Bool`
* **CellBalanceStatus**: `Array[1..12] of Bool`
* **DCCBits**: `Word`
* **PECErrorCount**: `DWord`
* **CommunicationStatus**: `Int`
* **SystemFaultFlags**: `Word`

---

### 3. SCL Mapping Block (FC / FB)
Create an SCL Function block (`FC_REG_MAP`). 

**Block Interface:**
* **Input:** `raw_array` : `Array[1..86] of Word`
* **Output:** `actual_data` : `typeEMS_ActualData`
* **Temp:** `i` : `Int`

**SCL Implementation:**
```scl
// Loop counter initialization
#i := 1;

// ==========================================
// 1. Cell Voltages (Regs 0-11 -> Array index 1 to 12)
// Scale: x10000 -> Divide by 10000.0 for Volts (V)
// ==========================================
FOR #i := 1 TO 12 DO
    #actual_data.CellVoltage[#i] := DINT_TO_REAL(WORD_TO_DINT(#raw_array[#i])) / 10000.0;
END_FOR;

// ==========================================
// 2. Raw ADC Readings (Regs 12-23 -> Array index 13 to 24)
// ==========================================
FOR #i := 1 TO 12 DO
    #actual_data.RawCell[#i] := WORD_TO_INT(#raw_array[12 + #i]);
END_FOR;

// ==========================================
// 3. Status Registers & Supplies
// ==========================================
// Stack Voltage (Index 25): Scale x1000 -> Divide by 1000.0 (V)
#actual_data.StackVoltage := DINT_TO_REAL(WORD_TO_DINT(#raw_array[25])) / 1000.0;

// Internal MCU Temp (Index 26): Scale x100 -> Divide by 100.0 (°C)
#actual_data.InternalTemp := DINT_TO_REAL(WORD_TO_DINT(#raw_array[26])) / 100.0;

// Analog Supply (VREG) (Index 27): Scale x10000 -> Divide by 10000.0 (V)
#actual_data.AnalogSupply := DINT_TO_REAL(WORD_TO_DINT(#raw_array[27])) / 10000.0;

// Digital Supply (VD) (Index 28): Scale x10000 -> Divide by 10000.0 (V)
#actual_data.DigitalSupply := DINT_TO_REAL(WORD_TO_DINT(#raw_array[28])) / 10000.0;

// Ref Voltage 2 (Index 29): Scale x10000 -> Divide by 10000.0 (V)
#actual_data.ReferenceVoltage2 := DINT_TO_REAL(WORD_TO_DINT(#raw_array[29])) / 10000.0;

// GPIO1 Voltage (Index 30): Scale x10000 -> Divide by 10000.0 (V)
#actual_data.GPIO1Voltage := DINT_TO_REAL(WORD_TO_DINT(#raw_array[30])) / 10000.0;

// NTC Temperature (Index 31): Scale x100 -> Divide by 100.0 (°C)
#actual_data.Temperature := DINT_TO_REAL(WORD_TO_DINT(#raw_array[31])) / 100.0;

// NTC Resistance (Index 32): Scale /100 -> Multiply by 100.0 (Ω)
#actual_data.NTCResistance := DINT_TO_REAL(WORD_TO_DINT(#raw_array[32])) * 100.0;

// ==========================================
// 4. Thresholds
// ==========================================
#actual_data.UVThreshold := DINT_TO_REAL(WORD_TO_DINT(#raw_array[33])) / 10000.0;
#actual_data.OVThreshold := DINT_TO_REAL(WORD_TO_DINT(#raw_array[34])) / 10000.0;

// ==========================================
// 5. Fault & Status Flags (Regs 34-81 -> Array index 35 to 82)
// Convert 0/1 to Booleans (using loop 1 to 12)
// ==========================================
FOR #i := 1 TO 12 DO
    // Over-voltage flags (raw_array[35] to [46])
    #actual_data.OVFlag[#i] := (#raw_array[34 + #i] <> 16#0);
    
    // Under-voltage flags (raw_array[47] to [58])
    #actual_data.UVFlag[#i] := (#raw_array[46 + #i] <> 16#0);
    
    // Open Wire flags (raw_array[59] to [70])
    #actual_data.OpenWireFlag[#i] := (#raw_array[58 + #i] <> 16#0);
    
    // Cell Balancing flags (raw_array[71] to [82])
    #actual_data.CellBalanceStatus[#i] := (#raw_array[70 + #i] <> 16#0);
END_FOR;

// ==========================================
// 6. DCC Bits & Health Status (Regs 82-85 -> Index 83 to 86)
// ==========================================
#actual_data.DCCBits := #raw_array[83];
#actual_data.PECErrorCount := WORD_TO_DWORD(#raw_array[84]);
#actual_data.CommunicationStatus := WORD_TO_INT(#raw_array[85]);
#actual_data.SystemFaultFlags := #raw_array[86];
```

### 4. Integration Steps
1. Call the `MB_CLIENT` block in TIA Portal to read the Modbus input registers (Function Code 4) and store the results in your raw DB array (`"DB_Modbus_Raw".RawData` which is defined as `Array[1..86] of Word`).
2. Instantiate a tag `"DB_EMS_Data".Actual` of type `typeEMS_ActualData`.
3. In `OB1` (or another cyclical block), call `FC_REG_MAP` and wire:
   * `raw_array` := `"DB_Modbus_Raw".RawData`
   * `actual_data` := `"DB_EMS_Data".Actual`

---

## Building

### Prerequisites

- **STM32CubeIDE** (v1.12+ recommended)
- **STM32F4 HAL Library** (included in project)
- ARM GCC Toolchain (bundled with STM32CubeIDE)

### Build Steps

1. Open STM32CubeIDE
2. **File → Import → Existing Projects into Workspace**
3. Select the `EMS_FW_v1.0` directory
4. Right-click project → **Refresh** (to discover new folders)
5. **Project → Build All** (`Ctrl+B`)

> **Note:** If the IDE shows include errors after import, right-click → **Index → Rebuild** to force re-indexing.

### Build Configurations

| Config | Optimization | Debug | Use Case |
|--------|:------------:|:-----:|----------|
| **Debug** | -O0 | -g3 | Development, debugging with ST-Link |
| **Release** | -Os | -g0 | Production deployment |

---

## Flashing

1. Connect ST-Link V2 debugger to the STM32F411 SWD header
2. **Run → Debug As → STM32 C/C++ Application**
3. Or use **Run → Run As** for direct flash without debug session

---

## Debug Output

Connect via **USB CDC** (virtual COM port). The firmware prints diagnostics via `printf()` which is redirected to USB CDC through the `_write()` system call override in `main.c`.

```
========== CELL VOLTAGES ==========
Cell 01 : 3.3245 V
Cell 02 : 3.3198 V
Cell 03 : 3.3301 V
...
Cell 12 : 0.0000 V
-----------------------------------
TOTAL STACK VOLTAGE : 19.9478 V
===================================

========== STATUS A ==========
Sum Of Cells    : 19.9480 V
Internal Temp   : 26.4521
Analog Supply   : 5.0012 V
================================

========== STATUS B ==========
Digital Supply  : 3.2987 V
REF2 Voltage    : 2.9995 V
================================

GPIO1 : 0.8234 V  Temp : 24.67 C

========== OPEN WIRE TEST ==========
Cell 01 : PU=3.3245V  PD=3.3240V  DIFF=0.0005V
...
====================================
```

---

## Future Roadmap

The architecture is designed for seamless expansion:

| Feature | Integration Point | Changes Required |
|---------|-------------------|-----------------|
| **Modbus RTU** | ✅ Completed (Maps to Input Registers via DMA) | None |
| **CAN Bus** | Read `EMS_Data_t` → CAN TX frames | New CAN module only |
| **Data Logger** | Read `EMS_Data_t` → SD card / Flash | New logger module only |
| **HMI Display** | Read `EMS_Data_t` → display protocol | New display module only |
| **Cloud / IoT** | Read `EMS_Data_t` → MQTT / HTTP | New connectivity module only |
| **SOC Estimation** | Read `CellVoltage[]` → SOC algorithm | New SOC module only |
| **Calibration Persistence** | Store `CellGain[]` & `CellOffset[]` → Flash/EEPROM | New storage module only |

> **Key principle:** EMS modules only *produce* data. Communication modules only *consume* data. No existing module needs modification when adding a new interface.

---

## License

Copyright (c) 2026. All rights reserved.

STM32 HAL drivers are licensed under terms found in the respective LICENSE files provided by STMicroelectronics.
