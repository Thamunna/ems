# EMS Firmware v1.0 — Detailed Implementation Guide

This document provides an in-depth technical walkthrough of every firmware module, including register-level details, mathematical derivations, data flow, and practical usage guidance.

---

## Table of Contents

1. [System Overview](#1-system-overview)
2. [Hardware Interface](#2-hardware-interface)
3. [Module Details](#3-module-details)
   - 3.1 [ems_data.h — Centralized Data Structure](#31-ems_datah--centralized-data-structure)
   - 3.2 [crc15.c — PEC15 CRC Engine](#32-crc15c--pec15-crc-engine)
   - 3.3 [ltc6811.c — LTC6811 SPI Driver](#33-ltc6811c--ltc6811-spi-driver)
   - 3.4 [ems_monitor.c — Measurement Orchestrator](#34-ems_monitorc--measurement-orchestrator)
   - 3.5 [ems_temperature.c — NTC Temperature Calculator](#35-ems_temperaturec--ntc-temperature-calculator)
   - 3.6 [ems_fault.c — Fault Detection Engine](#36-ems_faultc--fault-detection-engine)
   - 3.7 [ems_balancing.c — Passive Cell Balancing](#37-ems_balancingc--passive-cell-balancing)
   - 3.8 [ems_calibration.c — Dynamic Calibration Engine](#38-ems_calibrationc--dynamic-calibration-engine)
   - 3.9 [usb_debug.c — USB CDC Debug Output](#39-usb_debugc--usb-cdc-debug-output)
   - 3.10 [ems_modbus.c — Modbus RTU Register Mapping](#310-ems_modbusc--modbus-rtu-register-mapping)
4. [Main Application Loop](#4-main-application-loop)
5. [Configuration Reference](#5-configuration-reference)
6. [Calibration Procedures](#6-calibration-procedures)
7. [Troubleshooting](#7-troubleshooting)
8. [Build System Notes](#8-build-system-notes)

---

## 1. System Overview

### Architecture Diagram

```
  ┌──────────────────────────────────────────────────────────────────────┐
  │                         STM32F411CEU6                               │
  │                                                                      │
  │  ┌──────────────┐    ┌──────────────────────────────────────────┐   │
  │  │   main.c     │    │              EMS Modules                 │   │
  │  │  (main loop) │───▶│  ems_monitor    → Read measurements     │   │
  │  │              │    │  ems_temperature → Calc NTC temp         │   │
  │  │              │    │  ems_fault       → Check OV/UV/OW        │   │
  │  │              │    │  ems_balancing   → Passive balance       │   │
  │  │              │    │  ems_calibration → Adjust coefficients   │   │
  │  │              │    └──────────┬───────────────────────────────┘   │
  │  │              │               │                                    │
  │  │              │               ▼                                    │
  │  │              │    ┌──────────────────────┐                       │
  │  │              │    │    EMS_Data_t         │ ◄── Single Source    │
  │  │              │    │  (Global Structure)   │     of Truth         │
  │  │              │    └──────────┬────────────┘                      │
  │  │              │               │                                    │
  │  │              │    ┌──────────┴───────────┐                       │
  │  │              │    │    usb_debug.c        │                       │
  │  │              │    │  (Format & Print)     │                       │
  │  │              │    └──────────┬────────────┘                      │
  │  └──────────────┘               │                                    │
  │                                 ▼                                    │
  │                     ┌───────────────────────┐                       │
  │                     │ USB CDC → printf()    │──── to PC terminal    │
  │                     └───────────────────────┘                       │
  │                                                                      │
  │                     ┌───────────────────────┐                       │
  │                     │   SPI1 (PA4-PA7)      │──── to LTC6811       │
  │                     └───────────────────────┘                       │
  └──────────────────────────────────────────────────────────────────────┘
```

### Design Principles

1. **Single Source of Truth**: All measured data, fault flags, calibration coefficients, and system status live in one global `EMS_Data_t` structure.
2. **Separation of Concerns**: Each module has a single responsibility. The LTC6811 driver only handles SPI communication. EMS modules only process data. Debug modules only format output.
3. **No CubeMX Regeneration**: The supplied `main.c` is the functional baseline. Do NOT regenerate code from the `.ioc` file.
4. **Calibration-Aware Decoding**: Raw ADC values pass through `(Raw × Gain) + Offset` during decode, so every subsequent consumer sees calibrated values.

---

## 2. Hardware Interface

### SPI Communication with LTC6811-2

The LTC6811-2 communicates via SPI in a half-duplex fashion:

```
MCU (SPI1 Master)              LTC6811-2
────────────────────────────────────────
PA5 (SCK)    ───────────────►  SCLK
PA7 (MOSI)   ───────────────►  SDI
PA6 (MISO)   ◄───────────────  SDO
PA4 (CS)     ───────────────►  CSB
```

**SPI Configuration:**
- Mode: Full Duplex Master
- Data Size: 8-bit
- Clock Polarity: Low (CPOL = 0)
- Clock Phase: 1 Edge (CPHA = 0)
- Baud Rate: System dependent (< 1 MHz recommended)
- NSS: Software-managed (manual GPIO toggle)
- Timeout: 100ms (`LTC6811_SPI_TIMEOUT`)

### Command Frame Format

Every SPI transaction follows this frame structure:

```
┌──────────┬──────────┬──────────┬──────────┐
│  CMD[0]  │  CMD[1]  │ PEC_MSB  │ PEC_LSB  │
│  (1 byte)│  (1 byte)│  (1 byte)│  (1 byte)│
└──────────┴──────────┴──────────┴──────────┘
     ▲           ▲           ▲
     │           │           └── 15-bit CRC (PEC15) of CMD[0:1]
     │           └── Second command byte
     └── First command byte
```

### Register Data Frame Format

Register reads return 8 bytes:

```
┌─────┬─────┬─────┬─────┬─────┬─────┬──────────┬──────────┐
│ D0  │ D1  │ D2  │ D3  │ D4  │ D5  │ PEC_MSB  │ PEC_LSB  │
└─────┴─────┴─────┴─────┴─────┴─────┴──────────┴──────────┘
  ▲                                        ▲
  │ 6 bytes of register data               │ PEC15 of D0-D5
```

### Wakeup Sequence

The LTC6811 enters sleep mode after ~2 seconds of CS high. Before any communication:

```
CS LOW ──── 300µs ──── CS HIGH ──── 10µs ──── proceed with command
           (tWAKE)                 (tREADY)
```

This is implemented in `LTC6811_Wakeup()` using DWT cycle counter delays.

### USB CDC Debug Interface

`printf()` output is redirected to USB CDC via the `_write()` system call override in `main.c`:

```c
int _write(int file, char *ptr, int len)
{
    CDC_Transmit_FS((uint8_t *)ptr, len);
    HAL_Delay(1);  /* Allow USB transfer to complete */
    return len;
}
```

This enables standard `printf()` calls throughout the firmware to output to a PC terminal via USB virtual COM port.

---

## 3. Module Details

### 3.1 ems_data.h — Centralized Data Structure

**Location:** `Core/Inc/ems_data.h`

This header defines the `EMS_Data_t` structure and all system configuration macros. It is the **only** configuration file you need to modify for threshold adjustments.

#### Macro Calculation Reference

**UV/OV Register Values:**

The LTC6811 stores UV and OV comparison values in its configuration register. The register value is calculated from the desired voltage threshold:

```
Register_Value = Desired_Threshold_Voltage / (16 × 100µV)
               = Desired_Threshold_Voltage / 0.0016

Examples:
  0.7V UV  → 0.7   / 0.0016 = 437.5  → use 437
  4.2V OV  → 4.2   / 0.0016 = 2625.0 → use 2625
  2.5V UV  → 2.5   / 0.0016 = 1562.5 → use 1562
  3.0V OV  → 3.0   / 0.0016 = 1875.0 → use 1875
  3.6V OV  → 3.6   / 0.0016 = 2250.0 → use 2250
```

**Internal Temperature Conversion:**

The LTC6811 internal temperature sensor has a sensitivity of 7.5mV/°C and reports in Kelvin:

```
ADC LSB = 100µV
Sensitivity = 7.5mV/°C = 0.0075 V/°C

Scale factor = ADC_LSB / Sensitivity = 0.0001 / 0.0075 = 0.013333 °C/LSB

Temperature_Celsius = (Raw_ADC × 0.013333) - 273.0
```

**Sum of Cells Conversion:**

```
SOC register has 20-bit resolution with 2mV per LSB.
The 16-bit value read from Status Register A represents the upper 16 bits.

Stack_Voltage = Raw_SOC × 0.002V

Example: Raw = 15000 → Stack = 15000 × 0.002 = 30.000V
```

---

### 3.2 crc15.c — PEC15 CRC Engine

**Location:** `EMS/crc15.c`, `EMS/crc15.h`

Implements the 15-bit Packet Error Code required by the LTC6811 for all SPI communications.

**Algorithm:**
- Polynomial: `0x4599` (x¹⁵ + x¹⁴ + x¹⁰ + x⁸ + x⁷ + x⁴ + x³ + 0)
- Initial remainder: `16` (0x0010)
- Processing: Bit-by-bit, MSB first, each byte XOR'd shifted left by 7
- Output: 16-bit value (15-bit CRC left-shifted by 1 bit)

```
Input bytes:  [0x00, 0x01]
Processing:   Each bit processed against polynomial
Output:       16-bit PEC value → split into MSB and LSB for SPI frame
```

**Usage:**

```c
uint8_t cmd[2] = {0x00, 0x04};  /* RDCVA command */
uint16_t pec = pec15_calc(2, cmd);
cmd[2] = (uint8_t)(pec >> 8);   /* PEC MSB */
cmd[3] = (uint8_t)(pec);        /* PEC LSB */
```

---

### 3.3 ltc6811.c — LTC6811 SPI Driver

**Location:** `Drivers/LTC6811/ltc6811.c`, `Drivers/LTC6811/ltc6811.h`

This is the low-level hardware communication layer. It handles:
- SPI wakeup, command transmission, and register read/write
- PEC generation and attachment
- ADC conversion start commands
- Raw data decode with calibration coefficient application

#### Initialization (`LTC6811_Init`)

```c
void LTC6811_Init(void)
{
    /* 1. Enable DWT cycle counter for microsecond delays */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    /* 2. Initialize default calibration: Gain=1.0, Offset=0.0 */
    for (int i = 0; i < 12; i++) {
        EMS_Data.CellGain[i] = 1.0f;
        EMS_Data.CellOffset[i] = 0.0f;
    }

    /* 3. Write initial configuration (REFON=1, UV/OV thresholds) */
    LTC6811_WriteConfig();
}
```

#### Cell Voltage Decode with Calibration (`LTC6811_DecodeCellGroup`)

This is where calibration coefficients are applied:

```c
static void LTC6811_DecodeCellGroup(uint8_t *rxBuf, uint8_t startIndex)
{
    for each of 3 cells in the register group:
        raw = (rxBuf[2n+1] << 8) | rxBuf[2n];      /* 16-bit little-endian */
        EMS_Data.RawCell[i] = raw;                    /* Store uncalibrated */
        raw_voltage = raw * 0.0001;                   /* Convert: 100µV/LSB */
        EMS_Data.CellVoltage[i] = (raw_voltage * CellGain[i]) + CellOffset[i];
}
```

**Register layout for cell voltages:**

```
Cell Register Group A (RDCVA - Cmd 0x0004):
  Byte [0:1] → Cell 1   (little-endian 16-bit)
  Byte [2:3] → Cell 2
  Byte [4:5] → Cell 3

Cell Register Group B (RDCVB - Cmd 0x0006):
  Byte [0:1] → Cell 4
  Byte [2:3] → Cell 5
  Byte [4:5] → Cell 6

Cell Register Group C (RDCVC - Cmd 0x0008):
  Byte [0:1] → Cell 7
  Byte [2:3] → Cell 8
  Byte [4:5] → Cell 9

Cell Register Group D (RDCVD - Cmd 0x000A):
  Byte [0:1] → Cell 10
  Byte [2:3] → Cell 11
  Byte [4:5] → Cell 12
```

#### Configuration Register Layout

The `LTC6811_WriteConfig()` and `LTC6811_WriteConfigWithDCC()` functions pack the following into the 6-byte configuration register:

```
CFGR0: [GPIO5:1 pull-down][REFON][Reserved][ADCOPT]
       We set REFON=1 (bit 2) → 0x04
       This keeps the internal reference powered between conversions.

CFGR1: [VUV[7:0]]
       Lower 8 bits of the under-voltage comparison value.

CFGR2: [VUV[11:8] | VOV[3:0]]
       Upper nibble of VUV + lower nibble of VOV.

CFGR3: [VOV[11:4]]
       Upper 8 bits of the over-voltage comparison value.

CFGR4: [DCC4:DCC1 | VOV cont. in some configs]
       Lower nibble: DCC bits 1-4 for cell balancing.
       Upper nibble: continuation of OV threshold.

CFGR5: [DCC12:DCC5]
       DCC bits 5-12 for cell balancing.
```

**UV/OV Packing Example (VUV=437=0x1B5, VOV=2625=0xA41):**

```
tx_data[2] = 0xB5          /* VUV[7:0]                  */
tx_data[3] = 0x11          /* VUV[11:8]=0x1 | VOV[3:0]=0x1 shifted */
tx_data[4] = 0xA4          /* VOV[11:4]                 */
```

#### Status Register Decode

**Status Register A (RDSTATA - Cmd 0x0010):**

```
Byte [0:1] → SOC (Sum of Cells):       Raw × 0.002V
Byte [2:3] → ITMP (Internal Temp):     (Raw × 0.013333) - 273.0 °C
Byte [4:5] → VA (Analog Supply):       Raw × 0.0001V
```

**Status Register B (RDSTATB - Cmd 0x0012):**

```
Byte [0:1] → VD (Digital Supply):      Raw × 0.0001V
Byte [2:3] → UV/OV flags (per cell)    Bitfield
Byte [4:5] → VREF2 (Reference 2):      Raw × 0.0001V
```

---

### 3.4 ems_monitor.c — Measurement Orchestrator

**Location:** `EMS/ems_monitor.c`, `EMS/ems_monitor.h`

Orchestrates the complete measurement cycle in a single function call:

```
EMS_UpdateMeasurements()
│
├── STEP 1: LTC6811_ReadAllCells()          [Reads cells 1-12]
│           ├── LTC6811_StartCellADC_SC()   [Start conversion]
│           ├── HAL_Delay(10)               [Wait for conversion]
│           ├── RDCVA → decode cells 1-3
│           ├── RDCVB → decode cells 4-6
│           ├── RDCVC → decode cells 7-9
│           └── RDCVD → decode cells 10-12
│
├── HAL_Delay(20)
│
├── STEP 2: Status ADC
│           ├── LTC6811_StartStatusADC()    [ADSTAT command]
│           ├── HAL_Delay(20)
│           ├── LTC6811_ReadStatusA()       [SOC, Temp, VA]
│           ├── HAL_Delay(10)
│           └── LTC6811_ReadStatusB()       [VD, VREF2]
│
├── HAL_Delay(10)
│
└── STEP 3: LTC6811_ReadGPIO1()            [GPIO1 voltage]
            ├── LTC6811_StartGPIOADC()     [ADAX command]
            ├── HAL_Delay(10)
            └── RDAUXA → decode GPIO1
```

**Total measurement cycle time: ~120ms** (dominated by HAL_Delay calls for ADC settling)

---

### 3.5 ems_temperature.c — NTC Temperature Calculator

**Location:** `EMS/ems_temperature.c`, `EMS/ems_temperature.h`

Converts GPIO1 voltage to temperature using the Steinhart-Hart simplified Beta equation.

#### Circuit

```
VREG (Analog Supply, ~5V)
  │
  ├── 1MΩ Pull-up Resistor
  │
  ├──── GPIO1 (LTC6811 Auxiliary ADC Input)
  │
  └── NTC Thermistor (10K @ 25°C, β=3950)
  │
  GND (V-)
```

#### Mathematical Derivation

**Step 1: Calculate NTC Resistance from Voltage Divider**

```
V_GPIO = VREG × R_NTC / (R_PULLUP + R_NTC)

Solving for R_NTC:
R_NTC = R_PULLUP × V_GPIO / (VREG - V_GPIO)
      = 1,000,000 × V_GPIO / (V_ANALOG_SUPPLY - V_GPIO)
```

**Step 2: Beta Equation (Simplified Steinhart-Hart)**

```
1/T = 1/T₀ + (1/β) × ln(R_NTC / R₀)

Where:
  T₀ = 25°C + 273.15 = 298.15K  (reference temperature)
  β  = 3950                       (NTC beta coefficient)
  R₀ = 10,000Ω                    (resistance at T₀)

Result T is in Kelvin, subtract 273.15 for Celsius.
```

**Implementation in code:**

```c
steinhart = ntc_resistance / NTC_R_NOMINAL;      /* R/R₀         */
steinhart = logf(steinhart);                       /* ln(R/R₀)     */
steinhart /= NTC_BETA;                             /* ln(R/R₀) / β */
steinhart += 1.0f / (NTC_T_NOMINAL + 273.15f);    /* + 1/T₀       */
steinhart = 1.0f / steinhart;                      /* T in Kelvin  */
steinhart -= 273.15f;                              /* T in Celsius */
```

**Example Calculation:**

```
V_GPIO = 0.8234V,  V_ANALOG = 5.0012V

R_NTC = 1,000,000 × 0.8234 / (5.0012 - 0.8234) = 197,170Ω

steinhart = 197170 / 10000 = 19.717
steinhart = ln(19.717) = 2.9815
steinhart = 2.9815 / 3950 = 0.000755
steinhart = 0.000755 + 1/298.15 = 0.000755 + 0.003354 = 0.004109
steinhart = 1 / 0.004109 = 243.37K
Temperature = 243.37 - 273.15 = -29.78°C
```

> **Note:** `EMS_UpdateTemperature()` uses the measured `AnalogSupply` from Status Register A rather than a hardcoded 5V value, which provides more accurate results.

---

### 3.6 ems_fault.c — Fault Detection Engine

**Location:** `EMS/ems_fault.c`, `EMS/ems_fault.h`

#### Over/Under Voltage Detection (`EMS_CheckFaults`)

Iterates over all 12 cells and sets fault flags:

```
For each cell (i = 0 to 11):
  IF CellVoltage[i] < CELL_UV_THRESHOLD (0.5V):
    → Cell is unused/floating, clear both flags
  ELSE IF CellVoltage[i] > CELL_OV_THRESHOLD (4.2V):
    → Set OVFlag[i] = 1
  ELSE IF CellVoltage[i] < CELL_UV_THRESHOLD (0.5V):
    → Set UVFlag[i] = 1
```

#### Open Wire Detection (`EMS_OpenWireTest`)

Uses the LTC6811's ADOW (ADC Open Wire) command which injects a small test current:

```
STEP 1: Execute ADOW with Pull-Up current source
  → LTC6811_StartADOW(1)
  → Read all cell voltages → store as pullup_voltage[]

STEP 2: Execute ADOW with Pull-Down current source
  → LTC6811_StartADOW(0)
  → Read all cell voltages → store as pulldown_voltage[]

STEP 3: Compare results
  For each cell:
    diff = |pullup_voltage[i] - pulldown_voltage[i]|
    IF diff > OPEN_WIRE_THRESHOLD (0.4V):
      → OpenWireFlag[i] = 1  (open wire suspected)
```

**How it works:** A connected cell wire presents a low impedance path, so pull-up and pull-down readings are nearly identical. An open (broken) wire creates a high-impedance node where the test current causes a large voltage difference.

---

### 3.7 ems_balancing.c — Passive Cell Balancing

**Location:** `EMS/ems_balancing.c`, `EMS/ems_balancing.h`

#### Algorithm

The passive balancing algorithm identifies cells with higher voltage than the minimum and enables internal discharge resistors (via DCC bits) to bleed off excess charge:

```
SAFETY CHECK:
  IF InternalTemp > 60°C → STOP all balancing, return

STEP 1: Find minimum valid cell voltage
  min_voltage = lowest CellVoltage where CellVoltage > 1.0V
  (cells below 1.0V are considered unused/floating)

STEP 2: Select cells for discharge
  For each valid cell:
    diff = CellVoltage[i] - min_voltage
    IF diff > CELL_BALANCE_THRESHOLD (50mV):
      → Set DCC bit for cell i
      → CellBalanceStatus[i] = 1

STEP 3: Write DCC bits to LTC6811
  → LTC6811_WriteConfigWithDCC(dcc_bits)
```

#### DCC Bit Mapping

```
dcc_bits is a 16-bit value where bits 0-11 correspond to cells 1-12:

Bit  0 → DCC1  (Cell 1 discharge)
Bit  1 → DCC2  (Cell 2 discharge)
...
Bit 11 → DCC12 (Cell 12 discharge)

These are packed into Configuration Register bytes:
  CFGR4[7:4] → DCC4:DCC1
  CFGR5[7:0] → DCC12:DCC5
```

#### Stopping Balancing

`EMS_StopBalancing()` writes configuration with `dcc_bits = 0x0000`, clearing all discharge switches and resetting all `CellBalanceStatus[]` flags.

> **Important:** Balancing is commented out in `main.c` by default (`// EMS_CellBalancing();`). Enable it only when cells are connected and validated.

---

### 3.8 ems_calibration.c — Dynamic Calibration Engine

**Location:** `EMS/ems_calibration.c`, `EMS/ems_calibration.h`

#### How Calibration Works

Calibration coefficients are stored in `EMS_Data.CellGain[i]` and `EMS_Data.CellOffset[i]`. They are applied **inside** `LTC6811_DecodeCellGroup()` during every ADC read:

```
CellVoltage[i] = (RawADC × 0.0001 × CellGain[i]) + CellOffset[i]
```

This means calibration is transparent — all downstream consumers (fault detection, balancing, debug output) automatically see calibrated values.

#### Single-Point Calibration

**When to use:** The ADC gain is correct but there is a constant DC offset.

**Math:**

```
Given:
  Raw uncalibrated voltage = RawCell × 0.0001V
  Actual reference voltage (from multimeter)

Calculate:
  Offset = Actual_Voltage - Raw_Uncalibrated_Voltage
  Gain = 1.0 (unchanged)
```

**API:**

```c
/* Calibrate one cell */
void EMS_CalibrateCellSinglePoint(uint8_t cell_index, float actual_voltage);

/* Calibrate all 12 cells at once */
void EMS_CalibrateAllCellsSinglePoint(const float *actual_voltages);
```

**Step-by-step procedure:**

1. Connect a known voltage source (or measure with precision multimeter)
2. Let the firmware read several cycles to get a stable raw reading
3. Call `EMS_CalibrateCellSinglePoint(cell_index, actual_voltage)`
4. The function reads `EMS_Data.RawCell[cell_index]`, computes offset, and stores it
5. All subsequent readings will be corrected

**Example:**

```c
/* Cell 3 (index 2) reads 2.10V but actual is 2.60V */
/* The offset will be: 2.60 - 2.10 = +0.50V         */
EMS_CalibrateCellSinglePoint(2, 2.60f);
```

#### Two-Point Calibration

**When to use:** Both gain and offset errors need correction (more accurate).

**Math:**

```
Given two reference points:
  Point 1: Actual_Low voltage,  Raw_Low reading
  Point 2: Actual_High voltage, Raw_High reading

Calculate:
  Gain   = (Actual_High - Actual_Low) / (Raw_High - Raw_Low)
  Offset = Actual_High - (Gain × Raw_High)
```

**API:**

```c
/* Calibrate one cell with two reference points */
void EMS_CalibrateCellTwoPoint(uint8_t cell_index,
                               float low_actual, float low_raw,
                               float high_actual, float high_raw);

/* Calibrate all 12 cells at once */
void EMS_CalibrateAllCellsTwoPoint(const float *low_actuals,  const float *low_raws,
                                   const float *high_actuals, const float *high_raws);
```

**Step-by-step procedure:**

1. Apply a LOW reference voltage to the cell input
2. Record the raw uncalibrated reading from `EMS_Data.CellVoltage[]` (before any calibration)
3. Record the actual voltage from your reference/multimeter
4. Apply a HIGH reference voltage to the same cell input
5. Record the raw uncalibrated reading and actual voltage
6. Call `EMS_CalibrateCellTwoPoint()` with all four values

**Example:**

```c
/* Cell 5 (index 4):
 *   At 1.50V actual, firmware reads 1.48V
 *   At 4.00V actual, firmware reads 3.92V
 *
 * Gain   = (4.00 - 1.50) / (3.92 - 1.48) = 2.50 / 2.44 = 1.0246
 * Offset = 4.00 - (1.0246 × 3.92) = 4.00 - 4.0164 = -0.0164V
 */
EMS_CalibrateCellTwoPoint(4, 1.50f, 1.48f, 4.00f, 3.92f);
```

#### Important Notes

- **Calibration is volatile**: Coefficients are stored in RAM and reset to defaults (Gain=1.0, Offset=0.0) on every power cycle. To persist calibration, store values in Flash/EEPROM (not yet implemented).
- **Call calibration AFTER measurements**: The calibration functions read `EMS_Data.RawCell[]`, which must be populated by a prior `EMS_UpdateMeasurements()` call.
- **Hardware issues first**: If `VREF2 < 3.0V`, fix the V+ supply voltage before calibrating. Calibrating over a bad reference creates non-linear errors.

---

### 3.9 usb_debug.c — USB CDC Debug Output

**Location:** `EMS/usb_debug.c`, `EMS/usb_debug.h`

Pure formatting module — reads `EMS_Data_t` fields and prints formatted ASCII via `printf()`.

#### Available Print Functions

| Function | Output |
|----------|--------|
| `USB_PrintCellVoltages()` | All 12 cell voltages + calculated total |
| `USB_PrintStatusA()` | Sum of Cells, Internal Temp, Analog Supply |
| `USB_PrintStatusB()` | Digital Supply, REF2 Voltage |
| `USB_PrintGPIO1()` | GPIO1 raw voltage |
| `USB_PrintGPIO1Temperature()` | GPIO1 voltage + NTC temperature |
| `USB_PrintUVOV()` | Decoded UV/OV threshold values |
| `USB_PrintFaultStatus()` | OV/UV/Open Wire flags (only prints faulted cells) |
| `USB_PrintDebugMessage()` | Custom string message |

All functions accept a `const EMS_Data_t *` pointer, keeping them decoupled from the global variable.

---

### 3.10 ems_modbus.c — Modbus RTU Register Mapping

**Location:** `EMS/ems_modbus.c`, `EMS/ems_modbus.h`

This module maps the `EMS_Data_t` fields to the Modbus Input Registers array (`Input_Registers_Database[]`), converting float values into scaled integers suitable for 16-bit registers.

#### Register Layout (FC04 - Read Input Registers)

| Register Offset | Field | Scale Factor | Output Type |
|-----------------|-------|--------------|-------------|
| **0 - 11** | `CellVoltage[0..11]` | ×10000 | 0.1mV (scaled float) |
| **12 - 23** | `RawCell[0..11]` | ×1 | Raw ADC count |
| **24** | `StackVoltage` | ×1000 | 1mV (scaled float) |
| **25** | `InternalTemp` | ×100 | 0.01°C (scaled float) |
| **26** | `AnalogSupply` (VREG) | ×10000 | 0.1mV (scaled float) |
| **27** | `DigitalSupply` (VD) | ×10000 | 0.1mV (scaled float) |
| **28** | `ReferenceVoltage2` | ×10000 | 0.1mV (scaled float) |
| **29** | `GPIO1Voltage` | ×10000 | 0.1mV (scaled float) |
| **30** | `Temperature` (NTC) | ×100 | 0.01°C (scaled float) |
| **31** | `NTCResistance` | ÷100 | 100Ω units (scaled float) |
| **32** | `UVThreshold` | ×10000 | 0.1mV (scaled float) |
| **33** | `OVThreshold` | ×10000 | 0.1mV (scaled float) |
| **34 - 45** | `OVFlag[0..11]` | ×1 | 0 or 1 |
| **46 - 57** | `UVFlag[0..11]` | ×1 | 0 or 1 |
| **58 - 69** | `OpenWireFlag[0..11]` | ×1 | 0 or 1 |
| **70 - 81** | `CellBalanceStatus[0..11]` | ×1 | 0 or 1 |
| **82** | `DCCBits` | ×1 | 12-bit balancing bitmask |
| **83** | `PECErrorCount` | ×1 | Lower 16-bits of counter |
| **84** | `CommunicationStatus` | ×1 | 0=OK, non-zero=error |
| **85** | `SystemFaultFlags` | ×1 | Lower 16-bits of bitfield |

#### Data Integration
Every measurement cycle in `main.c` ends by calling `EMS_UpdateModbusRegisters()`. The values are written atomically to `Input_Registers_Database[]`, allowing external Modbus masters to retrieve real-time state using Function Code 4.

---

## 4. Main Application Loop

The main loop in `Core/Src/main.c` executes a simple sequential pipeline:

```c
while (1)
{
    /* STEP 1: Read all hardware measurements */
    EMS_UpdateMeasurements();
    HAL_Delay(20);

    /* STEP 2: Calculate NTC temperature from GPIO1 */
    EMS_UpdateTemperature();

    /* STEP 3: Check for OV/UV faults */
    EMS_CheckFaults();

    /* STEP 4: Open wire detection */
    EMS_OpenWireTest();
    HAL_Delay(20);

    /* STEP 5: Print all diagnostics via USB CDC */
    USB_PrintCellVoltages(&EMS_Data);
    USB_PrintStatusA(&EMS_Data);
    USB_PrintStatusB(&EMS_Data);
    USB_PrintGPIO1Temperature(&EMS_Data);

    /* STEP 6: Cell balancing (disabled by default) */
    // EMS_CellBalancing();

    HAL_Delay(2000);  /* ~2 second loop period */
}
```

**Loop timing:** Each iteration takes approximately **2.2 seconds** (dominated by the final `HAL_Delay(2000)` plus ~200ms of measurement and print time).

---

## 5. Configuration Reference

All configuration is centralized in `Core/Inc/ems_data.h`. Here is every configurable macro with its derivation:

### How to Calculate LTC6811 Register Values

| Desired Threshold | Formula | Result |
|-------------------|---------|:------:|
| 0.5V UV | 0.5 / 0.0016 | 312 |
| 0.7V UV | 0.7 / 0.0016 | 437 |
| 1.0V UV | 1.0 / 0.0016 | 625 |
| 2.5V UV | 2.5 / 0.0016 | 1562 |
| 3.0V UV | 3.0 / 0.0016 | 1875 |
| 3.6V OV | 3.6 / 0.0016 | 2250 |
| 4.0V OV | 4.0 / 0.0016 | 2500 |
| 4.2V OV | 4.2 / 0.0016 | 2625 |
| 4.35V OV | 4.35 / 0.0016 | 2718 |

**General formula:** `Register = Voltage / 0.0016`

### ADC Conversion Factor Derivations

| Factor | Value | How It's Derived |
|--------|:-----:|------------------|
| Cell LSB | 100µV | LTC6811 datasheet: Cell measurement resolution |
| GPIO LSB | 100µV | Same ADC used for auxiliary inputs |
| SOC LSB | 2mV | Sum of Cells has coarser resolution than individual cells |
| Internal Temp | 0.013333°C/LSB | 100µV/LSB ÷ 7.5mV/°C sensor gain = 0.013333 |
| Supply LSB | 100µV | Same ADC resolution for supply monitoring |
| UV/OV LSB | 1.6mV | 16 × 100µV = comparison resolution |

---

## 6. Calibration Procedures

### Procedure A: Single-Point Calibration for One Cell

**Equipment:** Precision multimeter (4½ digit or better)

1. Power on the system with cells connected
2. Wait for 3-5 measurement cycles (observe USB debug output stabilize)
3. Measure the actual voltage across the target cell with your multimeter
4. Note the firmware-reported voltage for the same cell from USB output
5. In your code, add the calibration call after `LTC6811_Init()`:

```c
/* Example: Cell 3 (index 2), multimeter reads 2.6000V */
EMS_CalibrateCellSinglePoint(2, 2.6000f);
```

6. Rebuild and flash
7. Verify the corrected reading matches your multimeter

### Procedure B: Single-Point Batch Calibration (All 12 Cells)

1. Measure all 12 cells with a precision multimeter
2. Create an array of actual voltages:

```c
float actual[12] = {
    3.312f,  /* Cell 1 actual  */
    3.298f,  /* Cell 2 actual  */
    2.600f,  /* Cell 3 actual  */
    0.000f,  /* Cell 4 unused  */
    0.000f,  /* Cell 5 unused  */
    0.000f,  /* Cell 6 unused  */
    0.000f,  /* Cell 7 unused  */
    0.000f,  /* Cell 8 unused  */
    0.000f,  /* Cell 9 unused  */
    0.000f,  /* Cell 10 unused */
    0.000f,  /* Cell 11 unused */
    0.000f   /* Cell 12 unused */
};
EMS_CalibrateAllCellsSinglePoint(actual);
```

> **Warning:** Only calibrate cells that are actually connected. Calibrating unused cells with 0.0V will set a negative offset that corrupts future readings.

### Procedure C: Two-Point Calibration

**Equipment:** Adjustable precision voltage source + multimeter

1. Apply a LOW voltage (e.g., 1.5V) to the cell input
2. Record the raw firmware reading and the actual multimeter reading
3. Apply a HIGH voltage (e.g., 4.0V) to the same cell input
4. Record both readings again
5. Call:

```c
EMS_CalibrateCellTwoPoint(cell_index,
    low_actual, low_raw,
    high_actual, high_raw);
```

---

## 7. Troubleshooting

### Cell Voltages Read Too Low

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| All cells read ~20% low | VREF2 < 3.0V due to low V+ supply | Ensure V+ ≥ 11V to the LTC6811 |
| One cell reads offset | Individual channel offset | Use single-point calibration |
| Cells read 0.0000V | SPI communication failure | Check wiring, verify PEC in debug RAW output |
| Sum of Cells ≠ individual sum | SOC uses different ADC path | Normal — SOC has 2mV LSB vs 100µV for cells |

### Reference Voltage Issues

The LTC6811 internal LDO requires a minimum input voltage:

```
V+ (pin) ≥ 11V → VREG = 5.0V, VREF2 = 3.0V  ✓ Normal
V+ (pin) = 8V  → VREG < 5.0V, VREF2 < 3.0V  ✗ Reference sagging
V+ (pin) = 5V  → VREG collapsed, no reliable measurements  ✗ Critical
```

**Always check:** `Status B → REF2 Voltage` in the debug output. It should be ≈ 3.0000V.

### Open Wire False Positives

- Unused cell channels (no cell connected) will always show as "open wire"
- Cells with very low voltage (< 0.5V) may trigger false positives
- The threshold of 0.4V can be adjusted via `OPEN_WIRE_THRESHOLD` in `ems_data.h`

### USB CDC Not Working

- Ensure USB cable supports data (not charge-only)
- Check Windows Device Manager for "STMicroelectronics Virtual COM Port"
- The `_write()` override in `main.c` includes a 1ms delay for USB transfer — do not remove
- If no output appears, try increasing the delay or adding `HAL_Delay(10)` after USB init

---

## 8. Build System Notes

### Include Paths Required

The project requires these include paths in `.cproject`:

```
Core/Inc
Drivers/LTC6811
EMS
Drivers/STM32F4xx_HAL_Driver/Inc
Drivers/CMSIS/Device/ST/STM32F4xx/Include
Drivers/CMSIS/Include
USB_DEVICE/App
USB_DEVICE/Target
Middlewares/ST/STM32_USB_Device_Library/Core/Inc
Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc
```

### Source Entries Required

The build system must compile sources from:

```
Core/Src          → main.c, HAL MSP, interrupt handlers, syscalls
Drivers/LTC6811   → ltc6811.c
EMS               → All EMS module .c files
USB_DEVICE        → USB CDC application layer
Middlewares       → ST USB library core
```

### Adding New Modules

1. Create `.c` and `.h` files in the appropriate directory
2. Add `#include` to `main.c` or calling module
3. If the new directory is not already in the source entries, add it in `.cproject`:
   - Right-click project → **Properties → C/C++ Build → Settings**
   - Under **Source Location**, add the new directory
4. Add the include path if headers are in a new directory
5. **Clean and rebuild** (`Project → Clean → Build All`)

---

*Last updated: 2026-07-07*
*Firmware Version: 1.0*
*Target: STM32F411CEU6 + LTC6811-2 (DC2260A)*
