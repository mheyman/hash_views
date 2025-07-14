# hash_view

Turn a view into a cryptographic hash view or turn a hashed view into a verified view.

## Work In Progress
*Work In progress - not all unit tests have been built so will probably fail in some cases*

## Overview
This library provides cryptographic hashing via range adapters.

### Features
Hash can be appended to the hashed data or separate.

Hash can be single-byte-based or multi-byte-based (like a range of arrays or
`std::is_standard_layout_v<T>`, `is_trivially_copyable_v<T>`, 
`!is_pointer_v<T>`, and `!is_reference_v<T>` types).

### Padding

If multi-byte-based, the option of padding the hash to fit exists. Padding is
not required if the user knows the hash will exactly fit the range of
multi-byte values. An exception will get thrown if requesting a
multi-byte-value-encoded hash that doesn't fit exactly.

The padding, if any, is the standard bit-padding of a single 1 bit followed by
as many 0 bits as needed to fill the range as described in RFC1321 (although
this usage isn't for the same purpose as that usage).

#### Caveat
Currently, hash lengths are byte-based, not bit-based. This may change and
include special handling for blake2 which requires byte-based lengths.

## Hash
**TODO**

### Examples
**TODO**

## Verify
**TODO**

### Examples
**TODO**
