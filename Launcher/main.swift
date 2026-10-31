import AppKit
import SwiftUI
import Foundation
import Darwin

struct Paths: Codable {
    var plugin = ""
    var fgRoot = ""
    var altitude = ""
    var flightGear = ""
    var interface = "en0"
}

@MainActor final class LauncherModel: ObservableObject {
    @Published var paths: Paths = Paths() { didSet { save() } }
    @Published var message = "Choisissez les quatre emplacements, puis cliquez sur Démarrer."
    @Published var running = false
    @Published var busy = false
    @Published var diagnostics = false
    private var fgApplication: NSRunningApplication?
    private var bridgeProcess: Process?
    private var altitudeProcess: NSRunningApplication?
    private var sequence = UUID()
    private let fm = FileManager.default
    private var appDir: URL { fm.homeDirectoryForCurrentUser.appendingPathComponent("Library/Application Support/IVAOFlightGearLauncher", isDirectory: true) }
    private var configURL: URL { appDir.appendingPathComponent("settings.json") }
    private var workDir: URL { appDir.appendingPathComponent("bridge", isDirectory: true) }
    private var logsDir: URL { appDir.appendingPathComponent("logs", isDirectory: true) }
    private var launcherLog: URL { logsDir.appendingPathComponent("launcher.log") }
    private var buildLog: URL { logsDir.appendingPathComponent("build.log") }
    private var bridgeLog: URL { logsDir.appendingPathComponent("bridge.log") }
    var diagnosticsPath: String { logsDir.path }
    func revealLogs() {
        try? fm.createDirectory(at: logsDir, withIntermediateDirectories: true)
        NSWorkspace.shared.open(logsDir)
    }
    private func record(_ text: String) {
        try? fm.createDirectory(at: logsDir, withIntermediateDirectories: true)
        let line = "[\(ISO8601DateFormatter().string(from: Date()))] \(text)\n"
        if !fm.fileExists(atPath: launcherLog.path) { _ = fm.createFile(atPath: launcherLog.path, contents: nil) }
        if let fh = try? FileHandle(forWritingTo: launcherLog) {
            defer { try? fh.close() }
            _ = try? fh.seekToEnd()
            try? fh.write(contentsOf: Data(line.utf8))
        }
    }
    private func setStatus(_ status: String) {
        message = status
        record(status)
    }

    init() {
        if let data = try? Data(contentsOf: configURL), let saved = try? JSONDecoder().decode(Paths.self, from: data) { paths = saved }
    }
    private func save() {
        try? fm.createDirectory(at: appDir, withIntermediateDirectories: true)
        if let bytes = try? JSONEncoder().encode(paths) { try? bytes.write(to: configURL, options: .atomic) }
    }
    func choose(_ kind: String) {
        let panel = NSOpenPanel(); panel.canChooseDirectories = true; panel.canChooseFiles = true
        panel.allowsMultipleSelection = false; panel.canCreateDirectories = false
        panel.prompt = "Sélectionner"
        switch kind {
        case "plugin": panel.message = "Choisir le fichier ivao_pilot.xpl (mac_x64)"; panel.canChooseDirectories = false
        case "fgRoot": panel.message = "Choisir le dossier FGData contenant Protocol et AI"; panel.canChooseFiles = false
        case "altitude": panel.message = "Choisir Altitude.app"; panel.canChooseDirectories = false; panel.canChooseFiles = true; panel.treatsFilePackagesAsDirectories = false
        default: panel.message = "Choisir FlightGear.app"; panel.canChooseDirectories = false; panel.canChooseFiles = true; panel.treatsFilePackagesAsDirectories = false
        }
        if panel.runModal() == .OK, let url = panel.url {
            switch kind {
            case "plugin": paths.plugin = url.path
            case "fgRoot": paths.fgRoot = url.path
            case "altitude": paths.altitude = url.path
            default: paths.flightGear = url.path
            }
        }
    }
    private func validate() throws {
        guard fm.fileExists(atPath: paths.plugin), paths.plugin.hasSuffix(".xpl") else { throw LaunchError.message("Sélectionne un fichier .xpl valide.") }
        guard fm.fileExists(atPath: paths.fgRoot + "/AI"), fm.fileExists(atPath: paths.fgRoot + "/Protocol") else { throw LaunchError.message("FGRoot ne contient pas les dossiers AI et Protocol.") }
        _ = try verifiedApp(at: paths.altitude, name: "Altitude")
        _ = try verifiedApp(at: paths.flightGear, name: "FlightGear")
        guard !paths.interface.isEmpty, paths.interface.allSatisfy({ $0.isLetter || $0.isNumber }) else { throw LaunchError.message("Interface réseau invalide.") }
    }
    // Respect the macOS app bundle: CFBundleExecutable is resolved by Bundle,
    // and LaunchServices / NSWorkspace launches the app (never a guessed binary).
    private func verifiedApp(at path: String, name: String) throws -> URL {
        let url = URL(fileURLWithPath: path, isDirectory: true)
        guard url.pathExtension.lowercased() == "app",
              let bundle = Bundle(url: url),
              let executableName = bundle.object(forInfoDictionaryKey: "CFBundleExecutable") as? String,
              !executableName.isEmpty,
              let executableURL = bundle.executableURL,
              fm.isExecutableFile(atPath: executableURL.path) else {
            throw LaunchError.message("Application \(name) invalide : impossible de résoudre CFBundleExecutable depuis Contents/Info.plist.")
        }
        return url
    }
    private func bundledBridge() throws -> URL {
        guard let resources = Bundle.main.resourceURL else { throw LaunchError.message("Ressources du lanceur introuvables.") }
        let dir = resources.appendingPathComponent("Bridge", isDirectory: true)
        guard fm.fileExists(atPath: dir.appendingPathComponent("build_macos.sh").path) else { throw LaunchError.message("Sources du bridge absentes du bundle.") }
        return dir
    }
    private func setup() throws -> URL {
        let source = try bundledBridge()
        try fm.createDirectory(at: appDir, withIntermediateDirectories: true)
        // Refresh bridge sources when upgrading the launcher; preserve user settings/logs.
        if fm.fileExists(atPath: workDir.path) { try fm.removeItem(at: workDir) }
        try fm.copyItem(at: source, to: workDir)
        let fgProtocol = URL(fileURLWithPath: paths.fgRoot).appendingPathComponent("Protocol/fg-altitude29.xml")
        let sourceProtocol = source.appendingPathComponent("fg-altitude29.xml")
        if !fm.fileExists(atPath: fgProtocol.path) {
            do { try fm.copyItem(at: sourceProtocol, to: fgProtocol) }
            catch { throw LaunchError.message("Impossible d'installer fg-altitude29.xml dans FGRoot/Protocol. Vérifie les droits d'écriture : \(error.localizedDescription)") }
        } else {
            let existing = try Data(contentsOf: fgProtocol)
            let expected = try Data(contentsOf: sourceProtocol)
            guard existing == expected else { throw LaunchError.message("FGRoot/Protocol/fg-altitude29.xml existe avec un contenu différent. Sauvegarde ou renomme-le avant de lancer.") }
        }
        return workDir
    }
    private func process(_ executable: String, _ args: [String], in folder: URL? = nil, debug: Bool = false) throws -> Process {
        let p = Process(); p.executableURL = URL(fileURLWithPath: executable); p.arguments = args
        p.currentDirectoryURL = folder
        if debug {
            try fm.createDirectory(at: logsDir, withIntermediateDirectories: true)
            let target = folder == workDir && executable == "/bin/sh" ? buildLog : bridgeLog
            if !fm.fileExists(atPath: target.path) { _ = fm.createFile(atPath: target.path, contents: nil) }
            let handle = try FileHandle(forWritingTo: target)
            _ = try handle.seekToEnd()
            p.standardOutput = handle
            p.standardError = handle
        } else { p.standardOutput = FileHandle.nullDevice; p.standardError = FileHandle.nullDevice }
        try p.run(); return p
    }
    func start() {
        guard !busy && !running else { return }
        do { try validate() } catch { message = error.localizedDescription; return }
        busy = true; let token = UUID(); sequence = token
        setStatus("Préparation du protocole FlightGear…")
        Task {
            do {
                let work = try setup()
                let pluginCopy = work.appendingPathComponent("ivao_pilot_test.xpl")
                let bridgeExe = work.appendingPathComponent("altitude_host")
                let needsBuild = !fm.fileExists(atPath: bridgeExe.path) || !fm.fileExists(atPath: pluginCopy.path) ||
                    ((try? Data(contentsOf: URL(fileURLWithPath: paths.plugin))) != (try? Data(contentsOf: pluginCopy)))
                if needsBuild {
                    setStatus("Compilation locale du bridge (premier lancement)…")
                    let build = try process("/bin/sh", [work.appendingPathComponent("build_macos.sh").path, paths.plugin], in: work, debug: diagnostics)
                    let exitCode = await withCheckedContinuation { (continuation: CheckedContinuation<Int32, Never>) in
                        DispatchQueue.global(qos: .userInitiated).async {
                            build.waitUntilExit()
                            continuation.resume(returning: build.terminationStatus)
                        }
                    }
                    guard exitCode == 0 else { throw LaunchError.message("Compilation du bridge échouée (code \(exitCode)). Active Diagnostics et consulte build.log.") }
                }
                guard sequence == token else { return }
                setStatus("Ouverture du launcher FlightGear…")
                // Two local-only property protocol sockets; generic UDP uses the bundled format.
                let fgURL = try verifiedApp(at: paths.flightGear, name: "FlightGear")
                let fgConfiguration = NSWorkspace.OpenConfiguration()
                fgConfiguration.arguments = [
                    "--launcher", "--fg-root=\(paths.fgRoot)", "--disable-ai-traffic", "--allow-nasal-from-sockets",
                    "--telnet=socket,in,10,127.0.0.1,5502,tcp",
                    "--telnet=socket,in,10,127.0.0.1,5503,tcp",
                    "--generic=socket,out,20,127.0.0.1,5501,udp,fg-altitude29"
                ]
                fgApplication = try await NSWorkspace.shared.openApplication(at: fgURL, configuration: fgConfiguration)
                setStatus("Choisis ton avion et clique sur Fly! dans FlightGear. Attente du port 5503…")
                let ready = await waitForPort(5503, token: token, limitSeconds: 600)
                guard ready, sequence == token else { throw LaunchError.message("FlightGear n'a pas démarré le port 5503, ou démarrage annulé.") }
                setStatus("Port 5503 disponible. Lancement du bridge…")
                // Continuous service mode: no HOLD and no timeout.
                let flags = [pluginCopy.path, "--run", "--traffic-interface", paths.interface, "--fg-ai", "--fg-ai-port", "5503"]
                bridgeProcess = try process(bridgeExe.path, flags + (diagnostics ? ["--log-file", bridgeLog.path] : []), in: work, debug: false)
                try await Task.sleep(nanoseconds: 1_000_000_000)
                guard bridgeProcess?.isRunning == true else {
                    let code = bridgeProcess?.terminationStatus ?? -1
                    throw LaunchError.message("Bridge arrêté immédiatement (code \(code)). Consulte bridge.log avec Diagnostics activé.")
                }
                setStatus("Ouverture d'Altitude…")
                let appURL = try verifiedApp(at: paths.altitude, name: "Altitude")
                let cfg = NSWorkspace.OpenConfiguration()
                altitudeProcess = try await NSWorkspace.shared.openApplication(at: appURL, configuration: cfg)
                running = true; busy = false
                setStatus("FlightGear, bridge et Altitude lancés.")
            } catch {
                if sequence == token { setStatus("ERREUR : " + error.localizedDescription); busy = false; stopProcesses() }
            }
        }
    }
    private func waitForPort(_ port: UInt16, token: UUID, limitSeconds: Int) async -> Bool {
        for _ in 0..<limitSeconds {
            if sequence != token { return false }
            // FlightGear's graphical launcher can be a different process from fgfs;
            // it is not safe to abort solely on NSRunningApplication.isTerminated.
            let socketFD = socket(AF_INET, SOCK_STREAM, 0)
            if socketFD >= 0 {
                var addr = sockaddr_in(); addr.sin_len = UInt8(MemoryLayout<sockaddr_in>.size)
                addr.sin_family = sa_family_t(AF_INET); addr.sin_port = port.bigEndian
                _ = "127.0.0.1".withCString { inet_pton(AF_INET, $0, &addr.sin_addr) }
                let success = withUnsafePointer(to: &addr) { ptr in
                    ptr.withMemoryRebound(to: sockaddr.self, capacity: 1) { connect(socketFD, $0, socklen_t(MemoryLayout<sockaddr_in>.size)) == 0 }
                }
                close(socketFD)
                if success { return true }
            }
            try? await Task.sleep(nanoseconds: 1_000_000_000)
        }
        return false
    }
    private func stopProcesses() {
        if bridgeProcess?.isRunning == true { bridgeProcess?.terminate() }
        bridgeProcess = nil
        // Altitude can have unsaved state; do not force terminate it.
        altitudeProcess = nil
        // FlightGear can be in an active flight; do not force terminate it.
        fgApplication = nil
        running = false
    }
    func stop() {
        sequence = UUID()
        stopProcesses(); busy = false
        setStatus("Bridge arrêté. FlightGear et Altitude restent ouverts.")
    }
}

enum LaunchError: LocalizedError {
    case message(String)
    var errorDescription: String? { if case .message(let text) = self { return text }; return nil }
}

struct LocationField: View {
    let title: String; let path: String; let click: () -> Void
    var body: some View {
        VStack(alignment: .leading, spacing: 5) {
            Text(title).font(.callout).fontWeight(.semibold)
            HStack {
                Text(path.isEmpty ? "Aucun emplacement sélectionné" : path)
                    .foregroundColor(path.isEmpty ? .secondary : .primary)
                    .lineLimit(1).truncationMode(.middle)
                    .frame(maxWidth: .infinity, alignment: .leading)
                Button("Parcourir…", action: click)
            }
            .padding(9)
            .background(Color(NSColor.controlBackgroundColor))
            .cornerRadius(8)
        }
    }
}

struct ContentView: View {
    @StateObject private var model = LauncherModel()
    var body: some View {
        VStack(alignment: .leading, spacing: 18) {
            HStack(spacing: 12) {
                Image(systemName: "airplane.circle.fill").font(.system(size: 38)).foregroundColor(.accentColor)
                VStack(alignment: .leading, spacing: 3) {
                    Text("IVAO FlightGear Launcher").font(.title2).fontWeight(.bold)
                    Text("FlightGear · Bridge · IVAO Altitude").foregroundColor(.secondary)
                }
            }
            Divider()
            LocationField(title: "Plugin IVAO pour X-Plane (.xpl)", path: model.paths.plugin) { model.choose("plugin") }
            LocationField(title: "Dossier FlightGear FGRoot (FGData)", path: model.paths.fgRoot) { model.choose("fgRoot") }
            LocationField(title: "Application IVAO Altitude", path: model.paths.altitude) { model.choose("altitude") }
            LocationField(title: "Application FlightGear", path: model.paths.flightGear) { model.choose("flightGear") }
            HStack {
                Text("Interface réseau IVAO :")
                TextField("en0", text: $model.paths.interface).frame(width: 85)
                Spacer()
                Toggle("Diagnostics", isOn: $model.diagnostics)
            }
            HStack(spacing: 10) {
                Text("Journaux : " + model.diagnosticsPath).font(.caption2).foregroundColor(.secondary).lineLimit(1).truncationMode(.middle)
                Spacer()
                Button("Ouvrir les logs") { model.revealLogs() }
            }
            Divider()
            HStack(spacing: 12) {
                Button(action: { model.start() }) {
                    Label("Démarrer FlightGear + IVAO", systemImage: "play.fill")
                        .frame(minWidth: 205)
                }.buttonStyle(.borderedProminent).disabled(model.busy || model.running)
                Button("Arrêter le bridge") { model.stop() }.disabled(!model.busy && !model.running)
            }
            HStack(spacing: 9) {
                if model.busy { ProgressView().controlSize(.small) }
                Text(model.message).font(.callout).foregroundColor(.secondary)
            }.frame(maxWidth: .infinity, alignment: .leading)
            Text("Prototype non officiel. Nécessite les outils de compilation Apple et une installation personnelle du plugin IVAO.")
                .font(.caption2).foregroundColor(.secondary)
        }
        .padding(24).frame(width: 670)
    }
}

@main struct IVAOFlightGearApp: App {
    var body: some Scene { WindowGroup { ContentView() } }
}
