package goapi

import (
	"bytes"
	"encoding/binary"
	"errors"
	"fmt"
	"math"
	"unicode/utf16"
)

// Variant type constants
const (
	VarTypeEmpty   = 0b00000000 // Empty variant - no payload and no length field
	VarTypeMap     = 0b00000001 // Dictionary (Map by Name)
	VarTypeList    = 0b00000010 // List
	VarTypeIndex   = 0b00000011 // Index (Map by uint32)
	VarTypeBytes   = 0b00000111 // Binary BLOB
	VarTypeASCII   = 0b00001000 // String ASCII Encoded
	VarTypeUTF8    = 0b00001001 // String UTF8 Encoded or ASCII
	VarTypeUnicode = 0b00001010 // String UTF16 Encoded
	VarTypeUInt    = 0b00001100 // Unsigned Integer 8, 16, 32, or 64 bit
	VarTypeSInt    = 0b00001101 // Signed Integer 8, 16, 32, or 64 bit
	VarTypeDouble  = 0b00001110 // Floating point Number 32 or 64 bit
	VarTypeCustom  = 0b00010000
	VarTypeInvalid = 0b00011111

	varTypeMask   = 0b00011111
	varLenField   = 0b00100000 // Indicates dedicated length field after type byte
	varLenMask    = 0b11000000
	varLen8       = 0b00000000
	varLen16      = 0b01000000
	varLen32      = 0b10000000
	varLen64      = 0b11000000
)

var (
	ErrInvalidFormat = errors.New("invalid variant format")
	ErrBufferTooSmall = errors.New("buffer too small")
	ErrInvalidType = errors.New("invalid variant type")
	ErrTypeMismatch = errors.New("type mismatch")
	ErrNotFound = errors.New("key not found")
)

// Variant represents a serializable variant value
type Variant struct {
	varType uint8
	data    []byte
}

// NewVariant creates a new empty variant
func NewVariant() *Variant {
	return &Variant{
		varType: VarTypeEmpty,
		data:    []byte{},
	}
}

// Type returns the variant type
func (v *Variant) Type() uint8 {
	return v.varType
}

// IsEmpty returns true if the variant is empty
func (v *Variant) IsEmpty() bool {
	return v.varType == VarTypeEmpty
}

// FromBuffer deserializes a variant from a byte buffer
func FromBuffer(buffer []byte) (*Variant, int, error) {
	if len(buffer) < 1 {
		return nil, 0, ErrInvalidFormat
	}

	typeByte := buffer[0]
	varType := typeByte & varTypeMask
	offset := 1

	// Read length
	length, bytesRead, err := readSize(buffer, typeByte)
	if err != nil {
		return nil, 0, err
	}
	offset += bytesRead

	// Validate data length
	if offset+int(length) > len(buffer) {
		return nil, 0, ErrBufferTooSmall
	}

	v := &Variant{
		varType: varType,
		data:    make([]byte, length),
	}
	copy(v.data, buffer[offset:offset+int(length)])

	return v, offset + int(length), nil
}

// FromPacket deserializes a variant from a packet with a name prefix
func FromPacket(buffer []byte) (string, *Variant, int, error) {
	if len(buffer) < 1 {
		return "", nil, 0, ErrInvalidFormat
	}

	nameLen := int(buffer[0])
	if len(buffer) < 1+nameLen {
		return "", nil, 0, ErrBufferTooSmall
	}

	name := string(buffer[1 : 1+nameLen])

	variant, bytesRead, err := FromBuffer(buffer[1+nameLen:])
	if err != nil {
		return "", nil, 0, err
	}

	return name, variant, 1 + nameLen + bytesRead, nil
}

// readSize reads the length from the buffer based on the type byte
func readSize(buffer []byte, typeByte uint8) (uint32, int, error) {
	varType := typeByte & varTypeMask

	// Empty type has no length
	if varType == VarTypeEmpty {
		return 0, 0, nil
	}

	if typeByte&varLenField != 0 {
		// Dedicated length field
		lenType := typeByte & varLenMask
		switch lenType {
		case varLen8:
			if len(buffer) < 2 {
				return 0, 0, ErrBufferTooSmall
			}
			return uint32(buffer[1]), 1, nil
		case varLen16:
			if len(buffer) < 3 {
				return 0, 0, ErrBufferTooSmall
			}
			return uint32(binary.LittleEndian.Uint16(buffer[1:3])), 2, nil
		case varLen32:
			if len(buffer) < 5 {
				return 0, 0, ErrBufferTooSmall
			}
			return binary.LittleEndian.Uint32(buffer[1:5]), 4, nil
		}
	} else {
		// Fixed size encoded in type byte
		lenType := typeByte & varLenMask
		switch lenType {
		case varLen8:
			return 1, 0, nil
		case varLen16:
			return 2, 0, nil
		case varLen32:
			return 4, 0, nil
		case varLen64:
			return 8, 0, nil
		}
	}

	return 0, 0, ErrInvalidFormat
}

// ToBuffer serializes the variant to a byte buffer
func (v *Variant) ToBuffer() ([]byte, error) {
	buf := &bytes.Buffer{}

	if err := v.writeToBuffer(buf); err != nil {
		return nil, err
	}

	return buf.Bytes(), nil
}

// ToPacket serializes the variant to a packet with a name prefix
func (v *Variant) ToPacket(name string) ([]byte, error) {
	if len(name) > 127 {
		return nil, errors.New("name too long (max 127 bytes)")
	}

	buf := &bytes.Buffer{}
	buf.WriteByte(byte(len(name)))
	buf.WriteString(name)

	if err := v.writeToBuffer(buf); err != nil {
		return nil, err
	}

	return buf.Bytes(), nil
}

// writeToBuffer writes the variant to a buffer
func (v *Variant) writeToBuffer(buf *bytes.Buffer) error {
	dataLen := len(v.data)

	// Write type and length
	if v.varType == VarTypeEmpty {
		buf.WriteByte(VarTypeEmpty)
	} else if dataLen == 1 {
		buf.WriteByte(v.varType | varLen8)
	} else if dataLen == 2 {
		buf.WriteByte(v.varType | varLen16)
	} else if dataLen == 4 {
		buf.WriteByte(v.varType | varLen32)
	} else if dataLen == 8 {
		buf.WriteByte(v.varType | varLen64)
	} else {
		// Use length field
		if dataLen >= 65535 {
			buf.WriteByte(v.varType | varLenField | varLen32)
			binary.Write(buf, binary.LittleEndian, uint32(dataLen))
		} else if dataLen >= 255 {
			buf.WriteByte(v.varType | varLenField | varLen16)
			binary.Write(buf, binary.LittleEndian, uint16(dataLen))
		} else {
			buf.WriteByte(v.varType | varLenField | varLen8)
			buf.WriteByte(byte(dataLen))
		}
	}

	// Write data
	buf.Write(v.data)

	return nil
}

// Integer conversion functions

// AsUInt8 returns the variant as a uint8
func (v *Variant) AsUInt8(defaultVal uint8) uint8 {
	val, err := v.asUInt64()
	if err != nil || val > math.MaxUint8 {
		return defaultVal
	}
	return uint8(val)
}

// AsUInt16 returns the variant as a uint16
func (v *Variant) AsUInt16(defaultVal uint16) uint16 {
	val, err := v.asUInt64()
	if err != nil || val > math.MaxUint16 {
		return defaultVal
	}
	return uint16(val)
}

// AsUInt32 returns the variant as a uint32
func (v *Variant) AsUInt32(defaultVal uint32) uint32 {
	val, err := v.asUInt64()
	if err != nil || val > math.MaxUint32 {
		return defaultVal
	}
	return uint32(val)
}

// AsUInt64 returns the variant as a uint64
func (v *Variant) AsUInt64(defaultVal uint64) uint64 {
	val, err := v.asUInt64()
	if err != nil {
		return defaultVal
	}
	return val
}

// AsInt8 returns the variant as an int8
func (v *Variant) AsInt8(defaultVal int8) int8 {
	val, err := v.asInt64()
	if err != nil || val < math.MinInt8 || val > math.MaxInt8 {
		return defaultVal
	}
	return int8(val)
}

// AsInt16 returns the variant as an int16
func (v *Variant) AsInt16(defaultVal int16) int16 {
	val, err := v.asInt64()
	if err != nil || val < math.MinInt16 || val > math.MaxInt16 {
		return defaultVal
	}
	return int16(val)
}

// AsInt32 returns the variant as an int32
func (v *Variant) AsInt32(defaultVal int32) int32 {
	val, err := v.asInt64()
	if err != nil || val < math.MinInt32 || val > math.MaxInt32 {
		return defaultVal
	}
	return int32(val)
}

// AsInt64 returns the variant as an int64
func (v *Variant) AsInt64(defaultVal int64) int64 {
	val, err := v.asInt64()
	if err != nil {
		return defaultVal
	}
	return val
}

// asUInt64 converts the variant to uint64
func (v *Variant) asUInt64() (uint64, error) {
	if v.varType != VarTypeUInt {
		return 0, ErrTypeMismatch
	}

	switch len(v.data) {
	case 1:
		return uint64(v.data[0]), nil
	case 2:
		return uint64(binary.LittleEndian.Uint16(v.data)), nil
	case 4:
		return uint64(binary.LittleEndian.Uint32(v.data)), nil
	case 8:
		return binary.LittleEndian.Uint64(v.data), nil
	default:
		return 0, ErrInvalidFormat
	}
}

// asInt64 converts the variant to int64
func (v *Variant) asInt64() (int64, error) {
	if v.varType != VarTypeSInt {
		return 0, ErrTypeMismatch
	}

	switch len(v.data) {
	case 1:
		return int64(int8(v.data[0])), nil
	case 2:
		return int64(int16(binary.LittleEndian.Uint16(v.data))), nil
	case 4:
		return int64(int32(binary.LittleEndian.Uint32(v.data))), nil
	case 8:
		return int64(binary.LittleEndian.Uint64(v.data)), nil
	default:
		return 0, ErrInvalidFormat
	}
}

// AsFloat32 returns the variant as a float32
func (v *Variant) AsFloat32(defaultVal float32) float32 {
	if v.varType != VarTypeDouble || len(v.data) != 4 {
		return defaultVal
	}
	bits := binary.LittleEndian.Uint32(v.data)
	return math.Float32frombits(bits)
}

// AsFloat64 returns the variant as a float64
func (v *Variant) AsFloat64(defaultVal float64) float64 {
	if v.varType != VarTypeDouble || len(v.data) != 8 {
		return defaultVal
	}
	bits := binary.LittleEndian.Uint64(v.data)
	return math.Float64frombits(bits)
}

// String conversion functions

// AsBytes returns the variant data as a byte slice
func (v *Variant) AsBytes() ([]byte, error) {
	if v.varType != VarTypeBytes && v.varType != VarTypeASCII {
		return nil, ErrTypeMismatch
	}
	return v.data, nil
}

// AsString returns the variant as a string (works with ASCII and UTF8)
func (v *Variant) AsString() (string, error) {
	if v.varType != VarTypeASCII && v.varType != VarTypeUTF8 {
		return "", ErrTypeMismatch
	}
	return string(v.data), nil
}

// AsUTF16String returns the variant as a UTF-16 string
func (v *Variant) AsUTF16String() (string, error) {
	if v.varType != VarTypeUnicode {
		return "", ErrTypeMismatch
	}

	if len(v.data)%2 != 0 {
		return "", ErrInvalidFormat
	}

	u16 := make([]uint16, len(v.data)/2)
	for i := 0; i < len(u16); i++ {
		u16[i] = binary.LittleEndian.Uint16(v.data[i*2:])
	}

	runes := utf16.Decode(u16)
	return string(runes), nil
}

// Container access functions

// Find searches for a key in a map variant
func (v *Variant) Find(name string) (*Variant, error) {
	if v.varType != VarTypeMap {
		return nil, ErrTypeMismatch
	}

	nameBytes := []byte(name)
	offset := 0

	for offset < len(v.data) {
		if offset >= len(v.data) {
			break
		}

		keyLen := int(v.data[offset])
		offset++

		if offset+keyLen > len(v.data) {
			break
		}

		key := v.data[offset : offset+keyLen]
		offset += keyLen

		if bytes.Equal(key, nameBytes) {
			variant, _, err := FromBuffer(v.data[offset:])
			if err != nil {
				return nil, err
			}
			return variant, nil
		}

		// Skip value
		_, bytesRead, err := FromBuffer(v.data[offset:])
		if err != nil {
			return nil, err
		}
		offset += bytesRead
	}

	return nil, ErrNotFound
}

// Get retrieves a value by index from an index variant
func (v *Variant) Get(index uint32) (*Variant, error) {
	if v.varType != VarTypeIndex {
		return nil, ErrTypeMismatch
	}

	offset := 0

	for offset < len(v.data) {
		if offset+4 > len(v.data) {
			break
		}

		curIndex := binary.LittleEndian.Uint32(v.data[offset:])
		offset += 4

		if curIndex == index {
			variant, _, err := FromBuffer(v.data[offset:])
			if err != nil {
				return nil, err
			}
			return variant, nil
		}

		// Skip value
		_, bytesRead, err := FromBuffer(v.data[offset:])
		if err != nil {
			return nil, err
		}
		offset += bytesRead
	}

	return nil, ErrNotFound
}

// At retrieves a value by position from a list variant
func (v *Variant) At(pos int) (*Variant, error) {
	if v.varType != VarTypeList {
		return nil, ErrTypeMismatch
	}

	offset := 0
	currentPos := 0

	for offset < len(v.data) {
		if currentPos == pos {
			variant, _, err := FromBuffer(v.data[offset:])
			if err != nil {
				return nil, err
			}
			return variant, nil
		}

		// Skip to next item
		_, bytesRead, err := FromBuffer(v.data[offset:])
		if err != nil {
			return nil, err
		}
		offset += bytesRead
		currentPos++
	}

	return nil, ErrNotFound
}

// Iterator for list variants
type VariantIterator struct {
	data   []byte
	offset int
}

// NewIterator creates an iterator for a list variant
func (v *Variant) NewIterator() (*VariantIterator, error) {
	if v.varType != VarTypeList {
		return nil, ErrTypeMismatch
	}

	return &VariantIterator{
		data:   v.data,
		offset: 0,
	}, nil
}

// Next returns the next variant in the list
func (it *VariantIterator) Next() (*Variant, bool) {
	if it.offset >= len(it.data) {
		return nil, false
	}

	variant, bytesRead, err := FromBuffer(it.data[it.offset:])
	if err != nil {
		return nil, false
	}

	it.offset += bytesRead
	return variant, true
}

// Builder for creating variants

// VariantBuilder helps build complex variants
type VariantBuilder struct {
	varType uint8
	buf     *bytes.Buffer
	err     error
}

// NewMapBuilder creates a builder for a map variant
func NewMapBuilder() *VariantBuilder {
	return &VariantBuilder{
		varType: VarTypeMap,
		buf:     &bytes.Buffer{},
	}
}

// NewListBuilder creates a builder for a list variant
func NewListBuilder() *VariantBuilder {
	return &VariantBuilder{
		varType: VarTypeList,
		buf:     &bytes.Buffer{},
	}
}

// NewIndexBuilder creates a builder for an index variant
func NewIndexBuilder() *VariantBuilder {
	return &VariantBuilder{
		varType: VarTypeIndex,
		buf:     &bytes.Buffer{},
	}
}

// InsertUInt inserts a uint value with a name (for maps)
func (b *VariantBuilder) InsertUInt(name string, value uint64, size int) *VariantBuilder {
	if b.err != nil {
		return b
	}
	if b.varType != VarTypeMap {
		b.err = ErrTypeMismatch
		return b
	}

	// Write name
	b.buf.WriteByte(byte(len(name)))
	b.buf.WriteString(name)

	// Write value
	v := NewUInt(value, size)
	data, err := v.ToBuffer()
	if err != nil {
		b.err = err
		return b
	}
	b.buf.Write(data)

	return b
}

// InsertInt inserts a signed int value with a name (for maps)
func (b *VariantBuilder) InsertInt(name string, value int64, size int) *VariantBuilder {
	if b.err != nil {
		return b
	}
	if b.varType != VarTypeMap {
		b.err = ErrTypeMismatch
		return b
	}

	// Write name
	b.buf.WriteByte(byte(len(name)))
	b.buf.WriteString(name)

	// Write value
	v := NewInt(value, size)
	data, err := v.ToBuffer()
	if err != nil {
		b.err = err
		return b
	}
	b.buf.Write(data)

	return b
}

// InsertString inserts a string value with a name (for maps)
func (b *VariantBuilder) InsertString(name string, value string) *VariantBuilder {
	if b.err != nil {
		return b
	}
	if b.varType != VarTypeMap {
		b.err = ErrTypeMismatch
		return b
	}

	// Write name
	b.buf.WriteByte(byte(len(name)))
	b.buf.WriteString(name)

	// Write value
	v := NewString(value)
	data, err := v.ToBuffer()
	if err != nil {
		b.err = err
		return b
	}
	b.buf.Write(data)

	return b
}

// Insert inserts a variant with a name (for maps)
func (b *VariantBuilder) Insert(name string, value *Variant) *VariantBuilder {
	if b.err != nil {
		return b
	}
	if b.varType != VarTypeMap {
		b.err = ErrTypeMismatch
		return b
	}

	// Write name
	b.buf.WriteByte(byte(len(name)))
	b.buf.WriteString(name)

	// Write value
	data, err := value.ToBuffer()
	if err != nil {
		b.err = err
		return b
	}
	b.buf.Write(data)

	return b
}

// AddUInt adds a uint value with an index (for index containers)
func (b *VariantBuilder) AddUInt(index uint32, value uint64, size int) *VariantBuilder {
	if b.err != nil {
		return b
	}
	if b.varType != VarTypeIndex {
		b.err = ErrTypeMismatch
		return b
	}

	// Write index
	binary.Write(b.buf, binary.LittleEndian, index)

	// Write value
	v := NewUInt(value, size)
	data, err := v.ToBuffer()
	if err != nil {
		b.err = err
		return b
	}
	b.buf.Write(data)

	return b
}

// Add adds a variant with an index (for index containers)
func (b *VariantBuilder) Add(index uint32, value *Variant) *VariantBuilder {
	if b.err != nil {
		return b
	}
	if b.varType != VarTypeIndex {
		b.err = ErrTypeMismatch
		return b
	}

	// Write index
	binary.Write(b.buf, binary.LittleEndian, index)

	// Write value
	data, err := value.ToBuffer()
	if err != nil {
		b.err = err
		return b
	}
	b.buf.Write(data)

	return b
}

// AppendUInt appends a uint value (for lists)
func (b *VariantBuilder) AppendUInt(value uint64, size int) *VariantBuilder {
	if b.err != nil {
		return b
	}
	if b.varType != VarTypeList {
		b.err = ErrTypeMismatch
		return b
	}

	// Write value
	v := NewUInt(value, size)
	data, err := v.ToBuffer()
	if err != nil {
		b.err = err
		return b
	}
	b.buf.Write(data)

	return b
}

// AppendInt appends a signed int value (for lists)
func (b *VariantBuilder) AppendInt(value int64, size int) *VariantBuilder {
	if b.err != nil {
		return b
	}
	if b.varType != VarTypeList {
		b.err = ErrTypeMismatch
		return b
	}

	// Write value
	v := NewInt(value, size)
	data, err := v.ToBuffer()
	if err != nil {
		b.err = err
		return b
	}
	b.buf.Write(data)

	return b
}

// AppendString appends a string value (for lists)
func (b *VariantBuilder) AppendString(value string) *VariantBuilder {
	if b.err != nil {
		return b
	}
	if b.varType != VarTypeList {
		b.err = ErrTypeMismatch
		return b
	}

	// Write value
	v := NewString(value)
	data, err := v.ToBuffer()
	if err != nil {
		b.err = err
		return b
	}
	b.buf.Write(data)

	return b
}

// Append appends a variant (for lists)
func (b *VariantBuilder) Append(value *Variant) *VariantBuilder {
	if b.err != nil {
		return b
	}
	if b.varType != VarTypeList {
		b.err = ErrTypeMismatch
		return b
	}

	// Write value
	data, err := value.ToBuffer()
	if err != nil {
		b.err = err
		return b
	}
	b.buf.Write(data)

	return b
}

// Build creates the final variant
func (b *VariantBuilder) Build() (*Variant, error) {
	if b.err != nil {
		return nil, b.err
	}

	return &Variant{
		varType: b.varType,
		data:    b.buf.Bytes(),
	}, nil
}

// Factory functions for creating simple variants

// NewUInt creates a new unsigned integer variant
func NewUInt(value uint64, size int) *Variant {
	data := make([]byte, size)
	switch size {
	case 1:
		data[0] = byte(value)
	case 2:
		binary.LittleEndian.PutUint16(data, uint16(value))
	case 4:
		binary.LittleEndian.PutUint32(data, uint32(value))
	case 8:
		binary.LittleEndian.PutUint64(data, value)
	}

	return &Variant{
		varType: VarTypeUInt,
		data:    data,
	}
}

// NewInt creates a new signed integer variant
func NewInt(value int64, size int) *Variant {
	data := make([]byte, size)
	switch size {
	case 1:
		data[0] = byte(int8(value))
	case 2:
		binary.LittleEndian.PutUint16(data, uint16(int16(value)))
	case 4:
		binary.LittleEndian.PutUint32(data, uint32(int32(value)))
	case 8:
		binary.LittleEndian.PutUint64(data, uint64(value))
	}

	return &Variant{
		varType: VarTypeSInt,
		data:    data,
	}
}

// NewFloat32 creates a new 32-bit float variant
func NewFloat32(value float32) *Variant {
	data := make([]byte, 4)
	binary.LittleEndian.PutUint32(data, math.Float32bits(value))

	return &Variant{
		varType: VarTypeDouble,
		data:    data,
	}
}

// NewFloat64 creates a new 64-bit float variant
func NewFloat64(value float64) *Variant {
	data := make([]byte, 8)
	binary.LittleEndian.PutUint64(data, math.Float64bits(value))

	return &Variant{
		varType: VarTypeDouble,
		data:    data,
	}
}

// NewBytes creates a new bytes variant
func NewBytes(value []byte) *Variant {
	return &Variant{
		varType: VarTypeBytes,
		data:    value,
	}
}

// NewString creates a new ASCII/UTF8 string variant
func NewString(value string) *Variant {
	return &Variant{
		varType: VarTypeASCII,
		data:    []byte(value),
	}
}

// NewUTF16String creates a new UTF-16 string variant
func NewUTF16String(value string) *Variant {
	runes := []rune(value)
	u16 := utf16.Encode(runes)
	data := make([]byte, len(u16)*2)

	for i, v := range u16 {
		binary.LittleEndian.PutUint16(data[i*2:], v)
	}

	return &Variant{
		varType: VarTypeUnicode,
		data:    data,
	}
}

// String returns a string representation of the variant (for debugging)
func (v *Variant) String() string {
	switch v.varType {
	case VarTypeEmpty:
		return "Empty"
	case VarTypeMap:
		return fmt.Sprintf("Map[%d bytes]", len(v.data))
	case VarTypeList:
		return fmt.Sprintf("List[%d bytes]", len(v.data))
	case VarTypeIndex:
		return fmt.Sprintf("Index[%d bytes]", len(v.data))
	case VarTypeBytes:
		return fmt.Sprintf("Bytes[%d]", len(v.data))
	case VarTypeASCII, VarTypeUTF8:
		return fmt.Sprintf("String(%s)", string(v.data))
	case VarTypeUnicode:
		str, _ := v.AsUTF16String()
		return fmt.Sprintf("UTF16String(%s)", str)
	case VarTypeUInt:
		val, _ := v.asUInt64()
		return fmt.Sprintf("UInt(%d)", val)
	case VarTypeSInt:
		val, _ := v.asInt64()
		return fmt.Sprintf("Int(%d)", val)
	case VarTypeDouble:
		if len(v.data) == 4 {
			return fmt.Sprintf("Float32(%f)", v.AsFloat32(0))
		}
		return fmt.Sprintf("Float64(%f)", v.AsFloat64(0))
	default:
		return fmt.Sprintf("Unknown[type=%d, %d bytes]", v.varType, len(v.data))
	}
}
