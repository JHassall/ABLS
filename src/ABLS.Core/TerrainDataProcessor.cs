using System;
using System.IO;
using System.IO.Compression;
using System.Text.Json;

namespace ABLS.Core
{
    /// <summary>
    /// Handles loading and processing of RgF DEM terrain data files for ABLS boom control
    /// </summary>
    public class TerrainDataProcessor
    {
        private DEMMetadata? _metadata;
        private float[,]? _elevationData;
        private bool _isLoaded = false;

        /// <summary>
        /// Gets whether a DEM file is currently loaded
        /// </summary>
        public bool IsLoaded => _isLoaded;

        /// <summary>
        /// Gets the loaded DEM metadata
        /// </summary>
        public DEMMetadata? Metadata => _metadata;

        /// <summary>
        /// Loads a RgF DEM file (.RgFdem format)
        /// </summary>
        /// <param name="demFilePath">Path to the .RgFdem file</param>
        /// <returns>True if loaded successfully, false otherwise</returns>
        public bool LoadDEMFile(string demFilePath)
        {
            try
            {
                if (!File.Exists(demFilePath))
                {
                    throw new FileNotFoundException($"DEM file not found: {demFilePath}");
                }

                using (var archive = ZipFile.OpenRead(demFilePath))
                {
                    // Load metadata
                    var metadataEntry = archive.GetEntry("metadata.json");
                    if (metadataEntry == null)
                    {
                        throw new InvalidDataException("metadata.json not found in DEM file");
                    }

                    using (var metadataStream = metadataEntry.Open())
                    using (var reader = new StreamReader(metadataStream))
                    {
                        var metadataJson = reader.ReadToEnd();
                        _metadata = JsonSerializer.Deserialize<DEMMetadata>(metadataJson);
                    }

                    if (_metadata == null)
                    {
                        throw new InvalidDataException("Failed to parse DEM metadata");
                    }

                    // Load elevation data
                    var elevationEntry = archive.GetEntry("elevation.dem");
                    if (elevationEntry == null)
                    {
                        throw new InvalidDataException("elevation.dem not found in DEM file");
                    }

                    using (var elevationStream = elevationEntry.Open())
                    using (var binaryReader = new BinaryReader(elevationStream))
                    {
                        // Read header (rows and columns)
                        int rows = binaryReader.ReadInt32();
                        int columns = binaryReader.ReadInt32();

                        if (rows != _metadata.Rows || columns != _metadata.Columns)
                        {
                            throw new InvalidDataException("DEM data dimensions don't match metadata");
                        }

                        // Read elevation data
                        _elevationData = new float[rows, columns];
                        for (int row = 0; row < rows; row++)
                        {
                            for (int col = 0; col < columns; col++)
                            {
                                _elevationData[row, col] = binaryReader.ReadSingle();
                            }
                        }
                    }
                }

                _isLoaded = true;
                return true;
            }
            catch (Exception ex)
            {
                // Log error (would integrate with AgOpenGPS logging)
                Console.WriteLine($"Error loading DEM file: {ex.Message}");
                _isLoaded = false;
                _metadata = null;
                _elevationData = null;
                return false;
            }
        }

        /// <summary>
        /// Gets elevation at specified latitude/longitude coordinates
        /// </summary>
        /// <param name="latitude">Latitude in decimal degrees</param>
        /// <param name="longitude">Longitude in decimal degrees</param>
        /// <returns>Elevation in meters, or null if coordinates are outside DEM bounds</returns>
        public double? GetElevationAt(double latitude, double longitude)
        {
            if (!_isLoaded || _metadata == null || _elevationData == null)
            {
                return null;
            }

            // Convert WGS84 coordinates to local coordinates
            double localX = (longitude - _metadata.ReferencePoint.Longitude) * 111320.0 * Math.Cos(latitude * Math.PI / 180.0);
            double localY = (latitude - _metadata.ReferencePoint.Latitude) * 111320.0;

            // Convert local coordinates to grid indices
            int col = (int)Math.Round((localX - _metadata.Bounds.Left) / _metadata.Resolution);
            int row = (int)Math.Round((_metadata.Bounds.Top - localY) / _metadata.Resolution);

            // Check bounds
            if (row < 0 || row >= _metadata.Rows || col < 0 || col >= _metadata.Columns)
            {
                return null;
            }

            return _elevationData[row, col];
        }

        /// <summary>
        /// Gets terrain profile for boom height calculation
        /// </summary>
        /// <param name="startLatitude">Starting latitude</param>
        /// <param name="startLongitude">Starting longitude</param>
        /// <param name="endLatitude">Ending latitude</param>
        /// <param name="endLongitude">Ending longitude</param>
        /// <param name="sampleCount">Number of elevation samples along the path</param>
        /// <returns>Array of elevation values along the path</returns>
        public double[] GetTerrainProfile(double startLatitude, double startLongitude, 
                                        double endLatitude, double endLongitude, int sampleCount = 10)
        {
            var profile = new double[sampleCount];
            
            for (int i = 0; i < sampleCount; i++)
            {
                double t = (double)i / (sampleCount - 1);
                double lat = startLatitude + t * (endLatitude - startLatitude);
                double lon = startLongitude + t * (endLongitude - startLongitude);
                
                profile[i] = GetElevationAt(lat, lon) ?? 0.0;
            }

            return profile;
        }

        /// <summary>
        /// Clears loaded DEM data
        /// </summary>
        public void ClearDEM()
        {
            _isLoaded = false;
            _metadata = null;
            _elevationData = null;
        }
    }

    /// <summary>
    /// DEM metadata structure matching RgF DEM format
    /// </summary>
    public class DEMMetadata
    {
        public string FarmName { get; set; } = "";
        public string FieldName { get; set; } = "";
        public DateTime CreatedDate { get; set; }
        public double Resolution { get; set; } // meters per pixel
        public int Rows { get; set; }
        public int Columns { get; set; }
        public ReferencePoint ReferencePoint { get; set; } = new();
        public BoundingBox Bounds { get; set; } = new();
        public string CoordinateSystem { get; set; } = "";
    }

    public class ReferencePoint
    {
        public double Latitude { get; set; }
        public double Longitude { get; set; }
    }

    public class BoundingBox
    {
        public double Left { get; set; }
        public double Top { get; set; }
        public double Right { get; set; }
        public double Bottom { get; set; }
    }
}
