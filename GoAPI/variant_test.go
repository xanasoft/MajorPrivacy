package goapi

import (
	"bytes"
	"encoding/binary"
	"testing"
)

// Test simple integer variants
func TestIntegerVariants(t *testing.T) {
	tests := []struct {
		name     string
		value    int64
		size     int
		expected int64
	}{
		{"Int8 positive", 42, 1, 42},
		{"Int8 negative", -42, 1, -42},
		{"Int16 positive", 1000, 2, 1000},
		{"Int16 negative", -1000, 2, -1000},
		{"Int32 positive", 100000, 4, 100000},
		{"Int32 negative", -100000, 4, -100000},
		{"Int64 positive", 10000000000, 8, 10000000000},
		{"Int64 negative", -10000000000, 8, -10000000000},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			// Create variant
			v := NewInt(tt.value, tt.size)

			// Serialize
			data, err := v.ToBuffer()
			if err != nil {
				t.Fatalf("Failed to serialize: %v", err)
			}

			// Deserialize
			v2, bytesRead, err := FromBuffer(data)
			if err != nil {
				t.Fatalf("Failed to deserialize: %v", err)
			}

			if bytesRead != len(data) {
				t.Errorf("Expected to read %d bytes, got %d", len(data), bytesRead)
			}

			// Check value
			result := v2.AsInt64(0)
			if result != tt.expected {
				t.Errorf("Expected %d, got %d", tt.expected, result)
			}
		})
	}
}

// Test unsigned integer variants
func TestUIntegerVariants(t *testing.T) {
	tests := []struct {
		name     string
		value    uint64
		size     int
		expected uint64
	}{
		{"UInt8", 255, 1, 255},
		{"UInt16", 65535, 2, 65535},
		{"UInt32", 4294967295, 4, 4294967295},
		{"UInt64", 18446744073709551615, 8, 18446744073709551615},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			// Create variant
			v := NewUInt(tt.value, tt.size)

			// Serialize
			data, err := v.ToBuffer()
			if err != nil {
				t.Fatalf("Failed to serialize: %v", err)
			}

			// Deserialize
			v2, bytesRead, err := FromBuffer(data)
			if err != nil {
				t.Fatalf("Failed to deserialize: %v", err)
			}

			if bytesRead != len(data) {
				t.Errorf("Expected to read %d bytes, got %d", len(data), bytesRead)
			}

			// Check value
			result := v2.AsUInt64(0)
			if result != tt.expected {
				t.Errorf("Expected %d, got %d", tt.expected, result)
			}
		})
	}
}

// Test float variants
func TestFloatVariants(t *testing.T) {
	t.Run("Float32", func(t *testing.T) {
		value := float32(3.14159)
		v := NewFloat32(value)

		// Serialize
		data, err := v.ToBuffer()
		if err != nil {
			t.Fatalf("Failed to serialize: %v", err)
		}

		// Deserialize
		v2, _, err := FromBuffer(data)
		if err != nil {
			t.Fatalf("Failed to deserialize: %v", err)
		}

		// Check value
		result := v2.AsFloat32(0)
		if result != value {
			t.Errorf("Expected %f, got %f", value, result)
		}
	})

	t.Run("Float64", func(t *testing.T) {
		value := float64(3.141592653589793)
		v := NewFloat64(value)

		// Serialize
		data, err := v.ToBuffer()
		if err != nil {
			t.Fatalf("Failed to serialize: %v", err)
		}

		// Deserialize
		v2, _, err := FromBuffer(data)
		if err != nil {
			t.Fatalf("Failed to deserialize: %v", err)
		}

		// Check value
		result := v2.AsFloat64(0)
		if result != value {
			t.Errorf("Expected %f, got %f", value, result)
		}
	})
}

// Test string variants
func TestStringVariants(t *testing.T) {
	t.Run("ASCII String", func(t *testing.T) {
		value := "Hello, World!"
		v := NewString(value)

		// Serialize
		data, err := v.ToBuffer()
		if err != nil {
			t.Fatalf("Failed to serialize: %v", err)
		}

		// Deserialize
		v2, _, err := FromBuffer(data)
		if err != nil {
			t.Fatalf("Failed to deserialize: %v", err)
		}

		// Check value
		result, err := v2.AsString()
		if err != nil {
			t.Fatalf("Failed to get string: %v", err)
		}
		if result != value {
			t.Errorf("Expected %s, got %s", value, result)
		}
	})

	t.Run("UTF16 String", func(t *testing.T) {
		value := "Hello, 世界!"
		v := NewUTF16String(value)

		// Serialize
		data, err := v.ToBuffer()
		if err != nil {
			t.Fatalf("Failed to serialize: %v", err)
		}

		// Deserialize
		v2, _, err := FromBuffer(data)
		if err != nil {
			t.Fatalf("Failed to deserialize: %v", err)
		}

		// Check value
		result, err := v2.AsUTF16String()
		if err != nil {
			t.Fatalf("Failed to get string: %v", err)
		}
		if result != value {
			t.Errorf("Expected %s, got %s", value, result)
		}
	})

	t.Run("Empty String", func(t *testing.T) {
		value := ""
		v := NewString(value)

		// Serialize
		data, err := v.ToBuffer()
		if err != nil {
			t.Fatalf("Failed to serialize: %v", err)
		}

		// Deserialize
		v2, _, err := FromBuffer(data)
		if err != nil {
			t.Fatalf("Failed to deserialize: %v", err)
		}

		// Check value
		result, err := v2.AsString()
		if err != nil {
			t.Fatalf("Failed to get string: %v", err)
		}
		if result != value {
			t.Errorf("Expected empty string, got %s", result)
		}
	})
}

// Test bytes variants
func TestBytesVariants(t *testing.T) {
	value := []byte{0x00, 0x01, 0x02, 0xFF, 0xFE, 0xFD}
	v := NewBytes(value)

	// Serialize
	data, err := v.ToBuffer()
	if err != nil {
		t.Fatalf("Failed to serialize: %v", err)
	}

	// Deserialize
	v2, _, err := FromBuffer(data)
	if err != nil {
		t.Fatalf("Failed to deserialize: %v", err)
	}

	// Check value
	result, err := v2.AsBytes()
	if err != nil {
		t.Fatalf("Failed to get bytes: %v", err)
	}
	if !bytes.Equal(result, value) {
		t.Errorf("Expected %v, got %v", value, result)
	}
}

// Test map variants
func TestMapVariants(t *testing.T) {
	// Build a map
	builder := NewMapBuilder()
	builder.InsertUInt("age", 42, 4)
	builder.InsertString("name", "John Doe")
	builder.InsertInt("temperature", -15, 2)

	v, err := builder.Build()
	if err != nil {
		t.Fatalf("Failed to build map: %v", err)
	}

	// Serialize
	data, err := v.ToBuffer()
	if err != nil {
		t.Fatalf("Failed to serialize: %v", err)
	}

	// Deserialize
	v2, _, err := FromBuffer(data)
	if err != nil {
		t.Fatalf("Failed to deserialize: %v", err)
	}

	// Check values
	t.Run("Find age", func(t *testing.T) {
		val, err := v2.Find("age")
		if err != nil {
			t.Fatalf("Failed to find 'age': %v", err)
		}
		result := val.AsUInt32(0)
		if result != 42 {
			t.Errorf("Expected 42, got %d", result)
		}
	})

	t.Run("Find name", func(t *testing.T) {
		val, err := v2.Find("name")
		if err != nil {
			t.Fatalf("Failed to find 'name': %v", err)
		}
		result, err := val.AsString()
		if err != nil {
			t.Fatalf("Failed to get string: %v", err)
		}
		if result != "John Doe" {
			t.Errorf("Expected 'John Doe', got '%s'", result)
		}
	})

	t.Run("Find temperature", func(t *testing.T) {
		val, err := v2.Find("temperature")
		if err != nil {
			t.Fatalf("Failed to find 'temperature': %v", err)
		}
		result := val.AsInt16(0)
		if result != -15 {
			t.Errorf("Expected -15, got %d", result)
		}
	})

	t.Run("Find non-existent key", func(t *testing.T) {
		_, err := v2.Find("nonexistent")
		if err != ErrNotFound {
			t.Errorf("Expected ErrNotFound, got %v", err)
		}
	})
}

// Test list variants
func TestListVariants(t *testing.T) {
	// Build a list
	builder := NewListBuilder()
	builder.AppendUInt(100, 4)
	builder.AppendString("test")
	builder.AppendInt(-50, 2)

	v, err := builder.Build()
	if err != nil {
		t.Fatalf("Failed to build list: %v", err)
	}

	// Serialize
	data, err := v.ToBuffer()
	if err != nil {
		t.Fatalf("Failed to serialize: %v", err)
	}

	// Deserialize
	v2, _, err := FromBuffer(data)
	if err != nil {
		t.Fatalf("Failed to deserialize: %v", err)
	}

	// Check values using At
	t.Run("At position 0", func(t *testing.T) {
		val, err := v2.At(0)
		if err != nil {
			t.Fatalf("Failed to get at position 0: %v", err)
		}
		result := val.AsUInt32(0)
		if result != 100 {
			t.Errorf("Expected 100, got %d", result)
		}
	})

	t.Run("At position 1", func(t *testing.T) {
		val, err := v2.At(1)
		if err != nil {
			t.Fatalf("Failed to get at position 1: %v", err)
		}
		result, err := val.AsString()
		if err != nil {
			t.Fatalf("Failed to get string: %v", err)
		}
		if result != "test" {
			t.Errorf("Expected 'test', got '%s'", result)
		}
	})

	t.Run("At position 2", func(t *testing.T) {
		val, err := v2.At(2)
		if err != nil {
			t.Fatalf("Failed to get at position 2: %v", err)
		}
		result := val.AsInt16(0)
		if result != -50 {
			t.Errorf("Expected -50, got %d", result)
		}
	})

	// Check values using iterator
	t.Run("Iterator", func(t *testing.T) {
		it, err := v2.NewIterator()
		if err != nil {
			t.Fatalf("Failed to create iterator: %v", err)
		}

		// First item
		val, ok := it.Next()
		if !ok {
			t.Fatal("Expected first item")
		}
		if val.AsUInt32(0) != 100 {
			t.Errorf("Expected 100, got %d", val.AsUInt32(0))
		}

		// Second item
		val, ok = it.Next()
		if !ok {
			t.Fatal("Expected second item")
		}
		str, _ := val.AsString()
		if str != "test" {
			t.Errorf("Expected 'test', got '%s'", str)
		}

		// Third item
		val, ok = it.Next()
		if !ok {
			t.Fatal("Expected third item")
		}
		if val.AsInt16(0) != -50 {
			t.Errorf("Expected -50, got %d", val.AsInt16(0))
		}

		// No more items
		_, ok = it.Next()
		if ok {
			t.Error("Expected no more items")
		}
	})
}

// Test index variants
func TestIndexVariants(t *testing.T) {
	// Build an index
	builder := NewIndexBuilder()
	builder.AddUInt(10, 100, 4)
	builder.Add(20, NewString("test"))
	builder.AddUInt(30, 200, 4)

	v, err := builder.Build()
	if err != nil {
		t.Fatalf("Failed to build index: %v", err)
	}

	// Serialize
	data, err := v.ToBuffer()
	if err != nil {
		t.Fatalf("Failed to serialize: %v", err)
	}

	// Deserialize
	v2, _, err := FromBuffer(data)
	if err != nil {
		t.Fatalf("Failed to deserialize: %v", err)
	}

	// Check values
	t.Run("Get index 10", func(t *testing.T) {
		val, err := v2.Get(10)
		if err != nil {
			t.Fatalf("Failed to get index 10: %v", err)
		}
		result := val.AsUInt32(0)
		if result != 100 {
			t.Errorf("Expected 100, got %d", result)
		}
	})

	t.Run("Get index 20", func(t *testing.T) {
		val, err := v2.Get(20)
		if err != nil {
			t.Fatalf("Failed to get index 20: %v", err)
		}
		result, err := val.AsString()
		if err != nil {
			t.Fatalf("Failed to get string: %v", err)
		}
		if result != "test" {
			t.Errorf("Expected 'test', got '%s'", result)
		}
	})

	t.Run("Get index 30", func(t *testing.T) {
		val, err := v2.Get(30)
		if err != nil {
			t.Fatalf("Failed to get index 30: %v", err)
		}
		result := val.AsUInt32(0)
		if result != 200 {
			t.Errorf("Expected 200, got %d", result)
		}
	})

	t.Run("Get non-existent index", func(t *testing.T) {
		_, err := v2.Get(999)
		if err != ErrNotFound {
			t.Errorf("Expected ErrNotFound, got %v", err)
		}
	})
}

// Test nested variants
func TestNestedVariants(t *testing.T) {
	// Build inner list
	innerList := NewListBuilder()
	innerList.AppendUInt(1, 1)
	innerList.AppendUInt(2, 1)
	innerList.AppendUInt(3, 1)
	innerListVariant, err := innerList.Build()
	if err != nil {
		t.Fatalf("Failed to build inner list: %v", err)
	}

	// Build map with nested list
	builder := NewMapBuilder()
	builder.InsertString("name", "test")
	builder.Insert("numbers", innerListVariant)
	builder.InsertUInt("count", 3, 4)

	v, err := builder.Build()
	if err != nil {
		t.Fatalf("Failed to build map: %v", err)
	}

	// Serialize
	data, err := v.ToBuffer()
	if err != nil {
		t.Fatalf("Failed to serialize: %v", err)
	}

	// Deserialize
	v2, _, err := FromBuffer(data)
	if err != nil {
		t.Fatalf("Failed to deserialize: %v", err)
	}

	// Check nested list
	t.Run("Access nested list", func(t *testing.T) {
		list, err := v2.Find("numbers")
		if err != nil {
			t.Fatalf("Failed to find 'numbers': %v", err)
		}

		if list.Type() != VarTypeList {
			t.Errorf("Expected list type, got %d", list.Type())
		}

		// Check list items
		val0, err := list.At(0)
		if err != nil {
			t.Fatalf("Failed to get item 0: %v", err)
		}
		if val0.AsUInt8(0) != 1 {
			t.Errorf("Expected 1, got %d", val0.AsUInt8(0))
		}

		val1, err := list.At(1)
		if err != nil {
			t.Fatalf("Failed to get item 1: %v", err)
		}
		if val1.AsUInt8(0) != 2 {
			t.Errorf("Expected 2, got %d", val1.AsUInt8(0))
		}

		val2, err := list.At(2)
		if err != nil {
			t.Fatalf("Failed to get item 2: %v", err)
		}
		if val2.AsUInt8(0) != 3 {
			t.Errorf("Expected 3, got %d", val2.AsUInt8(0))
		}
	})
}

// Test packet format (with name prefix)
func TestPacketFormat(t *testing.T) {
	v := NewUInt(12345, 4)

	// Serialize to packet
	data, err := v.ToPacket("test_name")
	if err != nil {
		t.Fatalf("Failed to serialize to packet: %v", err)
	}

	// Deserialize from packet
	name, v2, bytesRead, err := FromPacket(data)
	if err != nil {
		t.Fatalf("Failed to deserialize from packet: %v", err)
	}

	if bytesRead != len(data) {
		t.Errorf("Expected to read %d bytes, got %d", len(data), bytesRead)
	}

	if name != "test_name" {
		t.Errorf("Expected name 'test_name', got '%s'", name)
	}

	result := v2.AsUInt32(0)
	if result != 12345 {
		t.Errorf("Expected 12345, got %d", result)
	}
}

// Test large data (variable length encoding)
func TestLargeData(t *testing.T) {
	tests := []struct {
		name string
		size int
	}{
		{"Small (< 255)", 100},
		{"Medium (> 255)", 300},
		{"Large (> 65535)", 70000},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			data := make([]byte, tt.size)
			for i := range data {
				data[i] = byte(i % 256)
			}

			v := NewBytes(data)

			// Serialize
			serialized, err := v.ToBuffer()
			if err != nil {
				t.Fatalf("Failed to serialize: %v", err)
			}

			// Deserialize
			v2, _, err := FromBuffer(serialized)
			if err != nil {
				t.Fatalf("Failed to deserialize: %v", err)
			}

			// Check data
			result, err := v2.AsBytes()
			if err != nil {
				t.Fatalf("Failed to get bytes: %v", err)
			}

			if len(result) != len(data) {
				t.Errorf("Expected length %d, got %d", len(data), len(result))
			}

			if !bytes.Equal(result, data) {
				t.Error("Data mismatch")
			}
		})
	}
}

// Test compatibility with C implementation encoding
func TestCCompatibility(t *testing.T) {
	t.Run("Simple UInt32", func(t *testing.T) {
		// Manually create what the C implementation would produce
		// Type: VAR_TYPE_UINT (0x0C) with VAR_LEN32 (0x80) = 0x8C
		// Value: 12345 (little endian)
		expected := []byte{0x8C, 0x39, 0x30, 0x00, 0x00}

		// Parse with our implementation
		v, bytesRead, err := FromBuffer(expected)
		if err != nil {
			t.Fatalf("Failed to parse: %v", err)
		}

		if bytesRead != len(expected) {
			t.Errorf("Expected to read %d bytes, got %d", len(expected), bytesRead)
		}

		result := v.AsUInt32(0)
		if result != 12345 {
			t.Errorf("Expected 12345, got %d", result)
		}

		// Now serialize and compare
		serialized, err := v.ToBuffer()
		if err != nil {
			t.Fatalf("Failed to serialize: %v", err)
		}

		if !bytes.Equal(serialized, expected) {
			t.Errorf("Serialization mismatch:\nExpected: %v\nGot:      %v", expected, serialized)
		}
	})

	t.Run("String with length field", func(t *testing.T) {
		// Type: VAR_TYPE_ASCII (0x08) with VAR_LEN_FIELD | VAR_LEN8 (0x20) = 0x28
		// Length: 5
		// Value: "Hello"
		expected := []byte{0x28, 0x05, 'H', 'e', 'l', 'l', 'o'}

		// Parse with our implementation
		v, bytesRead, err := FromBuffer(expected)
		if err != nil {
			t.Fatalf("Failed to parse: %v", err)
		}

		if bytesRead != len(expected) {
			t.Errorf("Expected to read %d bytes, got %d", len(expected), bytesRead)
		}

		result, err := v.AsString()
		if err != nil {
			t.Fatalf("Failed to get string: %v", err)
		}

		if result != "Hello" {
			t.Errorf("Expected 'Hello', got '%s'", result)
		}

		// Serialize and compare
		serialized, err := v.ToBuffer()
		if err != nil {
			t.Fatalf("Failed to serialize: %v", err)
		}

		if !bytes.Equal(serialized, expected) {
			t.Errorf("Serialization mismatch:\nExpected: %v\nGot:      %v", expected, serialized)
		}
	})

	t.Run("Map with two entries", func(t *testing.T) {
		// Manually create a map with two entries
		buf := &bytes.Buffer{}

		// Type: VAR_TYPE_MAP (0x01) with VAR_LEN_FIELD | VAR_LEN8 (0x20) = 0x21
		buf.WriteByte(0x21)

		// Calculate total length
		// Entry 1: "age" (len=1+3) + uint8 value (1+1) = 6
		// Entry 2: "name" (len=1+4) + string "Bob" (1+1+3) = 10
		// Total: 16 bytes
		buf.WriteByte(16)

		// Entry 1: age = 25
		buf.WriteByte(3) // name length
		buf.WriteString("age")
		buf.WriteByte(0x0C) // VAR_TYPE_UINT | VAR_LEN8
		buf.WriteByte(25)

		// Entry 2: name = "Bob"
		buf.WriteByte(4) // name length
		buf.WriteString("name")
		buf.WriteByte(0x28) // VAR_TYPE_ASCII | VAR_LEN_FIELD | VAR_LEN8
		buf.WriteByte(3)    // string length
		buf.WriteString("Bob")

		expected := buf.Bytes()

		// Parse with our implementation
		v, bytesRead, err := FromBuffer(expected)
		if err != nil {
			t.Fatalf("Failed to parse: %v", err)
		}

		if bytesRead != len(expected) {
			t.Errorf("Expected to read %d bytes, got %d", len(expected), bytesRead)
		}

		// Check age
		age, err := v.Find("age")
		if err != nil {
			t.Fatalf("Failed to find 'age': %v", err)
		}
		if age.AsUInt8(0) != 25 {
			t.Errorf("Expected age 25, got %d", age.AsUInt8(0))
		}

		// Check name
		name, err := v.Find("name")
		if err != nil {
			t.Fatalf("Failed to find 'name': %v", err)
		}
		nameStr, err := name.AsString()
		if err != nil {
			t.Fatalf("Failed to get name string: %v", err)
		}
		if nameStr != "Bob" {
			t.Errorf("Expected name 'Bob', got '%s'", nameStr)
		}
	})
}

// Benchmark tests
func BenchmarkSerializeInt(b *testing.B) {
	v := NewUInt(12345, 4)
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		v.ToBuffer()
	}
}

func BenchmarkDeserializeInt(b *testing.B) {
	v := NewUInt(12345, 4)
	data, _ := v.ToBuffer()
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		FromBuffer(data)
	}
}

func BenchmarkSerializeMap(b *testing.B) {
	builder := NewMapBuilder()
	builder.InsertUInt("age", 42, 4)
	builder.InsertString("name", "John Doe")
	builder.InsertInt("temperature", -15, 2)
	v, _ := builder.Build()

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		v.ToBuffer()
	}
}

func BenchmarkDeserializeMap(b *testing.B) {
	builder := NewMapBuilder()
	builder.InsertUInt("age", 42, 4)
	builder.InsertString("name", "John Doe")
	builder.InsertInt("temperature", -15, 2)
	v, _ := builder.Build()
	data, _ := v.ToBuffer()

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		FromBuffer(data)
	}
}

func BenchmarkMapFind(b *testing.B) {
	builder := NewMapBuilder()
	for i := 0; i < 10; i++ {
		builder.InsertUInt(string(rune('a'+i)), uint64(i), 4)
	}
	v, _ := builder.Build()

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		v.Find("e")
	}
}

// Helper function to create test binary data matching C implementation
func createCBinaryInt32(value int32) []byte {
	buf := &bytes.Buffer{}
	buf.WriteByte(VarTypeSInt | varLen32) // Type byte
	binary.Write(buf, binary.LittleEndian, value)
	return buf.Bytes()
}

func TestCBinaryCompatibility(t *testing.T) {
	t.Run("Int32 from C format", func(t *testing.T) {
		// Create binary data as C would create it
		cData := createCBinaryInt32(-12345)

		// Parse with Go implementation
		v, _, err := FromBuffer(cData)
		if err != nil {
			t.Fatalf("Failed to parse C binary: %v", err)
		}

		result := v.AsInt32(0)
		if result != -12345 {
			t.Errorf("Expected -12345, got %d", result)
		}
	})
}
