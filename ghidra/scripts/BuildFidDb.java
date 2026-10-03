// Build a Function ID database from static COFF libraries (VC6 CRT/STL + the game's JPEG.lib).
// Headless only. Imports every COFF member of each .lib into /fidlibs/<variant>/ of the current
// project, auto-analyzes it, then populates one FidDb library per .lib.
//
// Script args: <out.fidb> <common_symbols.txt|-> <family>#<version>#<variant>#<path.lib> ...
// Run via re/tools/build_fiddb.py (it supplies the arg list and a dummy -process program).
//@category FunctionID
import java.io.*;
import java.util.*;

import ghidra.app.script.GhidraScript;
import ghidra.app.util.bin.*;
import ghidra.app.util.bin.format.coff.*;
import ghidra.app.util.bin.format.coff.archive.CoffArchiveHeader;
import ghidra.app.util.bin.format.coff.archive.CoffArchiveMemberHeader;
import ghidra.app.util.importer.MessageLog;
import ghidra.app.util.importer.ProgramLoader;
import ghidra.app.util.opinion.*;
import ghidra.feature.fid.db.*;
import ghidra.feature.fid.service.*;
import ghidra.framework.model.*;
import ghidra.program.model.lang.LanguageID;
import ghidra.program.model.listing.Program;
import ghidra.util.task.TaskMonitor;

public class BuildFidDb extends GhidraScript {

	private static final String ROOT = "/fidlibs";

	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		File out = new File(args[0]);
		List<String> common = null;
		if (!args[1].equals("-")) {
			common = new ArrayList<>();
			for (String l : java.nio.file.Files.readAllLines(new File(args[1]).toPath())) {
				if (!l.isBlank()) common.add(l.trim());
			}
		}
		DomainFolder root = state.getProject().getProjectData().getRootFolder();
		DomainFolder libsRoot = root.getFolder("fidlibs");
		if (libsRoot == null) libsRoot = root.createFolder("fidlibs");

		List<String[]> libs = new ArrayList<>();
		for (int i = 2; i < args.length; i++) libs.add(args[i].split("#"));

		for (String[] lib : libs) {
			DomainFolder f = libsRoot.getFolder(lib[2]);
			if (f != null && f.getFiles().length > 0) {
				println("SKIP-IMPORT " + lib[2] + " (" + f.getFiles().length + " programs already)");
				continue;
			}
			if (f == null) f = libsRoot.createFolder(lib[2]);
			importAndAnalyze(f, new File(lib[3]));
		}

		if (out.exists()) out.delete();
		FidFileManager fm = FidFileManager.getInstance();
		fm.createNewFidDatabase(out);
		FidFile ff = fm.addUserFidFile(out);
		FidService service = new FidService();
		try (FidDB db = ff.getFidDB(true)) {
			for (String[] lib : libs) {
				List<DomainFile> programs = new ArrayList<>();
				for (DomainFile df : libsRoot.getFolder(lib[2]).getFiles()) programs.add(df);
				FidPopulateResult r = service.createNewLibraryFromPrograms(db, lib[0], lib[1], lib[2],
					programs, null, new LanguageID("x86:LE:32:default"), null, common, monitor);
				if (r == null) { println("LIB " + lib[2] + " EMPTY (nothing populated)"); continue; }
				println(String.format("LIB %s|%s|%s programs=%d attempted=%d added=%d excluded=%d failures=%s",
					lib[0], lib[1], lib[2], programs.size(), r.getTotalAttempted(), r.getTotalAdded(),
					r.getTotalExcluded(), r.getFailures()));
			}
			db.saveDatabase("BuildFidDb", monitor);
		}
		finally {
			fm.removeUserFile(ff); // do not leave it registered in the user's Ghidra preferences
		}
		println("FIDDB-WRITTEN " + out.getAbsolutePath() + " " + out.length());
	}

	private void importAndAnalyze(DomainFolder dest, File libFile) throws Exception {
		MessageLog log = new MessageLog();
		Set<String> used = new HashSet<>();
		int n = 0;
		try (RandomAccessByteProvider provider = new RandomAccessByteProvider(libFile)) {
			if (!CoffArchiveHeader.isMatch(provider)) throw new IOException("not a COFF archive: " + libFile);
			CoffArchiveHeader hdr = CoffArchiveHeader.read(provider, TaskMonitor.DUMMY);
			Set<Long> seen = new HashSet<>();
			for (CoffArchiveMemberHeader m : hdr.getArchiveMemberHeaders()) {
				monitor.checkCancelled();
				if (!seen.add(m.getPayloadOffset()) || !m.isCOFF()) continue;
				try (ByteProvider coff = new ByteProviderWrapper(provider, m.getPayloadOffset(), m.getSize())) {
					CoffFileHeader ch = new CoffFileHeader(coff);
					if (!CoffMachineType.isMachineTypeDefined(ch.getMagic())) continue;
					String name = uniqueName(m.getName(), used);
					try (LoadResults<Program> lr = ProgramLoader.builder()
							.source(coff)
							.project(state.getProject())
							.projectFolderPath(dest.getPathname())
							// JPEG.lib objects carry no .drectve/.debug$S, which MSCoffLoader requires
							.loaders(List.of(MSCoffLoader.class, CoffLoader.class))
							.language("x86:LE:32:default")
							.compiler("windows")
							.name(name)
							.log(log)
							.monitor(monitor)
							.load()) {
						for (Loaded<Program> loaded : lr) {
							Program p = loaded.getDomainObject(this);
							try {
								int tx = p.startTransaction("analyze");
								try { analyzeAll(p); } finally { p.endTransaction(tx, true); }
								loaded.save(monitor);
								n++;
							}
							finally {
								p.release(this);
							}
						}
					}
				}
				catch (Exception e) {
					println("MEMBER-FAIL " + libFile.getName() + " " + m.getName() + " " + e);
				}
			}
		}
		println("IMPORTED " + libFile.getName() + " -> " + dest.getPathname() + " programs=" + n);
	}

	private static String uniqueName(String member, Set<String> used) {
		String s = member.replace('\\', '/');
		s = s.substring(s.lastIndexOf('/') + 1).replaceAll("[^A-Za-z0-9_.-]", "_");
		if (s.isEmpty()) s = "member";
		String u = s;
		for (int i = 2; !used.add(u.toLowerCase()); i++) u = s + "_" + i;
		return u;
	}
}
