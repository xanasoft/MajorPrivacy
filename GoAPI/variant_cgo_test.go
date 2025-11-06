//go:build cgo
// +build cgo

package goapi

import (
	"testing"
)

// TestCGO_IntegerSerialization tests that Go and C produce identical binary output for integers
func TestCGO_IntegerSerialization(t *testing.T) {
	tests := []struct {
		name     string
		value    uint64
		size     int
		expected uint64
	}{
		{"UInt8", 255, 1, 255},
		{"UInt16", 65535, 2, 65535},
		{"UInt32", 4294967295, 4, 4294967295},
		{"UInt64", 12345678901234, 8, 12345678901234},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			// Create variant with Go implementation
			goVariant := NewUInt(tt.value, tt.size)
			goData, err := goVariant.ToBuffer()
			if err != nil {
				t.Fatalf("Go serialization failed: %v", err)
			}

			// Create variant with C implementation
			cWrapper := NewCVariantWrapper(64)

			switch tt.size {
			case 1:
				cWrapper.FromUInt8(uint8(tt.value))
			case 2:
				cWrapper.FromUInt16(uint16(tt.value))
			case 4:
				cWrapper.FromUInt32(uint32(tt.value))
			case 8:
				cWrapper.FromUInt64(tt.value)
			}

			cData := cWrapper.ToBuffer()
			if cData == nil {
				t.Fatal("C serialization failed")
			}

			// Compare binary output
			if len(goData) != len(cData) {
				t.Errorf("Length mismatch: Go=%d, C=%d", len(goData), len(cData))
			}

			for i := 0; i < len(goData) && i < len(cData); i++ {
				if goData[i] != cData[i] {
					t.Errorf("Byte %d mismatch: Go=0x%02X, C=0x%02X", i, goData[i], cData[i])
				}
			}

			// Test Go can read C output
			goFromC, _, err := FromBuffer(cData)
			if err != nil {
				t.Fatalf("Go failed to parse C output: %v", err)
			}
			if goFromC.AsUInt64(0) != tt.expected {
				t.Errorf("Go reading C data: expected %d, got %d", tt.expected, goFromC.AsUInt64(0))
			}

			// Test C can read Go output
			cFromGo := NewCVariantWrapper(0)
			if !cFromGo.FromBuffer(goData) {
				t.Fatal("C failed to parse Go output")
			}

			cValue := cFromGo.AsUInt64()
			if cValue != tt.expected {
				t.Errorf("C reading Go data: expected %d, got %d", tt.expected, cValue)
			}
		})
	}
}

// TestCGO_SignedIntegerSerialization tests signed integers
func TestCGO_SignedIntegerSerialization(t *testing.T) {
	tests := []struct {
		name     string
		value    int64
		size     int
		expected int64
	}{
		{"Int8 positive", 127, 1, 127},
		{"Int8 negative", -128, 1, -128},
		{"Int16 positive", 32767, 2, 32767},
		{"Int16 negative", -32768, 2, -32768},
		{"Int32 positive", 2147483647, 4, 2147483647},
		{"Int32 negative", -2147483648, 4, -2147483648},
		{"Int64 positive", 9223372036854775807, 8, 9223372036854775807},
		{"Int64 negative", -9223372036854775808, 8, -9223372036854775808},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			// Create with Go
			goVariant := NewInt(tt.value, tt.size)
			goData, err := goVariant.ToBuffer()
			if err != nil {
				t.Fatalf("Go serialization failed: %v", err)
			}

			// Create with C
			cWrapper := NewCVariantWrapper(64)

			switch tt.size {
			case 1:
				cWrapper.FromSInt8(int8(tt.value))
			case 2:
				cWrapper.FromSInt16(int16(tt.value))
			case 4:
				cWrapper.FromSInt32(int32(tt.value))
			case 8:
				cWrapper.FromSInt64(tt.value)
			}

			cData := cWrapper.ToBuffer()
			if cData == nil {
				t.Fatal("C serialization failed")
			}

			// Compare binary output
			if len(goData) != len(cData) {
				t.Errorf("Length mismatch: Go=%d, C=%d", len(goData), len(cData))
			}

			for i := 0; i < len(goData) && i < len(cData); i++ {
				if goData[i] != cData[i] {
					t.Errorf("Byte %d mismatch: Go=0x%02X, C=0x%02X", i, goData[i], cData[i])
				}
			}

			// Cross-compatibility test
			goFromC, _, err := FromBuffer(cData)
			if err != nil {
				t.Fatalf("Go failed to parse C output: %v", err)
			}
			if goFromC.AsInt64(0) != tt.expected {
				t.Errorf("Go reading C data: expected %d, got %d", tt.expected, goFromC.AsInt64(0))
			}

			cFromGo := NewCVariantWrapper(0)
			if !cFromGo.FromBuffer(goData) {
				t.Fatal("C failed to parse Go output")
			}
			cValue := cFromGo.AsInt64()
			if cValue != tt.expected {
				t.Errorf("C reading Go data: expected %d, got %d", tt.expected, cValue)
			}
		})
	}
}

// TestCGO_StringSerialization tests string compatibility
func TestCGO_StringSerialization(t *testing.T) {
	tests := []struct {
		name  string
		value string
	}{
		{"Empty string", ""},
		{"Short string", "Hello"},
		{"Long string", "This is a longer string to test variable length encoding"},
		{"Special chars", "Hello, World! @#$%^&*()"},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			// Create with Go
			goVariant := NewString(tt.value)
			goData, err := goVariant.ToBuffer()
			if err != nil {
				t.Fatalf("Go serialization failed: %v", err)
			}

			// Create with C
			cWrapper := NewCVariantWrapper(1024)
			cWrapper.FromString(tt.value)
			cData := cWrapper.ToBuffer()
			if cData == nil {
				t.Fatal("C serialization failed")
			}

			// Compare binary output
			if len(goData) != len(cData) {
				t.Errorf("Length mismatch: Go=%d, C=%d", len(goData), len(cData))
			}

			for i := 0; i < len(goData) && i < len(cData); i++ {
				if goData[i] != cData[i] {
					t.Errorf("Byte %d mismatch: Go=0x%02X, C=0x%02X", i, goData[i], cData[i])
				}
			}

			// Test Go can read C output
			goFromC, _, err := FromBuffer(cData)
			if err != nil {
				t.Fatalf("Go failed to parse C output: %v", err)
			}
			goStr, err := goFromC.AsString()
			if err != nil {
				t.Fatalf("Failed to get string from Go: %v", err)
			}
			if goStr != tt.value {
				t.Errorf("Go reading C data: expected '%s', got '%s'", tt.value, goStr)
			}

			// Test C can read Go output
			cFromGo := NewCVariantWrapper(0)
			if !cFromGo.FromBuffer(goData) {
				t.Fatal("C failed to parse Go output")
			}

			cResult := cFromGo.AsString(len(tt.value) + 1)
			if cResult != tt.value {
				t.Errorf("C reading Go data: expected '%s', got '%s'", tt.value, cResult)
			}
		})
	}
}

// TestCGO_BytesSerialization tests byte array compatibility
func TestCGO_BytesSerialization(t *testing.T) {
	tests := []struct {
		name  string
		value []byte
	}{
		{"Empty bytes", []byte{}},
		{"Small bytes", []byte{0x01, 0x02, 0x03, 0x04}},
		{"Binary data", []byte{0x00, 0xFF, 0xAA, 0x55, 0x12, 0x34, 0x56, 0x78}},
		{"Large bytes", make([]byte, 300)},
	}

	// Initialize large bytes test data
	for i := range tests[3].value {
		tests[3].value[i] = byte(i % 256)
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			// Create with Go
			goVariant := NewBytes(tt.value)
			goData, err := goVariant.ToBuffer()
			if err != nil {
				t.Fatalf("Go serialization failed: %v", err)
			}

			// Create with C
			cWrapper := NewCVariantWrapper(2048)
			cWrapper.FromBytes(tt.value)
			cData := cWrapper.ToBuffer()
			if cData == nil {
				t.Fatal("C serialization failed")
			}

			// Compare binary output
			if len(goData) != len(cData) {
				t.Errorf("Length mismatch: Go=%d, C=%d", len(goData), len(cData))
			}

			for i := 0; i < len(goData) && i < len(cData); i++ {
				if goData[i] != cData[i] {
					t.Errorf("Byte %d mismatch: Go=0x%02X, C=0x%02X", i, goData[i], cData[i])
				}
			}

			// Test Go can read C output
			goFromC, _, err := FromBuffer(cData)
			if err != nil {
				t.Fatalf("Go failed to parse C output: %v", err)
			}
			goBytes, err := goFromC.AsBytes()
			if err != nil {
				t.Fatalf("Failed to get bytes from Go: %v", err)
			}
			if len(goBytes) != len(tt.value) {
				t.Errorf("Go reading C data: length mismatch, expected %d, got %d", len(tt.value), len(goBytes))
			}
			for i := range tt.value {
				if i < len(goBytes) && goBytes[i] != tt.value[i] {
					t.Errorf("Go reading C data: byte %d mismatch", i)
					break
				}
			}
		})
	}
}

// TestCGO_MapSerialization tests map/dictionary compatibility
func TestCGO_MapSerialization(t *testing.T) {
	t.Run("Simple map with integers", func(t *testing.T) {
		// Create map with Go
		goBuilder := NewMapBuilder()
		goBuilder.InsertUInt("age", 42, 4)
		goBuilder.InsertUInt("count", 100, 2)
		goMap, err := goBuilder.Build()
		if err != nil {
			t.Fatalf("Go map build failed: %v", err)
		}
		goData, err := goMap.ToBuffer()
		if err != nil {
			t.Fatalf("Go serialization failed: %v", err)
		}

		// Create map with C
		cWrapper := NewCVariantWrapper(1024)
		cWrapper.PrepareMap()
		cWrapper.InsertUInt32("age", 42)
		cWrapper.InsertUInt16("count", 100)
		cData := cWrapper.Finish()
		if cData == nil {
			t.Fatal("C serialization failed")
		}

		// Test Go can read C output
		goFromC, _, err := FromBuffer(cData)
		if err != nil {
			t.Fatalf("Go failed to parse C output: %v", err)
		}

		age, err := goFromC.Find("age")
		if err != nil {
			t.Fatalf("Go failed to find 'age': %v", err)
		}
		if age.AsUInt32(0) != 42 {
			t.Errorf("Expected age 42, got %d", age.AsUInt32(0))
		}

		count, err := goFromC.Find("count")
		if err != nil {
			t.Fatalf("Go failed to find 'count': %v", err)
		}
		if count.AsUInt16(0) != 100 {
			t.Errorf("Expected count 100, got %d", count.AsUInt16(0))
		}

		// Test C can read Go output
		cFromGo := NewCVariantWrapper(0)
		if !cFromGo.FromBuffer(goData) {
			t.Fatal("C failed to parse Go output")
		}

		cAgeValue := cFromGo.FindUInt32("age")
		if cAgeValue != 42 {
			t.Errorf("C reading Go data: expected age 42, got %d", cAgeValue)
		}

		cCountValue := cFromGo.FindUInt16("count")
		if cCountValue != 100 {
			t.Errorf("C reading Go data: expected count 100, got %d", cCountValue)
		}
	})

	t.Run("Map with string values", func(t *testing.T) {
		// Create map with Go
		goBuilder := NewMapBuilder()
		goBuilder.InsertString("name", "Alice")
		goBuilder.InsertString("city", "NewYork")
		goMap, err := goBuilder.Build()
		if err != nil {
			t.Fatalf("Go map build failed: %v", err)
		}
		goData, err := goMap.ToBuffer()
		if err != nil {
			t.Fatalf("Go serialization failed: %v", err)
		}

		// Test C can read Go output
		cFromGo := NewCVariantWrapper(0)
		if !cFromGo.FromBuffer(goData) {
			t.Fatal("C failed to parse Go output")
		}

		cName := cFromGo.FindString("name", 100)
		if cName != "Alice" {
			t.Errorf("Expected name 'Alice', got '%s'", cName)
		}

		cCity := cFromGo.FindString("city", 100)
		if cCity != "NewYork" {
			t.Errorf("Expected city 'NewYork', got '%s'", cCity)
		}
	})
}

// TestCGO_ListSerialization tests list compatibility
func TestCGO_ListSerialization(t *testing.T) {
	t.Run("Integer list", func(t *testing.T) {
		// Create list with Go
		goBuilder := NewListBuilder()
		goBuilder.AppendUInt(10, 1)
		goBuilder.AppendUInt(20, 1)
		goBuilder.AppendUInt(30, 1)
		goList, err := goBuilder.Build()
		if err != nil {
			t.Fatalf("Go list build failed: %v", err)
		}
		goData, err := goList.ToBuffer()
		if err != nil {
			t.Fatalf("Go serialization failed: %v", err)
		}

		// Create list with C
		cWrapper := NewCVariantWrapper(1024)
		cWrapper.PrepareList()
		cWrapper.AppendUInt8(10)
		cWrapper.AppendUInt8(20)
		cWrapper.AppendUInt8(30)
		cData := cWrapper.Finish()
		if cData == nil {
			t.Fatal("C serialization failed")
		}

		// Test Go can read C output
		goFromC, _, err := FromBuffer(cData)
		if err != nil {
			t.Fatalf("Go failed to parse C output: %v", err)
		}

		for i, expected := range []uint8{10, 20, 30} {
			val, err := goFromC.At(i)
			if err != nil {
				t.Fatalf("Go failed to get item %d: %v", i, err)
			}
			if val.AsUInt8(0) != expected {
				t.Errorf("Item %d: expected %d, got %d", i, expected, val.AsUInt8(0))
			}
		}

		// Test C can read Go output
		cFromGo := NewCVariantWrapper(0)
		if !cFromGo.FromBuffer(goData) {
			t.Fatal("C failed to parse Go output")
		}

		for i, expected := range []uint8{10, 20, 30} {
			cValue := cFromGo.AtUInt8(i)
			if cValue != expected {
				t.Errorf("C reading Go data, item %d: expected %d, got %d", i, expected, cValue)
			}
		}
	})
}

// TestCGO_PacketFormat tests the packet format with name prefix
func TestCGO_PacketFormat(t *testing.T) {
	name := "test_packet"
	value := uint32(12345)

	// Create packet with Go
	goVariant := NewUInt(uint64(value), 4)
	goData, err := goVariant.ToPacket(name)
	if err != nil {
		t.Fatalf("Go packet serialization failed: %v", err)
	}

	// Create packet with C
	cWrapper := NewCVariantWrapper(1024)
	cWrapper.FromUInt32(value)
	cData := cWrapper.ToPacket(name)
	if cData == nil {
		t.Fatal("C packet serialization failed")
	}

	// Compare binary output
	if len(goData) != len(cData) {
		t.Errorf("Length mismatch: Go=%d, C=%d", len(goData), len(cData))
	}

	for i := 0; i < len(goData) && i < len(cData); i++ {
		if goData[i] != cData[i] {
			t.Errorf("Byte %d mismatch: Go=0x%02X, C=0x%02X", i, goData[i], cData[i])
		}
	}

	// Test Go can read C packet
	goName, goVar, _, err := FromPacket(cData)
	if err != nil {
		t.Fatalf("Go failed to parse C packet: %v", err)
	}
	if goName != name {
		t.Errorf("Name mismatch: expected '%s', got '%s'", name, goName)
	}
	if goVar.AsUInt32(0) != value {
		t.Errorf("Value mismatch: expected %d, got %d", value, goVar.AsUInt32(0))
	}

	// Test C can read Go packet
	cFromGo := NewCVariantWrapper(0)
	cParsedName, ok := cFromGo.FromPacket(goData)
	if !ok {
		t.Fatal("C failed to parse Go packet")
	}

	if cParsedName != name {
		t.Errorf("C reading Go packet: name mismatch, expected '%s', got '%s'", name, cParsedName)
	}

	cParsedValue := cFromGo.AsUInt32()
	if cParsedValue != value {
		t.Errorf("C reading Go packet: value mismatch, expected %d, got %d", value, cParsedValue)
	}
}

// TestCGO_NestedStructures tests nested maps and lists
func TestCGO_NestedStructures(t *testing.T) {
	// Create nested structure with Go (map containing a list)
	goListBuilder := NewListBuilder()
	goListBuilder.AppendUInt(1, 1)
	goListBuilder.AppendUInt(2, 1)
	goListBuilder.AppendUInt(3, 1)
	goList, err := goListBuilder.Build()
	if err != nil {
		t.Fatalf("Failed to build list: %v", err)
	}

	goMapBuilder := NewMapBuilder()
	goMapBuilder.InsertString("name", "nested")
	goMapBuilder.Insert("numbers", goList)
	goMap, err := goMapBuilder.Build()
	if err != nil {
		t.Fatalf("Failed to build map: %v", err)
	}

	goData, err := goMap.ToBuffer()
	if err != nil {
		t.Fatalf("Go serialization failed: %v", err)
	}

	// Test C can read nested structure
	cFromGo := NewCVariantWrapper(0)
	if !cFromGo.FromBuffer(goData) {
		t.Fatal("C failed to parse Go data")
	}

	// Read the string field
	cName := cFromGo.FindString("name", 100)
	if cName != "nested" {
		t.Errorf("Expected 'nested', got '%s'", cName)
	}

	// Read the nested list
	cList := cFromGo.Find("numbers")
	if cList == nil {
		t.Fatal("C failed to find 'numbers' list")
	}

	// Verify list contents
	for i, expected := range []uint8{1, 2, 3} {
		val := cList.AtUInt8(i)
		if val != expected {
			t.Errorf("List item %d: expected %d, got %d", i, expected, val)
		}
	}
}

// TestCGO_LargeDataCompatibility tests large data with variable length encoding
func TestCGO_LargeDataCompatibility(t *testing.T) {
	// Test data larger than 255 bytes (requires 16-bit length field)
	largeData := make([]byte, 300)
	for i := range largeData {
		largeData[i] = byte(i % 256)
	}

	// Create with Go
	goVariant := NewBytes(largeData)
	goData, err := goVariant.ToBuffer()
	if err != nil {
		t.Fatalf("Go serialization failed: %v", err)
	}

	// Test C can read it
	cFromGo := NewCVariantWrapper(0)
	if !cFromGo.FromBuffer(goData) {
		t.Fatal("C failed to parse Go large data")
	}

	// Verify data size
	cSize := cFromGo.GetSize()
	if cSize != uint32(len(largeData)) {
		t.Errorf("Size mismatch: expected %d, got %d", len(largeData), cSize)
	}

	// Verify first and last bytes
	cData := cFromGo.GetData()
	if len(cData) > 0 && cData[0] != largeData[0] {
		t.Errorf("First byte mismatch: expected 0x%02X, got 0x%02X", largeData[0], cData[0])
	}
	if len(cData) >= len(largeData) && cData[len(largeData)-1] != largeData[len(largeData)-1] {
		t.Errorf("Last byte mismatch: expected 0x%02X, got 0x%02X", largeData[len(largeData)-1], cData[len(largeData)-1])
	}
}
