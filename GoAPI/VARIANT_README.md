# SVariant Go Implementation

This is a Go implementation of the SVariant binary serialization format, compatible with the C/C++ implementation found in `Common/SVariant.c` and `Common/SVariant.h`.

## Overview

The SVariant format is a compact binary serialization format that supports various data types including integers, floats, strings, byte arrays, and container types (maps, lists, and indexes).

## Features

- **Full compatibility** with C/C++ SVariant binary format
- **Type-safe** reading and writing of variants
- **Support for all variant types**:
  - Integers (signed and unsigned, 8/16/32/64 bit)
  - Floating point numbers (32 and 64 bit)
  - Strings (ASCII, UTF-8, UTF-16)
  - Byte arrays
  - Containers (Map, List, Index)
- **Efficient encoding** with automatic size optimization
- **Builder pattern** for easy construction of complex variants
- **Iterator support** for lists

## Installation

```go
import "goapi"
```

## Basic Usage

### Creating Simple Variants

```go
// Integers
v1 := goapi.NewInt(42, 4)           // 32-bit signed integer
v2 := goapi.NewUInt(255, 1)         // 8-bit unsigned integer

// Floating point
v3 := goapi.NewFloat64(3.14159)

// Strings
v4 := goapi.NewString("Hello, World!")
v5 := goapi.NewUTF16String("Hello, 世界!")

// Bytes
v6 := goapi.NewBytes([]byte{0x01, 0x02, 0x03})
```

### Creating Container Variants

#### Map (Dictionary)

```go
builder := goapi.NewMapBuilder()
builder.InsertUInt("age", 42, 4)
builder.InsertString("name", "John Doe")
builder.InsertInt("temperature", -15, 2)

mapVariant, err := builder.Build()
if err != nil {
    // Handle error
}

// Access map values
age, err := mapVariant.Find("age")
if err != nil {
    // Key not found
}
value := age.AsUInt32(0) // Returns 42
```

#### List

```go
builder := goapi.NewListBuilder()
builder.AppendUInt(100, 4)
builder.AppendString("test")
builder.AppendInt(-50, 2)

listVariant, err := builder.Build()
if err != nil {
    // Handle error
}

// Access by position
item, err := listVariant.At(0)
if err != nil {
    // Index out of bounds
}

// Or iterate
it, err := listVariant.NewIterator()
if err != nil {
    // Not a list
}

for {
    item, ok := it.Next()
    if !ok {
        break
    }
    // Process item
}
```

#### Index (Map by uint32)

```go
builder := goapi.NewIndexBuilder()
builder.AddUInt(10, 100, 4)
builder.Add(20, goapi.NewString("test"))
builder.AddUInt(30, 200, 4)

indexVariant, err := builder.Build()
if err != nil {
    // Handle error
}

// Access by index
item, err := indexVariant.Get(20)
if err != nil {
    // Index not found
}
```

### Serialization and Deserialization

```go
// Serialize to bytes
data, err := variant.ToBuffer()
if err != nil {
    // Handle error
}

// Deserialize from bytes
variant, bytesRead, err := goapi.FromBuffer(data)
if err != nil {
    // Handle error
}

// Serialize with name (packet format)
data, err := variant.ToPacket("myVariant")
if err != nil {
    // Handle error
}

// Deserialize from packet
name, variant, bytesRead, err := goapi.FromPacket(data)
if err != nil {
    // Handle error
}
```

### Type Conversion

All conversion functions accept a default value that is returned if the conversion fails:

```go
// Integer conversions
val := variant.AsUInt8(0)    // Returns 0 if conversion fails
val := variant.AsUInt16(0)
val := variant.AsUInt32(0)
val := variant.AsUInt64(0)
val := variant.AsInt8(0)
val := variant.AsInt16(0)
val := variant.AsInt32(0)
val := variant.AsInt64(0)

// Float conversions
val := variant.AsFloat32(0.0)
val := variant.AsFloat64(0.0)

// String conversions (return error on failure)
str, err := variant.AsString()
str, err := variant.AsUTF16String()

// Bytes conversion
bytes, err := variant.AsBytes()
```

### Nested Variants

```go
// Create nested structure
innerList := goapi.NewListBuilder()
innerList.AppendUInt(1, 1)
innerList.AppendUInt(2, 1)
innerList.AppendUInt(3, 1)
innerListVariant, _ := innerList.Build()

builder := goapi.NewMapBuilder()
builder.InsertString("name", "test")
builder.Insert("numbers", innerListVariant)
builder.InsertUInt("count", 3, 4)

mapVariant, _ := builder.Build()

// Access nested data
numbers, err := mapVariant.Find("numbers")
if err != nil {
    // Handle error
}

// Iterate through nested list
it, _ := numbers.NewIterator()
for {
    item, ok := it.Next()
    if !ok {
        break
    }
    value := item.AsUInt8(0)
    // Process value
}
```

## Binary Format

The binary format is compatible with the C implementation and uses the following encoding:

### Type Byte

```
Bits 0-4: Type (VAR_TYPE_*)
Bit 5:    Length field flag
Bits 6-7: Length encoding
```

### Length Encoding

- **Fixed sizes** (1, 2, 4, 8 bytes): Encoded in type byte
- **Variable sizes**: Separate length field after type byte
  - 1 byte length for sizes < 255
  - 2 byte length for sizes < 65535
  - 4 byte length for sizes >= 65535

### Container Formats

- **Map**: Sequence of (name_length, name, variant) tuples
- **List**: Sequence of variants
- **Index**: Sequence of (uint32_index, variant) tuples

## Error Handling

The package defines several error types:

- `ErrInvalidFormat`: Invalid binary format
- `ErrBufferTooSmall`: Buffer too small for data
- `ErrInvalidType`: Invalid variant type
- `ErrTypeMismatch`: Type mismatch during conversion
- `ErrNotFound`: Key or index not found in container

## Performance

The implementation is optimized for:

- **Zero-copy reads**: Data is not copied when reading from buffers
- **Efficient encoding**: Automatic selection of optimal size encoding
- **Minimal allocations**: Reuse of buffers where possible

Run benchmarks:

```bash
go test -bench=.
```

## Compatibility

This implementation is **fully compatible** with the C/C++ SVariant implementation. Binary data created by either implementation can be read by the other.

### Byte Order

All multi-byte values use **little-endian** encoding, matching the C implementation.

### Type Mappings

| Go Type | SVariant Type | C/C++ Type |
|---------|---------------|------------|
| uint8/16/32/64 | VarTypeUInt | uint8/16/32/64 |
| int8/16/32/64 | VarTypeSInt | sint8/16/32/64 |
| float32 | VarTypeDouble | float |
| float64 | VarTypeDouble | double |
| string | VarTypeASCII | char* |
| []byte | VarTypeBytes | byte* |
| UTF-16 string | VarTypeUnicode | wchar_t* |

## Testing

Run all tests:

```bash
go test -v
```

The test suite includes:

- Basic type serialization/deserialization
- Container operations (maps, lists, indexes)
- Nested variants
- Large data handling
- C compatibility tests
- Benchmarks

## Examples

See `variant_test.go` for comprehensive examples of all features.

## License

This implementation is part of the MajorPrivacy project.
