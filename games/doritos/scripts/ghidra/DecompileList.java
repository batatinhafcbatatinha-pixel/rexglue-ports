// Headless: create functions from pdata.txt (first run), then decompile the addresses given as script args
// (hex, comma separated) into D:/CodexBuilds/doritos/ghidra2/out/<addr>.c
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import java.io.*;
import java.nio.file.*;

public class DecompileList extends GhidraScript {
  @Override
  public void run() throws Exception {
    String[] args = getScriptArgs();
    Listing listing = currentProgram.getListing();
    if (currentProgram.getFunctionManager().getFunctionCount() < 1000) {
      for (String line : Files.readAllLines(Paths.get("D:/CodexBuilds/doritos/ghidra2/pdata.txt"))) {
        String[] p = line.trim().split(" ");
        Address a = toAddr(Long.parseLong(p[0].substring(2), 16));
        AddressSet body = new AddressSet(a, a.add(Long.parseLong(p[1]) - 1));
        new DisassembleCommand(body, body, true).applyTo(currentProgram, monitor);
      }
      for (String line : Files.readAllLines(Paths.get("D:/CodexBuilds/doritos/ghidra2/pdata.txt"))) {
        String[] p = line.trim().split(" ");
        Address a = toAddr(Long.parseLong(p[0].substring(2), 16));
        if (getFunctionAt(a) == null) new CreateFunctionCmd(a).applyTo(currentProgram, monitor);
      }
      println("functions: " + currentProgram.getFunctionManager().getFunctionCount());
    }
    if (args.length == 0) return;
    DecompInterface di = new DecompInterface();
    di.openProgram(currentProgram);
    new File("D:/CodexBuilds/doritos/ghidra2/out").mkdirs();
    for (String s : String.join(",", args).split(",")) {
      Address a = toAddr(Long.parseLong(s.replace("0x", ""), 16));
      Function f = getFunctionContaining(a);
      if (f == null) {
        // pdata start given directly: (re)create it, letting Ghidra find the body
        new DisassembleCommand(a, null, true).applyTo(currentProgram, monitor);
        new CreateFunctionCmd(a).applyTo(currentProgram, monitor);
        f = getFunctionAt(a);
      }
      if (f == null) { println("no function at " + s); continue; }
      DecompileResults r = di.decompileFunction(f, 120, monitor);
      String c = r.getDecompiledFunction() != null ? r.getDecompiledFunction().getC() : ("// failed: " + r.getErrorMessage());
      Files.writeString(Paths.get("D:/CodexBuilds/doritos/ghidra2/out/" + f.getEntryPoint() + ".c"), c);
      println("decompiled " + f.getEntryPoint());
    }
  }
}
