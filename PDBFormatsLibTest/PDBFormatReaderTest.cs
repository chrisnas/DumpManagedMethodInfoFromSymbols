using System;
using System.Collections.Generic;

namespace PDBFormatsLibTest
{
    /// <summary>
    /// A class for reading and processing PDB format information
    /// </summary>
    public class PDBFormatReaderTest
    {
        private string _filePath;
        private List<string> _records;

        /// <summary>
        /// Gets or sets the file path of the PDB file
        /// </summary>
        public string FilePath
        {
            get { return _filePath; }
            set { _filePath = value; }
        }

        /// <summary>
        /// Gets or sets the list of records read from the file
        /// </summary>
        public List<string> Records
        {
            get { return _records; }
            set { _records = value; }
        }

        /// <summary>
        /// Gets the number of records currently loaded
        /// </summary>
        public int RecordCount
        {
            get { return _records?.Count ?? 0; }
        }

        /// <summary>
        /// Constructor
        /// </summary>
        public PDBFormatReaderTest()
        {
            _records = new List<string>();
        }

        /// <summary>
        /// Constructor with file path
        /// </summary>
        /// <param name="filePath">The path to the PDB file</param>
        public PDBFormatReaderTest(string filePath) : this()
        {
            _filePath = filePath;
        }

        /// <summary>
        /// Initializes the reader with sample data
        /// </summary>
        public void Initialize()
        {
            _records.Clear();
            _records.Add("HEADER    Sample PDB Record");
            _records.Add("TITLE     PDB Format Library Demo");
            _records.Add("AUTHOR    PDBFormatsLib");
            Console.WriteLine($"Initialized with {_records.Count} sample records");
        }

        /// <summary>
        /// Adds a new record to the collection
        /// </summary>
        /// <param name="record">The record to add</param>
        public void AddRecord(string record)
        {
            if (string.IsNullOrEmpty(record))
            {
                throw new ArgumentException("Record cannot be null or empty", nameof(record));
            }
            _records.Add(record);
        }

        /// <summary>
        /// Gets all records as a formatted string
        /// </summary>
        /// <returns>Formatted string containing all records</returns>
        public string GetFormattedRecords()
        {
            if (_records.Count == 0)
            {
                return "No records available";
            }

            return string.Join(Environment.NewLine, _records);
        }

        /// <summary>
        /// Clears all records
        /// </summary>
        public void Clear()
        {
            _records.Clear();
            Console.WriteLine("All records cleared");
        }

        /// <summary>
        /// Gets statistics about the loaded records
        /// </summary>
        /// <returns>A string with statistics information</returns>
        public string GetStatistics()
        {
            return $"File: {(_filePath ?? "Not specified")}, Record Count: {RecordCount}";
        }
    }
}
