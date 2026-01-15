using System;
using PDBFormatsLibTest;

public class Program
{
    public static void Main()
    {
        Console.WriteLine("PDB Formats Console Application");
        Console.WriteLine("================================");
        Console.WriteLine();

        // Create an instance of the PDBFormatReader class
        PDBFormatReaderTest reader = new PDBFormatReaderTest("sample.pdb");

        // Initialize with sample data
        reader.Initialize();
        Console.WriteLine();

        // Display statistics
        Console.WriteLine("Statistics:");
        Console.WriteLine(reader.GetStatistics());
        Console.WriteLine();

        // Add a custom record
        Console.WriteLine("Adding a custom record...");
        reader.AddRecord("REMARK    Custom record added from ConsoleApp");
        Console.WriteLine($"New record count: {reader.RecordCount}");
        Console.WriteLine();

        // Display all records
        Console.WriteLine("All Records:");
        Console.WriteLine("------------");
        Console.WriteLine(reader.GetFormattedRecords());
        Console.WriteLine();

        // Test property access
        Console.WriteLine($"File Path Property: {reader.FilePath}");
        Console.WriteLine();

        // Clear records
        reader.Clear();
        Console.WriteLine($"Record count after clear: {reader.RecordCount}");
        Console.WriteLine();

        Console.WriteLine("Press any key to exit...");
        Console.ReadKey();
    }

}
