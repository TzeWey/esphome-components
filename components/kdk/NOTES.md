# KDK WiFi Module

DNSK-P11

# WARNING

These information was captured with a logic analyzer and a laptop without the
charger connected. Care was taken to avoid unintended ground loops between the
DUT and the computer.

# Observed Behavior

- Baud rate of 9600, 8-bits, 1 stop bit, even parity
- FAN sends MOD a ping every 600s
- FAN _DOES_ notify MOD when major states has changes, e.g. Fan turns ON/OFF, Light turns ON/OFF
- FAN _DOES NOT_ push minor updates to the APP, e.g. fan speed change, light brightness change
- APP _MUST_ poll status from FAN/MOD

# Packet Format

```
┌────────┬─────────┬─────────┬─────────┬─────────┬──────────┐
│ START  │ TX/RX   │ COMMAND │ LENGTH  │ PAYLOAD │ CHECKSUM │
│        │ COUNTER │         │         │         │          │
├────────┼─────────┼─────────┼─────────┼─────────┼──────────┤
│ 1 Byte │ 1 Byte  │ 2 Bytes │ 2 Bytes │ LENGTH  │ 1 Byte   │
│        │         │         │         │ Bytes   │          │
└────────┴─────────┴─────────┴─────────┴─────────┴──────────┘
```

## CHECKSUM

(SUM of _ALL_ bytes (including CHECKSUM) & 0xFF) MUST be equal to 0

## TX/RX COUNTER

- Each device (FAN/MOD) maintains a separate TX and RX counter to check for packet loss.

## NOTES

- Payload length is not part of the frame

## 0x66 packet

Seems to be a special packet, checksum does not include 0x66 and does not begin with 0x5A.
Appears to only be sent when FAN is powered on.
Re-sync packet counters when 0x66 is received.

# Captures / Sequence

## Init Sequence

### SYNC

```
FAN > MOD : 66 08 00 01 01 F6
FAN < MOD : 5A 00 00 06 00 00 A0
```

### CMD 0C00

```
MOD > FAN : 5A 01 00 0C 00 00 99
MOD < FAN : 5A 01 00 8C 00 01 00 18
```

### CMD 1000

```
MOD > FAN : 5A 02 00 10 00 01 20 73
MOD < FAN : 5A 02 00 90 00 02 00 20 F2
```

### CMD 1100 - Model Info

```
MOD > FAN : 5A 03 00 11 00 02 00 01 8F
MOD < FAN : 5A 03 00 91 00 29
            00
            00 01 00 01 0A 16
            4B 31 32 55 43 2B 56 42 48 48 2D 47 59 32 34 32 32 30 30 31 32 36
            0B 02 01 2C 0C 06
            46 4D 31 32 47 43
            D2
```

### CMD 1200

```
MOD > FAN : 5A 04 00 12 00 06 01 10 11 12 13 14 2F
MOD < FAN : 5A 04 00 92 00 04 00 01 10 11 EA
```

### CMD 4100

```
MOD > FAN : 5A 05 00 41 00 00 60
MOD < FAN : 5A 05 00 C1 00 02 00 03 DB
```

### CMD 4C01

```
MOD > FAN : 5A 06 01 4C 00 00 53
MOD < FAN : 5A 06 01 CC 00 02 00 01 D0
```

### CMD 0010

```
MOD > FAN : 5A 07 10 00 00 00 8F
MOD < FAN : 5A 07 10 80 00 05 00 01 01 3A 01 CD
```

### CMD 0110 - Seems to return a list of supported attributes?

```
MOD > FAN : 5A 08 10 01 00 05
            01 3A 01 00 01
            4B
MOD < FAN : 5A 08 10 81 00 8A
            00 01 3A 01 00 01
            00 01
            00 20 <= number of attributes?
            00 80 E2 01
            00 81 E2 01
            00 82 40 04
            00 86 62 2E
            00 88 62 01
            00 8A 40 03
            00 8C 42 0C
            00 93 C2 01
            00 9D 40 06
            00 9E 40 11
            00 9F 40 11
            00 F0 C2 01
            00 F1 C2 01
            00 F2 C2 01
            00 F3 E2 01
            00 F4 C2 01
            00 F5 C2 01
            00 F6 C2 01
            00 F7 C2 01
            00 F8 C2 04
            00 F9 42 02
            00 FA C2 04
            00 FB 42 02
            00 FC C2 01
            00 FD C2 01
            00 FE C2 01
            01 F0 42 18
            01 F1 42 03
            01 F2 42 03
            01 F3 42 03
            01 F4 42 03
            01 F5 42 03
            62
```

#### ENTRY:

- Byte 1-2 : Attribute ID
- Byte 3 : Attribute Type?
- Byte 4 : Attribute Data Length

### CMD 0210

```
MOD > FAN : 5A 09 10 02 00 13
            01 3A 01
            05
            00 82 00
            00 8A 00
            00 9D 00
            00 9E 00
            00 9F 00
            51
MOD < FAN : 5A 09 10 82 00 43
            00 01 3A 01
            05
            00 82 04 00 00 4C 00
            00 8A 03 00 00 FE
            00 9D 06 05 80 81 86 88 F3
            00 9E 11 10 81 81 80 82 80 80 80 80 80 00 80 00 80 80 80 00
            00 9F 11 1A 81 81 81 82 80 80 81 80 81 80 81 80 81 82 82 02
            E4
```

### CMD 1800

```
MOD > FAN : 5A 0A 00 18 00 00 84
MOD < FAN : 5A 0A 00 98 00 01 00 03
```

### CMD 0001 - Payload 0x10 - Before module is ready?

```
MOD > FAN : 5A 0B 01 00 00 01 10 89
MOD < FAN : 5A 0B 01 80 00 01 00 19
```

### CMD 0101 - FAN polls MOD (seems to be the MOD status)

```
FAN > MOD : 5A 00 01 01 00 00 A4
FAN < MOD : 5A 00 01 81 00 03 00 10 00 11
```

### CMD 0001 - Payload 0x11 - After module is ready?

```
MOD > FAN : 5A 0C 01 00 00 01 11 87
MOD < FAN : 5A 0C 01 80 00 01 00 18
```

### CMD 0101 - FAN polls MOD until 0x13 is returned

```
FAN > MOD : 5A 01 01 01 00 00 A3
FAN < MOD : 5A 01 01 81 00 03 00 11 12 FD
```

### CMD 0101 - FAN polls MOD until 0x13 is returned

```
FAN > MOD : 5A 05 01 01 00 00 9F
FAN < MOD : 5A 05 01 81 00 03 00 11 13 F8
```

## Idle Ping

### CMD 0101

```
FAN > MOD : 5A 08 01 01 00 00 9C
FAN < MOD : 5A 08 01 81 00 03 00 11 13 F5
```

## APP Poll

### CMD 0910

```
MOD > FAN : 5A 20 10 09 00 32
            02 01 3A 01
            0F
            00 80 00
            00 F0 00
            00 86 00
            00 88 00
            00 F8 00
            00 F2 00
            00 F1 00
            00 F9 00
            00 FA 00
            00 FB 00
            00 F3 00
            00 F5 00
            00 F4 00
            00 F7 00
            00 F6 00
            DE
MOD < FAN : 5A 20 10 89 00 76
            00 01 3A 01
            0F
            00 80 01 31
            00 F0 01 32
            00 86 2E 2A 00 00 FE 01 00 00 00 00 00 00 00 00 00 00 00
                     00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
                     00 00 00 00 00 00 00 00 00 00 00 00 00 00
            00 88 01 42
            00 F8 04 31 31 00 00
            00 F2 01 31
            00 F1 01 41
            00 F9 02 00 00
            00 FA 04 31 40 00 00
            00 FB 02 00 00
            00 F3 01 31
            00 F5 01 01
            00 F4 01 42
            00 F7 01 01
            00 F6 01 58
            F8
```

## Fan ON/OFF

### CMD 0810 - FAN ON : SPEED 2? - 00 F0 = 32

```
MOD > FAN : 5A 21 10 08 00 2C
            02 01 3A 01
            09
            00 FD 01 03
            00 FC 01 30
            00 FE 01 40
            00 80 01 30
            00 F0 01 32
            00 F2 01 31
            00 F1 01 41
            00 F3 01 31
            00 F8 04 31 31 FF FF
            E1
MOD < FAN : 5A 21 10 88 00 20
            00 01 3A 01
            09
            00 80 00
            00 F8 00
            00 F0 00
            00 F1 00
            00 F2 00
            00 F3 00
            00 FC 00
            00 FD 00
            00 FE 00
            53
```

### CMD 0910 - Poll after FAN ON

```
MOD > FAN : 5A 22 10 09 00 32
            02 01 3A 01
            0F
            00 80 00
            00 F0 00
            00 86 00
            00 88 00
            00 F8 00
            00 F2 00
            00 F1 00
            00 F9 00
            00 FA 00
            00 FB 00
            00 F3 00
            00 F5 00
            00 F4 00
            00 F7 00
            00 F6 00
            DC
MOD < FAN : 5A 22 10 89 00 76
            00 01 3A 01
            0F
            00 80 01 30
            00 F0 01 32
            00 86 2E 2A 00 00 FE 01 00 00 00 00 00 00 00 00 00 00 00
                     00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
                     00 00 00 00 00 00 00 00 00 00 00 00 00 00
            00 88 01 42
            00 F8 04 31 31 00 00
            00 F2 01 31
            00 F1 01 41
            00 F9 02 00 00
            00 FA 04 31 40 00 00
            00 FB 02 00 00
            00 F3 01 31
            00 F5 01 01
            00 F4 01 42
            00 F7 01 01
            00 F6 01 58
            F7
```

### CMD 0A10 - FAN updates MOD

```
FAN > MOD : 5A 22 10 0A 00 09
            00 01 3A 01
            01
            00 80 01 30
            73
FAN < MOD : 5A 22 10 8A 00 04
            00 01 3A 01
            AA
```

### CMD 0910 - APP crash CONNECT poll

```
MOD > FAN : 5A 23 10 09 00 32
            02 01 3A 01
            0F
            00 80 00
            00 F0 00
            00 86 00
            00 88 00
            00 F8 00
            00 F2 00
            00 F1 00
            00 F9 00
            00 FA 00
            00 FB 00
            00 F3 00
            00 F5 00
            00 F4 00
            00 F7 00
            00 F6 00
            DB
MOD < FAN : 5A 23 10 89 00 76
            00 01 3A 01
            0F
            00 80 01 30
            00 F0 01 32
            00 86 2E 2A 00 00 FE 01 00 00 00 00 00 00 00 00 00 00 00
                     00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
                     00 00 00 00 00 00 00 00 00 00 00 00 00 00
            00 88 01 42
            00 F8 04 31 31 00 00
            00 F2 01 31
            00 F1 01 41
            00 F9 02 00 00
            00 FA 04 31 40 00 00
            00 FB 02 00 00
            00 F3 01 31
            00 F5 01 01
            00 F4 01 42
            00 F7 01 01
            00 F6 01 58
            F6
```

### CMD 0810 - FAN SPEED 3 - 00 F0 = 33

```
MOD > FAN : 5A 26 10 08 00 2C
            02 01 3A 01
            09
            00 FD 01 03
            00 FC 01 30
            00 FE 01 40
            00 80 01 30
            00 F0 01 33
            00 F2 01 31
            00 F1 01 41
            00 F3 01 31
            00 F8 04 FF 31 FF FF
            0D
MOD < FAN : 5A 26 10 88 00 20
            00 01 3A 01
            09
            00 80 00
            00 F8 00
            00 F0 00
            00 F1 00
            00 F2 00
            00 F3 00
            00 FC 00
            00 FD 00
            00 FE 00
            4E
```

### CMD 0810 - FAN SPEED 4 - 00 F0 = 34

```
MOD > FAN : 5A 28 10 08 00 2C
            02 01 3A 01
            09
            00 FD 01 03
            00 FC 01 30
            00 FE 01 40
            00 80 01 30
            00 F0 01 34
            00 F2 01 31
            00 F1 01 41
            00 F3 01 31
            00 F8 04 FF 31 FF FF
            0A
MOD < FAN : 5A 28 10 88 00 20
            00 01 3A 01
            09
            00 80 00
            00 F8 00
            00 F0 00
            00 F1 00
            00 F2 00
            00 F3 00
            00 FC 00
            00 FD 00
            00 FE 00
            4C
```

### CMD 0810 - FAN SPEED 10 - 00 F0 = 3A

```
MOD > FAN : 5A 34 10 08 00 2C
            02 01 3A 01
            09
            00 FD 01 03
            00 FC 01 30
            00 FE 01 40
            00 80 01 30
            00 F0 01 3A
            00 F2 01 31
            00 F1 01 41
            00 F3 01 31
            00 F8 04 FF 31 FF FF
            F8
MOD < FAN : 5A 34 10 88 00 20
            00 01 3A 01
            09
            00 80 00
            00 F8 00
            00 F0 00
            00 F1 00
            00 F2 00
            00 F3 00
            00 FC 00
            00 FD 00
            00 FE 00
            40
```

### CMD 0A10 - IR - Light ON

```
FAN > MOD : 5A 0C 10 0A 00 09
            00 01 3A 01
            01
            00 F3 01 30
            16
FAN < MOD : 5A 0C 10 8A 00 04
            00 01 3A 01
            C0
```

### CMD 0A10 - IR - Light OFF

```
FAN > MOD : 5A 0D 10 0A 00 09
            00 01 3A 01
            01
            00 F3 01 31
            14
FAN < MOD : 5A 0D 10 8A 00 04
            00 01 3A 01
            BF
```

### CMD 0810 - APP - Fan ON - 00 80 = 30

```
MOD > FAN : 5A 3C 10 08 00 2C
            02 01 3A 01
            09
            00 FD 01 03
            00 FC 01 30
            00 FE 01 40
            00 80 01 30
            00 F0 01 31
            00 F2 01 31
            00 F1 01 41
            00 F3 01 31
            00 F8 04 31 31 FF FF
            C7
MOD < FAN : 5A 3C 10 88 00 20
            00 01 3A 01
            09
            00 80 00
            00 F8 00
            00 F0 00
            00 F1 00
            00 F2 00
            00 F3 00
            00 FC 00
            00 FD 00
            00 FE 00
            38
```

### CMD 0A10 - After CMD above

```
FAN > MOD : 5A 24 10 0A 00 09
            00 01 3A 01
            01
            00 80 01 30
            71
FAN < MOD : 5A 24 10 8A 00 04
            00 01 3A 01
            A8
```

### CMD 0810 - APP - Fan Reverse Enable - 00 F1 = 42

```
MOD > FAN : 5A 3E 10 08 00 2C
            02 01 3A 01
            09
            00 FD 01 03
            00 FC 01 30
            00 FE 01 40
            00 80 01 30
            00 F0 01 31
            00 F2 01 31
            00 F1 01 42
            00 F3 01 31
            00 F8 04 FF 31 FF FF
            F6
MOD < FAN : 5A 3E 10 88 00 20
            00 01 3A 01
            09
            00 80 00
            00 F8 00
            00 F0 00
            00 F1 00
            00 F2 00
            00 F3 00
            00 FC 00
            00 FD 00
            00 FE 00
            36
```

### CMD 0810 - APP - Fan Reverse Enable (another capture) - 00 F1 = 42

```
MOD > FAN : 5A 14 10 08 00 30
            01 01 3A 01
            0A
            00 93 01 42
            00 FD 01 04
            00 FC 01 30
            00 FE 01 40
            00 80 01 30
            00 F0 01 35
            00 F2 01 31
            00 F1 01 42
            00 F3 01 31
            00 F8 04 FF 31 FF FF
            41
MOD < FAN : 5A 14 10 88 00 23
            00 01 3A 01
            0A
            00 93 00
            00 80 00
            00 F8 00
            00 F0 00
            00 F1 00
            00 F2 00
            00 F3 00
            00 FC 00
            00 FD 00
            00 FE 00
            C9
```

### CMD 0810 - APP - Fan Reverse Disable - 00 F1 = 41

```
MOD > FAN : 5A 40 10 08 00 2C
            02 01 3A 01
            09
            00 FD 01 03
            00 FC 01 30
            00 FE 01 40
            00 80 01 30
            00 F0 01 31
            00 F2 01 31
            00 F1 01 41
            00 F3 01 31
            00 F8 04 FF 31 FF FF
            F5
MOD < FAN : 5A 40 10 88 00 20
            00 01 3A 01
            09
            00 80 00
            00 F8 00
            00 F0 00
            00 F1 00
            00 F2 00
            00 F3 00
            00 FC 00
            00 FD 00
            00 FE 00
            34
```

### CMD 0810 - APP - Fan OFF - 00 80 = 31

```
MOD > FAN : 5A 42 10 08 00 1C
            02 01 3A 01
            05
            00 FD 01 03
            00 FC 01 30
            00 80 01 31
            00 F3 01 31
            00 FA 04 31 FF FF FF
            BC
MOD < FAN : 5A 42 10 88 00 14
            00 01 3A 01
            05
            00 80 00
            00 FA 00
            00 F3 00
            00 FC 00
            00 FD 00
            11
```

### CMD 0810 - APP - Light ON - 00 F3 = 0x30

```
MOD > FAN : 5A 4E 10 08 00 2C
            02 01 3A 01
            09
            00 FD 01 03
            00 FC 01 30
            00 FE 01 40
            00 80 01 31
            00 F3 01 30
            00 F6 01 58
            00 F4 01 42
            00 F5 01 01
            00 FA 04 31 40 FF FF
            A0
MOD < FAN : 5A 4E 10 88 00 20
            00 01 3A 01
            09
            00 80 00
            00 FA 00
            00 F3 00
            00 F4 00
            00 F5 00
            00 F6 00
            00 FC 00
            00 FD 00
            00 FE 00
            18
```

### CMD 0810 - APP - Light OFF - 00 F3 = 0x31

```
MOD > FAN : 5A 50 10 08 00 1C
            02 01 3A 01
            05
            00 FD 01 03
            00 FC 01 30
            00 80 01 31
            00 F3 01 31
            00 FA 04 31 40 FF FF
            6D
MOD < FAN : 5A 50 10 88 00 14
            00 01 3A 01
            05
            00 80 00
            00 FA 00
            00 F3 00
            00 FC 00
            00 FD 00
            03
```

### CMD 0810 - APP - Light BRIGHTNESS ~15 - 00 F5 = 11

```
MOD > FAN : 5A 54 10 08 00 2C
            02 01 3A 01
            09
            00 FD 01 03
            00 FC 01 30
            00 FE 01 40
            00 80 01 31
            00 F3 01 30
            00 F6 01 58
            00 F4 01 42
            00 F5 01 11
            00 FA 04 31 40 FF FF
            8A
MOD < FAN : 5A 54 10 88 00 20
            00 01 3A 01
            09
            00 80 00
            00 FA 00
            00 F3 00
            00 F4 00
            00 F5 00
            00 F6 00
            00 FC 00
            00 FD 00
            00 FE 00
            12
```

### CMD 0810 - APP - Light BRIGHTNESS ~50 - 00 F5 = 33

```
MOD > FAN : 5A 56 10 08 00 2C
            02 01 3A 01
            09
            00 FD 01 03
            00 FC 01 30
            00 FE 01 40
            00 80 01 31
            00 F3 01 30
            00 F6 01 58
            00 F4 01 42
            00 F5 01 33
            00 FA 04 31 40 FF FF
            66
MOD < FAN : 5A 56 10 88 00 20
            00 01 3A 01
            09
            00 80 00
            00 FA 00
            00 F3 00
            00 F4 00
            00 F5 00
            00 F6 00
            00 FC 00
            00 FD 00
            00 FE 00
            10
```

### CMD 0810 - APP - Light BRIGHTNESS 100 - 00 F5 = 64

```
MOD > FAN : 5A 58 10 08 00 2C
            02 01 3A 01
            09
            00 FD 01 03
            00 FC 01 30
            00 FE 01 40
            00 80 01 31
            00 F3 01 30
            00 F6 01 58
            00 F4 01 42
            00 F5 01 64
            00 FA 04 31 40 FF FF
            33
MOD < FAN : 5A 58 10 88 00 20
            00 01 3A 01
            09
            00 80 00
            00 FA 00
            00 F3 00
            00 F4 00
            00 F5 00
            00 F6 00
            00 FC 00
            00 FD 00
            00 FE 00
            0E
```

### CMD 0810 - APP - Light COLOR 100 - 00 F6 = 64

```
MOD > FAN : 5A 5A 10 08 00 2C
            02 01 3A 01
            09
            00 FD 01 03
            00 FC 01 30
            00 FE 01 40
            00 80 01 31
            00 F3 01 30
            00 F6 01 64
            00 F4 01 42
            00 F5 01 64
            00 FA 04 31 40 FF FF
            25
MOD < FAN : 5A 5A 10 88 00 20
            00 01 3A 01
            09
            00 80 00
            00 FA 00
            00 F3 00
            00 F4 00
            00 F5 00
            00 F6 00
            00 FC 00
            00 FD 00
            00 FE 00
            0C
```

### CMD 0810 - APP - Light COLOR ~15 - 00 F6 = 0E

```
MOD > FAN : 5A 5E 10 08 00 2C
            02 01 3A 01
            09
            00 FD 01 03
            00 FC 01 30
            00 FE 01 40
            00 80 01 31
            00 F3 01 30
            00 F6 01 0E
            00 F4 01 42
            00 F5 01 64
            00 FA 04 31 40 FF FF
            77
MOD < FAN : 5A 5E 10 88 00 20
            00 01 3A 01
            09
            00 80 00
            00 FA 00
            00 F3 00
            00 F4 00
            00 F5 00
            00 F6 00
            00 FC 00
            00 FD 00
            00 FE 00
            08
```

### CMD 0810 - APP - NightLight ON - 00 F4 = 43, 00 F7 = 01

```
MOD > FAN : 5A 6B 10 08 00 28
            02 01 3A 01
            08
            00 FD 01 03
            00 FC 01 30
            00 FE 01 40
            00 80 01 31
            00 F3 01 30
            00 F4 01 43
            00 F7 01 01
            00 FA 04 31 40 FF FF
            D4
MOD < FAN : 5A 6B 10 88 00 1D
            00 01 3A 01
            08
            00 80 00
            00 FA 00
            00 F3 00
            00 F4 00
            00 F7 00
            00 FC 00
            00 FD 00
            00 FE 00
            F3
```

### CMD 0810 - APP - NightLight 2 - 00 F7 = 32

```
MOD > FAN : 5A 6E 10 08 00 28
            02 01 3A 01
            08
            00 FD 01 03
            00 FC 01 30
            00 FE 01 40
            00 80 01 31
            00 F3 01 30
            00 F4 01 43
            00 F7 01 32
            00 FA 04 31 40 FF FF
            A0
MOD < FAN : 5A 6E 10 88 00 1D
            00 01 3A 01
            08
            00 80 00
            00 FA 00
            00 F3 00
            00 F4 00
            00 F7 00
            00 FC 00
            00 FD 00
            00 FE 00
            F0
```

### CMD 0810 - APP - NightLight 3 - 00 F7 = 64

```
MOD > FAN : 5A 70 10 08 00 28
            02 01 3A 01
            08
            00 FD 01 03
            00 FC 01 30
            00 FE 01 40
            00 80 01 31
            00 F3 01 30
            00 F4 01 43
            00 F7 01 64
            00 FA 04 31 40 FF FF
            6C
MOD < FAN : 5A 70 10 88 00 1D
            00 01 3A 01
            08
            00 80 00
            00 FA 00
            00 F3 00
            00 F4 00
            00 F7 00
            00 FC 00
            00 FD 00
            00 FE 00
            EE
```

### CMD 0810 - APP - NightLight OFF

```
MOD > FAN : 5A 72 10 08 00 1C
            02 01 3A 01
            05
            00 FD 01 03
            00 FC 01 30
            00 80 01 31
            00 F3 01 31
            00 FA 04 31 40 FF FF
            4B
MOD < FAN : 5A 72 10 88 00 14
            00 01 3A 01
            05
            00 80 00
            00 FA 00
            00 F3 00
            00 FC 00
            00 FD 00
            E1
```

### CMD 0810 - APP - Fan Yuragi ON - 00 F2 = 30

```
MOD > FAN : 5A 1A 10 08 00 2C
            02 01 3A 01
            09
            00 FD 01 03
            00 FC 01 30
            00 FE 01 40
            00 80 01 30
            00 F0 01 35
            00 F2 01 30
            00 F1 01 41
            00 F3 01 31
            00 F8 04 FF 31 FF FF
            18
MOD < FAN : 5A 1A 10 88 00 20
            00 01 3A 01
            09
            00 80 00
            00 F8 00
            00 F0 00
            00 F1 00
            00 F2 00
            00 F3 00
            00 FC 00
            00 FD 00
            00 FE 00
            5A
```

### CMD 0810 - APP - Fan Yuragi OFF - 00 F2 = 31

```
MOD > FAN : 5A 1B 10 08 00 2C
            02 01 3A 01
            09
            00 FD 01 03
            00 FC 01 30
            00 FE 01 40
            00 80 01 30
            00 F0 01 35
            00 F2 01 31
            00 F1 01 41
            00 F3 01 31
            00 F8 04 FF 31 FF FF
            16
MOD < FAN : 5A 1B 10 88 00 20
            00 01 3A 01
            09
            00 80 00
            00 F8 00
            00 F0 00
            00 F1 00
            00 F2 00
            00 F3 00
            00 FC 00
            00 FD 00
            00 FE 00
            59
```

# Command

## 0101 - FAN polls MOD every 10 minutes (600s) for module status

MOD returns payload "00 11 13" to keep FAN happy

# Parameters

APP polls the following parameters every time it connects.

```
8000 : Fan ON/OFF
F000 : Fan Speed
8600 : UNKNOWN, large 46-bytes parameter
8800 : UNKNOWN, always 0x42
F800 : Some kind of parameter mask, sent when changing the fan states
F200 : Fan Yuragi
F100 : Fan Direction
F900 : UNKNOWN, always 0x00 0x00
FA00 : Some kind of parameter mask, sent when changing the fan states
FB00 : UNKNOWN, always 0x00 0x00
F300 : Light ON/OFF
F500 : Light Brightness
F400 : Light Mode
F700 : NightLight Brightness
F600 : Light Color
```

## 8000 : Fan ON/OFF

- 0x30 = ON
- 0x31 = OFF

## 9300 : ???

Sent when testing yuragi

- 0x42

## F000 : Fan Speed

- 0x32 = 2
- 0x33 = 3
- 0x34 = 4
- 0x3A = 10

## F100 : Fan Direction

- 0x41 = Normal
- 0x42 = Reverse

## F200 : Fan Yuragi

- 0x30 = ON
- 0x31 = OFF

## F300 : Light ON/OFF

- 0x30 = ON
- 0x31 = OFF

## F400 : Light Mode

- 0x42 = Normal
- 0x43 = NightLight

## F500 : Light Brightness

- 0x11 = 17
- 0x33 = 51
- 0x64 = 100

## F600 : Light Color

- 0x0E = 14
- 0x64 = 100

## F700 : NightLight Brightness

Percent Brightness (might be 3 levels of brightness only)

- 0x01 = 1 [1%]
- 0x32 = 2 [50%]
- 0x64 = 3 [100%]

## F800 : UNKNOWN

Payload is 4 bytes

- 0x31 0x31 0xFF 0xFF : When turning FAN ON
- 0xFF 0x31 0xFF 0xFF : When changing FAN speeds

## FA00 : UNKNOWN

Payload is 4 bytes

- 0x31 0xFF 0xFF 0xFF : When turning FAN OFF
- 0x31 0x40 0xFF 0xFF : When turning Light/NightLight ON, OFF, Color, Brightness

## FC00 : ??? one of FC00, FD00 or FE00 triggers the buzzer, without them, change happens but silently

- 0x30

## FD00 : ???

- 0x3 : Earlier captures
- 0x4 : Later captures

## FE00 : ???

- 0x40

```
[ 0] ID=8000, SIZE=1 , META=E2, DATA=31
[ 1] ID=8100, SIZE=1 , META=E2, DATA=00
[ 2] ID=8200, SIZE=4 , META=40, DATA=00 00 4C 00
[ 3] ID=8600, SIZE=46, META=62, DATA=2A 00 00 FE 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
[ 4] ID=8800, SIZE=1 , META=62, DATA=42
[ 5] ID=8A00, SIZE=3 , META=40, DATA=00 00 FE
[ 6] ID=8C00, SIZE=12, META=42, DATA=4B 31 32 55 43 00 00 00 00 00 00 00
[ 7] ID=9300, SIZE=1 , META=C2, DATA=41
[ 8] ID=9D00, SIZE=6 , META=40, DATA=05 80 81 86 88 F3
[ 9] ID=9E00, SIZE=17, META=40, DATA=10 81 81 80 82 80 80 80 80 80 00 80 00 80 80 80 00
[10] ID=9F00, SIZE=17, META=40, DATA=1A 81 81 81 82 80 80 81 80 81 80 81 80 81 82 82 02
[11] ID=F000, SIZE=1 , META=C2, DATA=31
[12] ID=F100, SIZE=1 , META=C2, DATA=41
[13] ID=F200, SIZE=1 , META=C2, DATA=31
[14] ID=F300, SIZE=1 , META=E2, DATA=31
[15] ID=F400, SIZE=1 , META=C2, DATA=42
[16] ID=F500, SIZE=1 , META=C2, DATA=64
[17] ID=F600, SIZE=1 , META=C2, DATA=64
[18] ID=F700, SIZE=1 , META=C2, DATA=01
[19] ID=F800, SIZE=4 , META=C2, DATA=31 31 00 00
[20] ID=F900, SIZE=2 , META=42, DATA=00 00
[21] ID=FA00, SIZE=4 , META=C2, DATA=31 40 00 00
[22] ID=FB00, SIZE=2 , META=42, DATA=00 00
[23] ID=FC00, SIZE=1 , META=C2, DATA=31
[24] ID=FD00, SIZE=1 , META=C2, DATA=02
[25] ID=FE00, SIZE=1 , META=C2, DATA=40
[26] ID=F001, SIZE=24, META=42, DATA=56 42 48 48 2D 47 59 32 34 32 32 30 30 31 32 36 00 00 00 00 00 00 00 00
[27] ID=F101, SIZE=3 , META=42, DATA=00 01 3F
[28] ID=F201, SIZE=3 , META=42, DATA=00 01 3F
[29] ID=F301, SIZE=3 , META=42, DATA=00 00 00
[30] ID=F401, SIZE=3 , META=42, DATA=00 00 A0
[31] ID=F501, SIZE=3 , META=42, DATA=00 00 00
```
