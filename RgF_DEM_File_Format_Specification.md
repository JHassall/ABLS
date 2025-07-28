# RgF DEM File Format Specification v1.0

## Overview

The RgF DEM (`.RgFdem`) file format is a proprietary, single-file Digital Elevation Model container designed specifically for agricultural applications. It provides optimized storage and transfer of DEM data between DEMBuilder and ABLS tractor software systems.

## File Structure

The `.RgFdem` file is a **ZIP-based container** with the following internal structure:

```
Farm_Field_ddmmyy.RgFdem (ZIP Archive)
├── metadata.json              # Complete metadata and parameters
├── elevation.dem              # Binary elevation data
├── coordinate_system.txt      # Projection and coordinate information  
└── README.txt                 # Human-readable documentation
```

## Filename Convention

**Format:** `{FarmName}_{FieldName}_{ddmmyy}.RgFdem`

**Examples:**
- `NorthField_Paddock1_240724.RgFdem` (July 24, 2024)
- `MyFarm_BackPaddock_250724.RgFdem` (July 25, 2024)

**Rules:**
- Farm and field names are sanitized (invalid filename characters removed)
- Date format is `ddmmyy` (day, month, year - 2 digits each)
- Extension is always `.RgFdem` (case-sensitive)

## File Contents Detail

### 1. metadata.json

**Format:** UTF-8 encoded JSON with indented formatting

**Structure:**
```json
{
  "Version": "1.0",
  "CreatedBy": "DEMBuilder",
  "CreatedDate": "2024-07-24T02:45:00.0000000Z",
  "FarmName": "MyFarm",
  "FieldName": "BackPaddock", 
  "ReferenceLatitude": -36.12345678,
  "ReferenceLongitude": 147.87654321,
  "Resolution": 0.25,
  "PixelsX": 1200,
  "PixelsY": 800,
  "MinElevation": 245.67,
  "MaxElevation": 267.89,
  "Bounds": {
    "Left": 0.0,
    "Bottom": 0.0,
    "Right": 300.0,
    "Top": 200.0
  },
  "TotalPoints": 960000,
  "ProjectionInfo": "AgOpenGPS Compatible Local Coordinate System",
  "IsCompressed": false,
  "CompressionType": "None",
  "CustomProperties": {
    "format_version": "1.0",
    "compatible_software": ["ABLS", "AgOpenGPS"],
    "transfer_optimized": true,
    "coordinate_system": "local_tangent_plane"
  }
}
```

**Field Descriptions:**
- `Version`: File format version (always "1.0")
- `CreatedBy`: Source application (always "DEMBuilder")
- `CreatedDate`: UTC timestamp of file creation
- `FarmName`: User-defined farm name
- `FieldName`: User-defined field name
- `ReferenceLatitude`: WGS84 latitude of local coordinate system origin (8 decimal places)
- `ReferenceLongitude`: WGS84 longitude of local coordinate system origin (8 decimal places)
- `Resolution`: Meters per pixel (typically 0.25)
- `PixelsX`: Number of columns in elevation grid
- `PixelsY`: Number of rows in elevation grid
- `MinElevation`: Minimum elevation value in meters
- `MaxElevation`: Maximum elevation value in meters
- `Bounds`: Local coordinate system bounds in meters
  - `Left`: Western boundary (X minimum)
  - `Right`: Eastern boundary (X maximum)
  - `Bottom`: Southern boundary (Y minimum)
  - `Top`: Northern boundary (Y maximum)
- `TotalPoints`: Total number of elevation points (PixelsX × PixelsY)
- `ProjectionInfo`: Coordinate system description
- `IsCompressed`: Boolean indicating if elevation data uses compression (true/false)
- `CompressionType`: Compression method used ("ZIP" if compressed, "None" if uncompressed)
- `CustomProperties`: Additional metadata for software compatibility

### 2. elevation.dem

**Format:** Binary file with little-endian byte order

**Structure:**
```
[4 bytes] Rows (int32)           # Number of rows (PixelsY)
[4 bytes] Columns (int32)        # Number of columns (PixelsX)
[4 bytes] Elevation[0,0] (float) # First elevation value
[4 bytes] Elevation[0,1] (float) # Second elevation value
...
[4 bytes] Elevation[R-1,C-1] (float) # Last elevation value
```

**Data Layout:**
- **Header:** 8 bytes total
  - Bytes 0-3: Number of rows (int32, little-endian)
  - Bytes 4-7: Number of columns (int32, little-endian)
- **Data:** 4 bytes per elevation point (float32, little-endian)
  - Row-major order: [row][column]
  - Origin at top-left (Northwest corner)
  - X increases eastward (columns)
  - Y increases southward (rows)

**Coordinate Mapping:**
```
Local X = Bounds.Left + (column * Resolution)
Local Y = Bounds.Top - (row * Resolution)
```

**Special Values:**
- `NaN` values represent no-data areas
- All elevation values are in meters above sea level

### 3. coordinate_system.txt

**Format:** Plain text with key=value pairs

**Content:**
```
# AgOpenGPS Compatible Coordinate System
Reference_Latitude=-36.12345678
Reference_Longitude=147.87654321
Resolution_Meters=0.250
Bounds_Left=0.000
Bounds_Right=300.000
Bounds_Bottom=0.000
Bounds_Top=200.000
Projection=Local_Tangent_Plane
Units=Meters
```

**Purpose:** Provides coordinate system information in a simple text format for easy parsing by ABLS software.

### 4. README.txt

**Format:** Plain text documentation

**Content:** Human-readable summary including:
- Farm and field names
- Creation timestamp
- Resolution and dimensions
- Elevation range
- File contents description
- Usage instructions

## Coordinate System

**Type:** Local Tangent Plane (AgOpenGPS Compatible)

**Origin:** Specified by `ReferenceLatitude` and `ReferenceLongitude` in WGS84

**Axes:**
- **X-axis:** Points East (positive = eastward)
- **Y-axis:** Points North (positive = northward)  
- **Z-axis:** Points Up (elevation above sea level)

**Units:** Meters

**Grid Layout:**
- Origin (0,0) corresponds to the reference lat/lon
- Grid bounds define the local coordinate extents
- Elevation data is stored in row-major order
- Row 0 = northernmost data (highest Y value)
- Column 0 = westernmost data (lowest X value)

## File Size and Performance

**Typical Sizes:**
- 100 hectares @ 0.25m resolution: ~15-25 MB
- 500 hectares @ 0.25m resolution: ~75-125 MB

**Compression Options:**
- **No Compression (Default):** Maximum data integrity, 100% preservation of elevation precision
- **ZIP Compression (Optional):** 60-80% size reduction using safe DEFLATE algorithm
- **User Selectable:** DEMBuilder allows users to choose compression level
- **Self-Describing:** Compression type stored in metadata for automatic ABLS detection

**Transfer Optimization:**
- Single file for easy USB/wireless transfer
- Binary elevation data for compact storage
- Structured metadata for fast parsing
- No external dependencies required
- Compression metadata ensures compatibility

## ABLS Integration Requirements

### File Validation
1. Verify `.RgFdem` extension
2. Confirm ZIP archive structure
3. Validate presence of all 4 required files
4. Parse and validate `metadata.json` structure

### Compression Detection and Handling

**CRITICAL:** ABLS must check compression metadata before processing elevation data.

**Step 1: Read Compression Metadata**
```csharp
// Parse metadata.json to determine compression type
var metadata = JsonSerializer.Deserialize<RgFdemMetadata>(metadataJson);
bool isCompressed = metadata.IsCompressed;
string compressionType = metadata.CompressionType; // "ZIP" or "None"
```

**Step 2: Handle ZIP Archive Appropriately**
- **Both compressed and uncompressed files use ZIP container format**
- **Difference is in the compression level applied to ZIP entries**
- **metadata.json is always readable (never compressed for compatibility)**

**Step 3: Process Elevation Data Based on Compression**
```csharp
if (isCompressed && compressionType == "ZIP")
{
    // ZIP entries use DEFLATE compression - .NET handles automatically
    using (var elevationEntry = archive.GetEntry("elevation.dem"))
    using (var stream = elevationEntry.Open()) // Auto-decompresses
    {
        // Process binary data normally
        ProcessElevationData(stream);
    }
}
else
{
    // No compression - standard ZIP entry reading
    using (var elevationEntry = archive.GetEntry("elevation.dem"))
    using (var stream = elevationEntry.Open())
    {
        // Process binary data normally
        ProcessElevationData(stream);
    }
}
```

**Key Points:**
- Same code path for both compressed and uncompressed files
- .NET ZipArchive automatically handles decompression
- No special decompression libraries required
- Compression is transparent to the elevation data processing logic

### Data Loading Process
1. **Extract ZIP archive** to temporary directory
2. **Read metadata.json** for dimensions and parameters
3. **Parse coordinate_system.txt** for projection info
4. **Load elevation.dem** binary data:
   ```csharp
   using (var reader = new BinaryReader(stream))
   {
       int rows = reader.ReadInt32();
       int cols = reader.ReadInt32();
       float[,] elevations = new float[rows, cols];
       
       for (int row = 0; row < rows; row++)
       {
           for (int col = 0; col < cols; col++)
           {
               elevations[row, col] = reader.ReadSingle();
           }
       }
   }
   ```

### Coordinate Conversion
Convert from local coordinates to GPS coordinates:
```csharp
// Local to GPS conversion (approximate for small areas)
double deltaX = localX; // meters east of reference
double deltaY = localY; // meters north of reference

double lat = referenceLat + (deltaY / 111320.0); // ~111.32km per degree latitude
double lon = referenceLon + (deltaX / (111320.0 * Math.Cos(referenceLat * Math.PI / 180.0)));
```

## Version History

**v1.0** (Current)
- Initial release
- ZIP-based container format
- Binary elevation data storage
- AgOpenGPS coordinate system compatibility
- ABLS software optimization

## Technical Notes

- **Endianness:** All binary data uses little-endian byte order
- **Floating Point:** IEEE 754 single precision (32-bit)
- **Character Encoding:** UTF-8 for all text files
- **Compression:** Standard ZIP compression (DEFLATE algorithm)
- **Compatibility:** Designed for Windows/.NET environments
- **Thread Safety:** Files should be read-only during processing

## Error Handling

**Common Issues:**
- Corrupted ZIP archive → Re-export from DEMBuilder
- Missing required files → File format validation failed
- Invalid metadata JSON → Parse error, check format version
- Binary data size mismatch → Verify rows×cols matches file size

**Validation Checksums:**
- Expected elevation.dem size: 8 + (rows × cols × 4) bytes
- Expected total points: rows × cols = metadata.TotalPoints

---

**Document Version:** 1.0  
**Last Updated:** July 24, 2024  
**Created By:** ABLS Development Team  
**For:** ABLS Software Integration
