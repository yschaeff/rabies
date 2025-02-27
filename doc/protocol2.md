# Hardware layer

On the wire we have 4 different characters:

data characters:
- 0
- 1

control characters:
- DATA READY FLAG
- RESET

## DATA READY FLAG (DRF)

The data ready flag indicates that this RABI has something to share and likes
to be polled ASAP.

A RABI may send the data ready flag when it has NOT seen a poll since the last
time it send the flag AND it is currently in its resting state. A trigger to do
so might be one of:

1. an edge was detected on its switch
2. a data ready flag was offered from its upstream neighbour.

This way when multiple keys are pressed at the same time the DRF will only be
seen once by the Akela. Regardless how many RABIes send it or how many times
they saw an edge.

## RESET

A RESET may be send any time by a RABI. Whenever it encounters an error of
sorts it emits a reset and should go to the reset state. It must remain
in that state until it received a reset from upstream. Any other characters MUST
be ignored (and are no reason to send another reset).

If a RABI receives a reset it must forward it and return to its resting state.
Unless it is currently in the reset state, in which case it will silently
return to its resting state.

# Protocol layer

There are 2 types of messages: short message and an extended message. A short
message is as compact as possible thus there is only 1 such message: a poll.
Extended messages have properties to indicate how a RABI should respond.

## Data Frame

Whenever a part of the Akela's message is intended for one RABI individually
the data is split in to frames. In the same fashion, if a RABI responds to the
Akela with data it does so by encapsulating it in a frame. Frames are essential
for the RABIes, they provide structure and indicate when it is allowed append
their own data at the end of a transmission.

A dataframe consists of a 0 (frame marker) followed by a number of bits. The
length of the dataframe is context dependent (and likely subject to change).

Example:

- 01001

This dataframe is said to be length 4, we do not count the frame marker. Its
payload is 0x9.

## End of Transmission Marker (EOT)

The end of transmission marker is the counterpart of the frame marker. It is
represented by a single 1. A RABI must NOT send data after EOT. Only RESET or
DRF is permitted. Failure to do so is considered an error condition.

# Transmission

A transmission consists of the following components:

1. Type of message (i.e. poll or extended)
2. Optional header (optinal, only for extended)
3. Input. Either: (optional)
    - data
    - series of data frames
4. Output. Series of data frames (optional)
5. EOT

## Poll message

A poll message requests the RABIes to report their state. It is indicated by
type 1. the frame length is 1 for now. Good for switches.

For example, messages send by:

Akela:   11
R1   :   10x1
R2   :   10x0y1
R3   :   10x0y0z1


## Extended messages

An extended message is more complicated. It is indicated by type 0 and has 3
header bits.

- has input data (I)
- expects output (O)
- bulk (B)

         IOB
Akela:  0000  ## call (command)
Akela:  0001  ## call all (bulk command)
Akela:  0010  ## query (read)
Akela:  0011  ## query all (bulk read)
Akela:  0100  ## method (write)
Akela:  0101  ## method all (bulk write)
Akela:  0110  ## function (compute)
Akela:  0111  ## function (bulk compute)

### Call

A call only takes an address/opcode but no input data. This can be used to
implement a command. Examples: generate private key, initiate self destruct.

Akela tx: 0 000 0AAAA 0BBBB 0CCCC 1
Akela rx: 0 000 1

Tell RABI1 to do AAAA, RABI2 to do BBBB, RABI3 to do CCCC
Each RABI will consume the first frame.

### Call bulk

Same, but now each RABI processes the same opcode:

Akela tx: 0 001 AAAA 1
Akela rx: 0 001 AAAA 1

Tell RABI1 to do AAAA, RABI2 to do AAAA, RABI3 to do AAAA

### Query

A query takes an address/opcode and expects the RABIes to respond with data.
This can be used to implement a read. Examples: What is in memory location X?
What is your temperature?

Akela tx: 0 010 0AAAA 0BBBB 0CCCC 1
Akela rx: 0 010 0aaaa 0bbbb 0cccc 1

Ask RABI1 contents of AAAA, RABI2 contents of BBBB, RABI3 contents of CCCC
Each RABI will consume the first frame. At the end it will insert its response.

### Query bulk

Same, but now each RABI processes the same opcode:

Akela tx: 0 011 AAAA 1
Akela rx: 0 011 AAAA 0aaaa 0bbbb 0cccc 1

Ask RABI1 contents of AAAA, RABI2 contents of AAAA, RABI3 contents of AAAA

### Method

A method takes an address/opcode plus data and expects the RABIes to respond
without data. This can be used to implement a write. Examples: Put x in memory
at location Y. Increment some counter

Akela tx: 0 100 0AAAAxxxx 0BBBByyyy 0CCCCzzzz 1
Akela rx: 0 100 1

Ask RABI1 to write xxxx to AAAA, RABI2 to write yyyy to BBBB, RABI3 write zzzz
to CCCC Each RABI will consume the first frame.

### Method bulk

Same, but now each RABI processes the same opcode:

Akela tx: 0 101 AAAAxxxx 1
Akela rx: 0 101 1

Ask RABI1 to write xxxx to AAAA, RABI2 to write xxxx to AAAA, RABI3 write xxxx
to AAAA

### Function

A function takes an address/opcode plus data and expects the RABIes to respond
with data. This can be used to implement a compute. Examples:
challenge/response, what is the sqrt(x)?

Akela tx: 0 110 0AAAAxxxx 0BBBByyyy 0CCCCzzzz 1
Akela rx: 0 110 0aaaa 0bbbb 0cccc 1

Ask RABI1 to compute AAAA(xxxx) and return aaaa, Ask RABI2 to compute
BBBB(yyyy) and return bbbb, Ask RABI3 to compute CCCC(zzzz) and return cccc.
Each RABI will consume the first frame.

### Method bulk

Same, but now each RABI processes the same opcode:

Akela tx: 0 111 AAAAxxxx 1
Akela rx: 0 110 0aaaa 0bbbb 0cccc 1

Ask RABI1 to compute AAAA(xxxx) and return aaaa, Ask RABI2 to compute
AAAA(xxxx) and return bbbb, Ask RABI3 to compute AAAA(xxxx) and return cccc.
