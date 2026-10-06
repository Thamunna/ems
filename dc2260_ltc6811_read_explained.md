# How Data is Read from DC2260 (LTC6811-2) — Full Explanation

## 🔷 What is DC2260 / LTC6811?

The **DC2260** is Analog Devices' demo board for the **LTC6811-2** IC. The LTC6811-2 is a **multi-cell battery stack monitor** that:
- Measures up to **12 cell voltages** simultaneously
- Measures **GPIO/auxiliary voltages** (for NTC temperature sensors)
- Measures **stack voltage, internal die temperature, supply voltages**
- Communicates via **SPI** with the host MCU (STM32F411 in this project)

---

## 🔷 Big Picture — The Full Read Flow

```
STM32 MCU
    │
    │  SPI (MOSI / MISO / SCK / CS)
    ▼
LTC6811-2 (on DC2260 board)
    │
    ├── ADC internally measures cell voltages
    ├── Stores results in internal registers (RDCVA, RDCVB, RDCVC, RDCVD)
    ├── Stores status in RDSTATA, RDSTATB
    └── Stores GPIO in RDAUXA
```

Every read follows this 4-step pattern:
1. **Wakeup** the IC
2. **Send a Command** (ADCV / ADSTAT / ADAX) to trigger ADC conversion
3. **Wait** for conversion to complete
4. **Send a Read Command** (RDCVA, RDSTATA, etc.) → **Receive 8 bytes back**

---

## 🔷 STEP 0 — Initialization (`LTC6811_Init`)

**File:** [ltc6811.c](file:///c:/OSSUS_BIO/STM_32_MC/EMS_FW_v1.0/Drivers/LTC6811/ltc6811.c#L79-L94)

```c
void LTC6811_Init(void)
{
    // Enable DWT hardware cycle counter for microsecond-accurate delays
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL        |= DWT_CTRL_CYCCNTENA_Msk;

    // Set default calibration: Gain=1.0, Offset=0.0 for all 12 cells
    for (int i = 0; i < EMS_TOTAL_CELLS; i++) {
        EMS_Data.CellGain[i]   = 1.0f;
        EMS_Data.CellOffset[i] = 0.0f;
    }

    // Write initial configuration register (REFON=1, UV/OV thresholds)
    LTC6811_WriteConfig();
}
```

**What happens:**
- The DWT (Data Watchpoint and Trace) peripheral is enabled so `delay_us()` works accurately
- Default calibration is applied (no scaling or offset — raw voltage = real voltage)
- `LTC6811_WriteConfig()` sends the **WRCFG** command to configure the IC with:
  - **REFON = 1**: Internal voltage reference stays ON between measurements (avoids settling delay)
  - **VUV = 437** → Under-voltage threshold = 0.7V
  - **VOV = 2625** → Over-voltage threshold = 4.2V

---

## 🔷 STEP 1 — Wakeup (`LTC6811_Wakeup`)

**File:** [ltc6811.c](file:///c:/OSSUS_BIO/STM_32_MC/EMS_FW_v1.0/Drivers/LTC6811/ltc6811.c#L103-L112)

```c
void LTC6811_Wakeup(void)
{
    LTC6811_CS_LOW();         // Pull CS low
    delay_us(300);            // Hold CS low for 300µs
    LTC6811_CS_HIGH();        // Release CS
    delay_us(10);             // Settle for 10µs
}
```

**Why is this needed?**
The LTC6811 enters a low-power **sleep state** after ~1.8 seconds of inactivity. Pulling `CS` LOW for **≥300µs** is the wakeup signal. The IC then takes **10µs** to settle before it can receive valid SPI data.

> **CS (Chip Select)** is controlled via GPIO pin `CHIP_SELECT_Pin` defined in CubeMX.
> `LTC6811_CS_LOW()` → GPIO = 0 (asserted)
> `LTC6811_CS_HIGH()` → GPIO = 1 (de-asserted)

---

## 🔷 STEP 2 — PEC15 Checksum (`pec15_calc`)

**File:** [crc15.c](file:///c:/OSSUS_BIO/STM_32_MC/EMS_FW_v1.0/EMS/crc15.c#L28-L51)

Before any command or data goes over SPI, a **2-byte PEC (Packet Error Code)** must be appended.

```c
uint16_t pec15_calc(uint8_t len, uint8_t *data)
{
    uint16_t remainder = 16;          // Initial remainder = 16
    uint16_t polynomial = 0x4599;     // LTC6811 CRC polynomial

    for (uint8_t i = 0; i < len; i++) {
        remainder ^= (uint16_t)data[i] << 7;
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (remainder & 0x4000)
                remainder = (remainder << 1) ^ polynomial;
            else
                remainder = (remainder << 1);
        }
    }
    return (remainder << 1);   // Left-shift 1 to get 15-bit CRC in [15:1]
}
```

**How it works:**
- Uses **CRC-15** with polynomial **0x4599** (as specified in the LTC6811 datasheet)
- The IC independently calculates PEC on received bytes
- If PEC doesn't match → **communication error** (tracked in `EMS_Data.PECErrorCount`)

---

## 🔷 STEP 3 — Sending a Command (`LTC6811_SendCommand`)

**File:** [ltc6811.c](file:///c:/OSSUS_BIO/STM_32_MC/EMS_FW_v1.0/Drivers/LTC6811/ltc6811.c#L123-L146)

```c
HAL_StatusTypeDef LTC6811_SendCommand(uint8_t cmd0, uint8_t cmd1)
{
    uint8_t cmd[4];
    uint16_t pec;

    cmd[0] = cmd0;
    cmd[1] = cmd1;
    pec    = pec15_calc(2, cmd);   // Calculate PEC over 2 command bytes
    cmd[2] = (uint8_t)(pec >> 8); // PEC high byte
    cmd[3] = (uint8_t)(pec);      // PEC low byte

    LTC6811_Wakeup();              // Wake IC first

    LTC6811_CS_LOW();
    HAL_SPI_Transmit(&hspi1, cmd, 4, 100); // Send 4 bytes over SPI
    LTC6811_CS_HIGH();

    return status;
}
```

**SPI frame structure for a command:**
```
┌────────┬────────┬────────┬────────┐
│ CMD[0] │ CMD[1] │ PEC[H] │ PEC[L] │   ← 4 bytes total
└────────┴────────┴────────┴────────┘
```

**Example — Start Cell ADC (`ADCVSC`):**
```c
LTC6811_StartCellADC_SC()
→ LTC6811_SendCommand(0x04, 0x67)
```
`0x0467` = **ADCVSC** command → Tells the IC to start all 12 cell ADC conversions + sum of cells.

---

## 🔷 STEP 4 — Reading a Register (`LTC6811_ReadRegister`)

**File:** [ltc6811.c](file:///c:/OSSUS_BIO/STM_32_MC/EMS_FW_v1.0/Drivers/LTC6811/ltc6811.c#L157-L204)

```c
HAL_StatusTypeDef LTC6811_ReadRegister(uint8_t cmd0, uint8_t cmd1, uint8_t *rxBuf)
{
    uint8_t cmd[4];
    uint8_t dummy_tx[8] = {0};   // Dummy bytes to clock out MISO

    // Build 4-byte command packet with PEC
    cmd[0] = cmd0;
    cmd[1] = cmd1;
    pec    = pec15_calc(2, cmd);
    cmd[2] = (uint8_t)(pec >> 8);
    cmd[3] = (uint8_t)(pec);

    LTC6811_Wakeup();

    LTC6811_CS_LOW();

    // 1) Send read command (4 bytes)
    HAL_SPI_Transmit(&hspi1, cmd, 4, 100);

    // 2) Clock out 8 bytes → IC sends back 6 data + 2 PEC
    HAL_SPI_TransmitReceive(&hspi1, dummy_tx, rxBuf, 8, 100);

    LTC6811_CS_HIGH();

    // Debug: Print raw bytes
    printf("RAW: %02X %02X %02X %02X %02X %02X\r\n",
           rxBuf[0], rxBuf[1], rxBuf[2], rxBuf[3], rxBuf[4], rxBuf[5]);

    return status;
}
```

**SPI transaction timeline:**
```
        ┌──── CS LOW ───────────────────────────────────────┐
MOSI:   │ CMD0 │ CMD1 │ PEC_H │ PEC_L │ 0x00 x 8 bytes     │
MISO:   │ (ignored)           │ D0 │ D1 │ D2 │ D3 │ D4 │ D5 │ PEC_H │ PEC_L │
        └──── CS HIGH ──────────────────────────────────────┘
```

- **Phase 1:** MCU transmits 4-byte read command → IC parses it
- **Phase 2:** MCU clocks 8 dummy bytes on MOSI (all 0x00) → IC sends back **6 data bytes + 2 PEC bytes** on MISO
- `rxBuf[0..5]` = actual register data (little-endian 16-bit pairs)
- `rxBuf[6..7]` = PEC checksum (for error detection — currently stored but not explicitly validated in driver)

---

## 🔷 STEP 5 — Reading All 12 Cell Voltages (`LTC6811_ReadAllCells`)

**File:** [ltc6811.c](file:///c:/OSSUS_BIO/STM_32_MC/EMS_FW_v1.0/Drivers/LTC6811/ltc6811.c#L454-L486)

```c
HAL_StatusTypeDef LTC6811_ReadAllCells(void)
{
    // 1. Start all-cell ADC conversion
    LTC6811_StartCellADC_SC();      // ADCVSC command (0x0467)
    HAL_Delay(10);                  // Wait 10ms for conversion to complete

    // 2. Read Group A → Cells 1, 2, 3
    LTC6811_ReadRegister(0x00, 0x04, rxBuf);   // RDCVA
    LTC6811_DecodeCellGroup(rxBuf, 0);         // → CellVoltage[0..2]

    // 3. Read Group B → Cells 4, 5, 6
    LTC6811_ReadRegister(0x00, 0x06, rxBuf);   // RDCVB
    LTC6811_DecodeCellGroup(rxBuf, 3);         // → CellVoltage[3..5]

    // 4. Read Group C → Cells 7, 8, 9
    LTC6811_ReadRegister(0x00, 0x08, rxBuf);   // RDCVC
    LTC6811_DecodeCellGroup(rxBuf, 6);         // → CellVoltage[6..8]

    // 5. Read Group D → Cells 10, 11, 12
    LTC6811_ReadRegister(0x00, 0x0A, rxBuf);   // RDCVD
    LTC6811_DecodeCellGroup(rxBuf, 9);         // → CellVoltage[9..11]
}
```

**Why 4 groups?** Each LTC6811 register group holds **3 cells × 2 bytes = 6 bytes**. With 12 cells total, 4 register reads (A, B, C, D) are needed.

---

## 🔷 STEP 6 — Decoding Cell Voltages (`LTC6811_DecodeCellGroup`)

**File:** [ltc6811.c](file:///c:/OSSUS_BIO/STM_32_MC/EMS_FW_v1.0/Drivers/LTC6811/ltc6811.c#L47-L66)

```c
static void LTC6811_DecodeCellGroup(uint8_t *rxBuf, uint8_t startIndex)
{
    uint16_t raw;
    float raw_voltage;

    // Cell N+0: bytes [1:0] → little-endian 16-bit
    raw = ((uint16_t)rxBuf[1] << 8) | rxBuf[0];
    EMS_Data.RawCell[startIndex] = raw;
    raw_voltage = raw * 0.0001f;                    // 100µV per LSB
    EMS_Data.CellVoltage[startIndex] =
        (raw_voltage * EMS_Data.CellGain[startIndex])
        + EMS_Data.CellOffset[startIndex];

    // Cell N+1: bytes [3:2]
    raw = ((uint16_t)rxBuf[3] << 8) | rxBuf[2];
    // ... same formula ...

    // Cell N+2: bytes [5:4]
    raw = ((uint16_t)rxBuf[5] << 8) | rxBuf[4];
    // ... same formula ...
}
```

**Decoding formula (per LTC6811 datasheet):**

```
Raw register (16-bit, little-endian) → assembled: raw = (rxBuf[1]<<8) | rxBuf[0]
Voltage (V) = raw × 100µV = raw × 0.0001

With calibration:
Voltage = (raw × 0.0001 × Gain) + Offset
```

**Example:**
```
rxBuf[0] = 0xE8, rxBuf[1] = 0x0F
raw = (0x0F << 8) | 0xE8 = 0x0FE8 = 4072
Voltage = 4072 × 0.0001 = 0.4072V  ← wait, that's too low...

Actually for a nominal Li-ion cell at 3.7V:
raw ≈ 37000
rxBuf[0] = 0x10, rxBuf[1] = 0x90
raw = 0x9010 = 36880  → 36880 × 0.0001 = 3.688V ✓
```

**Register byte layout (one 6-byte group):**
```
Byte  0    1    2    3    4    5
     ├────┤├────┤├────┤├────┤├────┤├────┤
     │  Cell 1  │  Cell 2  │  Cell 3  │
     │ Low│High │ Low│High │ Low│High │
     └──────────┴──────────┴──────────┘
     (little-endian 16-bit each)
```

---

## 🔷 STEP 7 — Reading Status Register A (`LTC6811_ReadStatusA`)

**File:** [ltc6811.c](file:///c:/OSSUS_BIO/STM_32_MC/EMS_FW_v1.0/Drivers/LTC6811/ltc6811.c#L495-L517)

```c
HAL_StatusTypeDef LTC6811_ReadStatusA(void)
{
    LTC6811_ReadRegister(0x00, 0x10, rxBuf);    // RDSTATA command

    // Sum of All Cells voltage (bytes 1:0)
    raw = ((uint16_t)rxBuf[1] << 8) | rxBuf[0];
    EMS_Data.StackVoltage = raw * 0.002f;        // LSB = 2mV

    // Internal Die Temperature (bytes 3:2)
    raw = ((uint16_t)rxBuf[3] << 8) | rxBuf[2];
    EMS_Data.InternalTemp = (raw * 0.013333f) - 273.0f;
    // 0.013333 = 100µV / 7.5mV/°C sensor gain
    // -273 = Kelvin to Celsius offset

    // Analog Supply Voltage VREG (bytes 5:4)
    raw = ((uint16_t)rxBuf[5] << 8) | rxBuf[4];
    EMS_Data.AnalogSupply = raw * 0.0001f;       // LSB = 100µV
}
```

| Register Bytes | Field | LSB | Formula |
|---|---|---|---|
| [1:0] | Sum of Cells (SC) | 2mV | `raw × 0.002` |
| [3:2] | Internal Temp (ITMP) | 100µV | `(raw × 0.01333) - 273` |
| [5:4] | Analog Supply (VA) | 100µV | `raw × 0.0001` |

---

## 🔷 STEP 8 — Reading Status Register B (`LTC6811_ReadStatusB`)

**File:** [ltc6811.c](file:///c:/OSSUS_BIO/STM_32_MC/EMS_FW_v1.0/Drivers/LTC6811/ltc6811.c#L526-L544)

```c
HAL_StatusTypeDef LTC6811_ReadStatusB(void)
{
    LTC6811_ReadRegister(0x00, 0x12, rxBuf);     // RDSTATB command

    // Digital Supply Voltage VD (bytes 1:0)
    raw = ((uint16_t)rxBuf[1] << 8) | rxBuf[0];
    EMS_Data.DigitalSupply = raw * 0.0001f;

    // Reference Voltage 2 (bytes 5:4)
    raw = ((uint16_t)rxBuf[5] << 8) | rxBuf[4];
    EMS_Data.ReferenceVoltage2 = raw * 0.0001f;
}
```

---

## 🔷 STEP 9 — Reading GPIO1 for NTC Temperature (`LTC6811_ReadGPIO1`)

**File:** [ltc6811.c](file:///c:/OSSUS_BIO/STM_32_MC/EMS_FW_v1.0/Drivers/LTC6811/ltc6811.c#L553-L583)

```c
HAL_StatusTypeDef LTC6811_ReadGPIO1(void)
{
    // 1. Trigger GPIO ADC conversion
    LTC6811_StartGPIOADC();     // ADAX command (0x0560)
    HAL_Delay(10);              // Wait for ADC

    // 2. Read Auxiliary Register Group A (GPIO1, GPIO2, GPIO3)
    LTC6811_ReadRegister(0x00, 0x0C, rxBuf);    // RDAUXA

    // 3. GPIO1 is first 16-bit value (bytes 1:0)
    raw = ((uint16_t)rxBuf[1] << 8) | rxBuf[0];
    EMS_Data.RawGPIO[0]   = raw;
    EMS_Data.GPIO1Voltage = raw * 0.0001f;      // LSB = 100µV
}
```

GPIO1 on the DC2260 is connected to an NTC thermistor. After reading `GPIO1Voltage`, the temperature calculation happens in `ems_temperature.c` using the **Steinhart-Hart / Beta equation**.

---

## 🔷 STEP 10 — Writing Config (`LTC6811_WriteConfig`)

**File:** [ltc6811.c](file:///c:/OSSUS_BIO/STM_32_MC/EMS_FW_v1.0/Drivers/LTC6811/ltc6811.c#L214-L284)

The WRCFG command sends a **4-byte command + 8-byte config frame** in a single CS-LOW window:

```
SPI frame (CS held LOW for entire duration):
┌────────────────────────┬──────────────────────────────────────────────────┐
│  WRCFG Command (4B)    │  Config Register Data (8B)                       │
│  0x00 0x01 PEC_H PEC_L │  CFGR0 CFGR1 CFGR2 CFGR3 CFGR4 CFGR5 PEC_H PEC_L │
└────────────────────────┴──────────────────────────────────────────────────┘
```

**Config register bit fields:**
```
CFGR0 [7:0]:  GPIO5-1 pull-down | REFON | SWTRD | ADCOPT
              0x04 = only REFON=1

CFGR1 [7:0]:  0x00 (no GPIO pull-down, no DTEN)

CFGR2 [7:0]:  VUV[7:0]   = lower 8 bits of under-voltage threshold
CFGR3 [3:0]:  VUV[11:8]  = upper 4 bits of under-voltage threshold
CFGR3 [7:4]:  VOV[3:0]   = lower 4 bits of over-voltage threshold
CFGR4 [7:0]:  VOV[11:4]  = upper 8 bits of over-voltage threshold

CFGR5 [7:0]:  DCC8-DCC1  = discharge cell switches (balancing)
```

**VUV/VOV encoding:**
```c
// VUV = 437 → 0.7V  (437 × 0.0016V = 0.6992V ≈ 0.7V)
tx_data[2] = vuv & 0xFF;                        // VUV[7:0]
tx_data[3] = ((vuv >> 8) & 0x0F) |             // VUV[11:8] in lower nibble
             ((vov & 0x0F) << 4);              // VOV[3:0] in upper nibble
tx_data[4] = vov >> 4;                          // VOV[11:4]
```

---

## 🔷 Complete Sequence — End-to-End Cell Voltage Read

```
MCU                                         LTC6811
 │                                             │
 │── LTC6811_Init ──────────────────────────▶  │ (write config REFON=1, VUV, VOV)
 │                                             │
 │── LTC6811_ReadAllCells ─────────────────▶   │
 │   │                                         │
 │   │── LTC6811_StartCellADC_SC() ──────────▶ │ ADCVSC (0x0467) → start ADC
 │   │                                         │ [IC doing ADC conversion...]
 │   │── HAL_Delay(10ms) ─────────────────      │
 │   │                                         │ [ADC done, results in regs]
 │   │                                         │
 │   │── LTC6811_ReadRegister(RDCVA) ────────▶ │
 │   │     CS LOW                              │
 │   │     TX: 0x00 0x04 PEC_H PEC_L ────────▶ │
 │   │     RX: ◀── 8 bytes (6 data + 2 PEC)    │
 │   │     CS HIGH                             │
 │   │── LTC6811_DecodeCellGroup(rxBuf, 0) ─── │ (decode cells 1,2,3 → CellVoltage[0..2])
 │   │                                         │
 │   │── LTC6811_ReadRegister(RDCVB) ────────▶ │
 │   │── LTC6811_DecodeCellGroup(rxBuf, 3) ─── │ (cells 4,5,6)
 │   │                                         │
 │   │── LTC6811_ReadRegister(RDCVC) ────────▶ │
 │   │── LTC6811_DecodeCellGroup(rxBuf, 6) ─── │ (cells 7,8,9)
 │   │                                         │
 │   │── LTC6811_ReadRegister(RDCVD) ────────▶ │
 │   │── LTC6811_DecodeCellGroup(rxBuf, 9) ─── │ (cells 10,11,12)
 │   │                                         │
 │   └── return HAL_OK                         │
 │                                             │
 │── EMS_Data.CellVoltage[0..11] now ready    │
```

---

## 🔷 Data Structures (`EMS_Data`)

**File:** [ems_data.h](file:///c:/OSSUS_BIO/STM_32_MC/EMS_FW_v1.0/Core/Inc/ems_data.h)

All read data lands here:

| Field | Type | What it stores |
|---|---|---|
| `CellVoltage[12]` | `float` | Final calibrated cell voltages in Volts |
| `RawCell[12]` | `uint16_t` | Raw 16-bit ADC values from IC |
| `CellGain[12]` | `float` | Per-cell calibration gain (default 1.0) |
| `CellOffset[12]` | `float` | Per-cell calibration offset (default 0.0V) |
| `StackVoltage` | `float` | Sum of all cells in Volts |
| `InternalTemp` | `float` | LTC6811 die temperature in °C |
| `AnalogSupply` | `float` | VREG analog supply in Volts |
| `DigitalSupply` | `float` | VD digital supply in Volts |
| `ReferenceVoltage2` | `float` | REF2 voltage in Volts |
| `GPIO1Voltage` | `float` | GPIO1 input voltage (for NTC) |
| `RawGPIO[5]` | `uint16_t` | Raw GPIO ADC values |
| `PECErrorCount` | `uint32_t` | SPI integrity error count |

---

## 🔷 PEC15 — Error Detection Deep Dive

Every SPI frame (both commands and data) includes a 2-byte PEC:

```
Command frame → PEC over 2 command bytes
Data frame    → PEC over 6 data bytes
```

The IC calculates PEC independently and compares. If mismatch → ignored/flagged.

**Algorithm ([crc15.c](file:///c:/OSSUS_BIO/STM_32_MC/EMS_FW_v1.0/EMS/crc15.c)):**
```
Initial remainder = 16 (0x0010)
Polynomial = 0x4599
For each byte:
    remainder XOR= (byte << 7)
    For each of 8 bits:
        if bit 14 is set: remainder = (remainder << 1) XOR 0x4599
        else:             remainder = (remainder << 1)
Return: remainder << 1   ← bit 0 is always 0 (LSB forced 0)
```

This is a **CRC-15** producing a 15-bit checksum in bits [15:1] of the 16-bit result.

---

## 🔷 SPI Configuration Summary

| Parameter | Value |
|---|---|
| SPI Instance | `hspi1` |
| Mode | Master, Full-Duplex |
| Clock Polarity | CPOL=1, CPHA=1 (SPI Mode 3) |
| CS Control | Manual (software GPIO) |
| Timeout | 100ms per transfer |
| Wakeup CS pulse | 300µs LOW |

---

## 🔷 Summary — Key Points

| What | How |
|---|---|
| **Wake IC** | CS LOW 300µs, then HIGH 10µs |
| **Send any command** | 2 cmd bytes + 2 PEC bytes = 4 bytes TX |
| **Read any register** | 4 bytes TX (cmd+PEC) then 8 bytes RX (6 data + 2 PEC) |
| **Cell voltage LSB** | 100µV (×0.0001 to get Volts) |
| **GPIO voltage LSB** | 100µV (×0.0001 to get Volts) |
| **Stack voltage LSB** | 2mV (×0.002 to get Volts) |
| **Temp formula** | `(raw × 0.01333) - 273.0` |
| **12 cells = 4 reads** | RDCVA, RDCVB, RDCVC, RDCVD (3 cells each) |
| **Calibration** | `V_calibrated = (V_raw × Gain) + Offset` |
