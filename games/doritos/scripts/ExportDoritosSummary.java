import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.listing.Program;
import ghidra.program.model.symbol.ExternalLocation;
import ghidra.program.model.symbol.ExternalManager;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;
import ghidra.program.model.symbol.SymbolTable;

import java.io.PrintWriter;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;

public class ExportDoritosSummary extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] arguments = getScriptArgs();
        if (arguments == null || arguments.length == 0 || arguments[0].isBlank()) {
            throw new IllegalArgumentException("output directory is required");
        }
        Path directory = Paths.get(arguments[0]);
        Files.createDirectories(directory);
        Program program = currentProgram;

        try (PrintWriter summary = new PrintWriter(Files.newBufferedWriter(directory.resolve("ghidra-summary.txt")))) {
            summary.printf("program=%s%n", program.getName());
            summary.printf("image_base=%s%n", program.getImageBase());
            summary.printf("entry=%s%n", program.getImageBase().add(program.getExecutableFormat().equals("Portable Executable (PE)") ? 0 : 0));
            int blocks = 0;
            for (MemoryBlock block : program.getMemory().getBlocks()) {
                blocks++;
                summary.printf("segment=%s,%s,%s,%s%n", block.getName(), block.getStart(), block.getEnd(), block.isExecute());
            }
            summary.printf("segment_count=%d%n", blocks);

            int functions = 0;
            try (PrintWriter functionFile = new PrintWriter(Files.newBufferedWriter(directory.resolve("functions.csv")))) {
                functionFile.println("entry,name,body_start,body_end");
                FunctionIterator iterator = program.getFunctionManager().getFunctions(true);
                while (iterator.hasNext()) {
                    Function function = iterator.next();
                    functions++;
                    functionFile.printf("%s,%s,%s,%s%n",
                        function.getEntryPoint(), function.getName(),
                        function.getBody().getMinAddress(), function.getBody().getMaxAddress());
                }
            }
            summary.printf("function_count=%d%n", functions);

            try (PrintWriter imports = new PrintWriter(Files.newBufferedWriter(directory.resolve("imports.csv")))) {
                imports.println("library,name,address");
                ExternalManager external = program.getExternalManager();
                for (String library : external.getExternalLibraryNames()) {
                    var locations = external.getExternalLocations(library);
                    while (locations.hasNext()) {
                        ExternalLocation location = locations.next();
                        imports.printf("%s,%s,%s%n", library, location.getLabel(), location.getAddress());
                    }
                }
            }

            try (PrintWriter strings = new PrintWriter(Files.newBufferedWriter(directory.resolve("strings.csv")))) {
                strings.println("address,value");
                SymbolTable symbols = program.getSymbolTable();
                SymbolIterator iterator = symbols.getAllSymbols(true);
                while (iterator.hasNext()) {
                    Symbol symbol = iterator.next();
                    if (symbol.getSource() != null && symbol.getName().startsWith("s_")) {
                        Address address = symbol.getAddress();
                        strings.printf("%s,%s%n", address, symbol.getName());
                    }
                }
            }
        }
    }
}
