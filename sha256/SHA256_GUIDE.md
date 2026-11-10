# SHA-256: implementation guide

This guide describes SHA-256, one member of the SHA-2 family, at the level
needed to implement it from scratch. It explains the data flow and the
constants, but intentionally does not provide a ready-to-compile
implementation.

SHA-256 maps an input message of any supported length to a 256-bit (32-byte)
digest. It is a hash function, not encryption: there is no key and no way to
recover the input from the digest. The same input always produces the same
digest.

## The big picture

The algorithm has five phases:

1. Encode the input as bytes.
2. Pad it so its length is a multiple of 512 bits, including the original
   message length in the padding.
3. Split the padded message into 512-bit blocks.
4. For each block, expand its 16 input words to 64 words and use those words
   in 64 rounds of compression.
5. Serialize the final eight 32-bit state words as 32 digest bytes.

The compression state is carried from one block to the next. As a result,
each block affects the final digest, not just the last block.

## Bit and word conventions

SHA-256 operates on **32-bit words**. Every addition of words is modulo
2^32: discard any carry beyond bit 31. Rotations and shifts apply to
32-bit values.

Words are read and written in **big-endian** order: the most significant
byte comes first. For example, bytes `01 23 45 67` form the word
`0x01234567`. This is part of the SHA-256 specification; do not use the
machine's native byte order.

The following notation is used below:

- `ROTR^n(x)`: rotate the 32-bit word `x` right by `n` bits. Bits shifted
  off the right end wrap around to the left.
- `SHR^n(x)`: shift right by `n` bits, filling the left with zeroes. Bits
  shifted off the right end are discarded.
- `XOR`, `AND`, and `NOT`: bitwise operations on 32-bit words.
- `+`: addition modulo 2^32 when applied to words.

In a language with signed integer types, take care that shifts and overflow
behave as intended. Prefer an unsigned 32-bit type and explicitly ensure
additions wrap modulo 2^32.

## 1. Prepare the input and pad it

Treat the input as a sequence of bytes. Let `L` be the original message
length in **bits**, before padding.

Append these bits, in order:

1. One `1` bit.
2. The minimum number of `0` bits needed to make the length congruent to
   448 modulo 512.
3. The 64-bit big-endian representation of `L`.

The resulting length is a multiple of 512 bits. The final 64 bits are always
reserved for the original length, so the `1` bit and zeroes must bring the
message to a point 64 bits short of a block boundary. In byte-oriented code,
the first appended byte is `0x80` (binary `10000000`); the remaining padding
bytes are zero. The length field is eight bytes, most significant byte first.

Padding can add one or two blocks. For example, if the message is already
448 bits modulo 512, the `1` bit pushes it past the length-field space, so
SHA-256 uses another full block for the length.

The SHA-256 standard represents the length in a 64-bit field. Your
implementation should decide how it handles inputs that exceed the supported
length range rather than silently encoding a truncated length. Also avoid
overflow when converting a byte count to a bit count.

## 2. Initialize the hash state

The state consists of eight 32-bit words, conventionally named `H0` through
`H7`. Set them to these values before processing any blocks:

| Word | Initial value |
|---|---|
| H0 | `0x6a09e667` |
| H1 | `0xbb67ae85` |
| H2 | `0x3c6ef372` |
| H3 | `0xa54ff53a` |
| H4 | `0x510e527f` |
| H5 | `0x9b05688c` |
| H6 | `0x1f83d9ab` |
| H7 | `0x5be0cd19` |

These are fixed SHA-256 parameters, not values calculated from the input.

## 3. Process each 512-bit block

For each block, first parse its 64 bytes into 16 big-endian 32-bit words,
`W[0]` through `W[15]`. Then extend that schedule to 64 words.

For each `t` from 16 to 63, calculate:

```text
small_sigma0(x) = ROTR^7(x) XOR ROTR^18(x) XOR SHR^3(x)
small_sigma1(x) = ROTR^17(x) XOR ROTR^19(x) XOR SHR^10(x)

W[t] = small_sigma1(W[t-2])
     + W[t-7]
     + small_sigma0(W[t-15])
     + W[t-16]                 (all additions modulo 2^32)
```

The 64 schedule words mix information across the whole block. Keep all
intermediate values as 32-bit words.

### The 64 round constants

Each round uses one fixed constant `K[t]`, in this order:

```text
K[ 0.. 7] = 428a2f98 71374491 b5c0fbcf e9b5dba5 3956c25b 59f111f1 923f82a4 ab1c5ed5
K[ 8..15] = d807aa98 12835b01 243185be 550c7dc3 72be5d74 80deb1fe 9bdc06a7 c19bf174
K[16..23] = e49b69c1 efbe4786 0fc19dc6 240ca1cc 2de92c6f 4a7484aa 5cb0a9dc 76f988da
K[24..31] = 983e5152 a831c66d b00327c8 bf597fc7 c6e00bf3 d5a79147 06ca6351 14292967
K[32..39] = 27b70a85 2e1b2138 4d2c6dfc 53380d13 650a7354 766a0abb 81c2c92e 92722c85
K[40..47] = a2bfe8a1 a81a664b c24b8b70 c76c51a3 d192e819 d6990624 f40e3585 106aa070
K[48..55] = 19a4c116 1e376c08 2748774c 34b0bcb5 391c0cb3 4ed8aa4a 5b9cca4f 682e6ff3
K[56..63] = 748f82ee 78a5636f 84c87814 8cc70208 90befffa a4506ceb bef9a3f7 c67178f2
```

The constants are part of the specification. Their published values can be
derived from the fractional parts of the cube roots of the first 64 primes;
you should use the values above rather than computing them at runtime.

### Compression rounds

Copy the current hash state into eight working words, conventionally called
`a`, `b`, `c`, `d`, `e`, `f`, `g`, and `h`:

```text
a = H0   b = H1   c = H2   d = H3
e = H4   f = H5   g = H6   h = H7
```

The round functions are:

```text
big_sigma0(x) = ROTR^2(x) XOR ROTR^13(x) XOR ROTR^22(x)
big_sigma1(x) = ROTR^6(x) XOR ROTR^11(x) XOR ROTR^25(x)

Ch(x, y, z)  = (x AND y) XOR ((NOT x) AND z)
Maj(x, y, z) = (x AND y) XOR (x AND z) XOR (y AND z)
```

`Ch` chooses bits from `y` or `z` according to `x`. `Maj` returns the
bitwise majority of the three inputs.

For each round `t` from 0 to 63, calculate the two temporary words from the
**old** working values:

```text
T1 = h + big_sigma1(e) + Ch(e, f, g) + K[t] + W[t]
T2 = big_sigma0(a) + Maj(a, b, c)
```

Then update the working words simultaneously (the right-hand side uses the
values from before this round):

```text
h = g
g = f
f = e
e = d + T1
d = c
c = b
b = a
a = T1 + T2
```

All additions in these equations are modulo 2^32. A common implementation
bug is to update `a` before calculating `T2`, or otherwise use a newly
updated word in the same round. Calculate `T1` and `T2` first, then perform
the shift/update.

After all 64 rounds for the block, add each working word back into its
corresponding state word, modulo 2^32:

```text
H0 = H0 + a   H1 = H1 + b   H2 = H2 + c   H3 = H3 + d
H4 = H4 + e   H5 = H5 + f   H6 = H6 + g   H7 = H7 + h
```

The updated `H0..H7` become the starting state for the next block.

## 4. Produce the digest

After every padded block has been processed, write `H0` through `H7` in
order, each as four big-endian bytes. The concatenation is the 32-byte
digest. For display, render each byte as two lowercase hexadecimal digits;
the usual displayed digest is 64 hex characters.

Do not reverse the word order or serialize words in little-endian order.
Those mistakes can leave the internal computation apparently plausible but
produce the wrong digest.

## Language-neutral implementation outline

This is a checklist of the control flow, not source code:

```text
encode input as bytes
record original bit length
append SHA-256 padding and 64-bit big-endian length
initialize H0..H7

for each 64-byte block:
    parse 16 big-endian words
    expand the words to a 64-word schedule
    copy H0..H7 to a..h
    repeat 64 rounds:
        compute T1 and T2 from the current a..h, K[t], and W[t]
        update a..h using the round update
    add a..h back into H0..H7 modulo 2^32

serialize H0..H7 as big-endian bytes
```

## A practical order for building your own implementation

1. Start with a function that accepts a byte sequence and returns 32 bytes;
   decide whether it processes one-shot inputs or supports incremental
   updates.
2. Implement padding and block parsing. Check that the padded input is
   block-aligned and that the encoded length is the *original* length.
3. Implement and inspect the 64-word schedule independently.
4. Implement one block's compression, carefully preserving old round values.
5. Add the final state serialization and hexadecimal formatting.
6. Test boundary lengths around padding changes: 0, 1, 55, 56, 63, 64, and
   65 bytes are useful for a byte-oriented implementation.

For an incremental interface, buffer bytes until a full 64-byte block is
available, keep the total original byte count, and apply padding only when
finalizing. Do not count padding bytes as part of the original message.

## Test vectors

Compare your implementation's output (hexadecimal, lowercase) against known
vectors. These are standard SHA-256 examples:

| Input (text) | SHA-256 digest |
|---|---|
| empty string | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| `abc` | `ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad` |
| `abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq` | `248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1` |

The third test is long enough to span multiple blocks and helps catch errors
that a short single-block input may not reveal. Also test inputs around
padding boundaries, because the empty string and `abc` alone do not exercise
every padding case.

## Common mistakes to watch for

- Using little-endian byte order when parsing blocks, encoding length, or
  serializing the result.
- Computing padding from the padded length instead of the original length.
- Placing the length field at the wrong offset or encoding it little-endian.
- Using a left shift where a right rotation is required, or confusing
  `SHR` with `ROTR`.
- Forgetting modulo-2^32 behavior in additions.
- Updating working variables in place before `T1` and `T2` are both computed.
- Using the wrong schedule indices or round-constant order.
- Treating input text characters as bits without first specifying an encoding
  such as UTF-8. SHA-256 hashes bytes; text-to-byte encoding is a separate
  choice.
- Including padding in an incremental implementation's message-length count.

## Scope and security note

SHA-256 is useful for learning and for verifying data integrity, but a raw
hash is not a password-storage scheme or a message-authentication code.
Real applications should use established cryptographic libraries and
appropriate constructions. For example, password storage needs a
password-hashing algorithm with a salt and work factor, while authentication
typically needs HMAC or another vetted protocol construction.
