#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

#include "Cli.h"
#include "Commands.h"
#include "DebugSupport.h"
#include "Debugger.h"
#include "EmCommon.h"
#include "EmDevice.h"
#include "EmROMReader.h"
#include "EmSession.h"
#include "ExternalStorage.h"
#include "Feature.h"
#include "GdbStub.h"
#include "LogDomain.h"
#include "MainLoop.h"
#include "ScreenDimensions.h"
#include "SessionImage.h"
#include "SuspendContextClipboardCopy.h"
#include "SuspendContextClipboardPaste.h"
#include "SuspendManager.h"
#include "argparse.h"
#include "uri/uri.h"
#include "util.h"

using namespace std;

// --- Framebuffer Globals ---
int fbfd = 0;
struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;
uint8_t* fbp = nullptr;

struct DebuggerConfiguration {
    bool enabled{false};
    unsigned int port{0};
    bool waitForAttach{false};
    optional<string> appFile;
};

struct Options {
    string image;
    optional<string> deviceId;
    optional<string> scriptFile;
    bool traceNetlib;
    bool traceDebugger;
    bool traceInstaller;
    optional<string> mountImage;
    DebuggerConfiguration debuggerConfiguration;
    bool smallWindow;
};

void handleSuspend() {
    if (!SuspendManager::IsSuspended()) return;
    SuspendContext& context = SuspendManager::GetContext();
    // Clipboard integration disabled
    switch (context.GetKind()) {
        case SuspendContext::Kind::clipboardCopy:
            context.AsContextClipboardCopy().Resume();
            break;
        case SuspendContext::Kind::clipboardPaste:
            context.AsContextClipboardPaste().Resume("");
            break;
        default:
            break;
    }
}

void setupCard(const Options& options) {
    string imageKey;
    if (options.mountImage) imageKey = util::registerImage(*options.mountImage);

    if (!(options.deviceId ? util::initializeSession(options.image, *options.deviceId)
                           : util::initializeSession(options.image)))
        exit(1);

    if (!imageKey.empty() && gExternalStorage.RemountFailed()) {
        cout << "remount failed" << endl << flush;
        gExternalStorage.RemoveImage(imageKey);
        imageKey.clear();
    }

    if (!imageKey.empty() && util::mountKey(imageKey))
        cout << *options.mountImage << " mounted successfully" << endl << flush;
}

void setupDebugger(GdbStub& gdbStub, const Options& options) {
    if (options.debuggerConfiguration.enabled) {
        gdbStub.Listen();
        if (options.debuggerConfiguration.waitForAttach) {
            cout << "waiting for debugger to attach..." << endl << flush;
            while (gdbStub.GetConnectionState() == GdbStub::ConnectionState::listening)
                gdbStub.Cycle(1000);
        }
        if (options.debuggerConfiguration.appFile) {
            unique_ptr<uint8[]> buffer;
            size_t len;
            if (!util::ReadFile(*options.debuggerConfiguration.appFile, buffer, len))
                cout << "failed to read " << *options.debuggerConfiguration.appFile << endl
                     << flush;
            else
                debug_support::SetApp(buffer.get(), len, nullptr, gdbStub, gDebugger);
        }
    }
}

void initFB() {
    fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd == -1) {
        cerr << "Error: cannot open framebuffer device." << endl;
        exit(1);
    }

    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) == -1) {
        cerr << "Error reading fixed information." << endl;
        exit(1);
    }

    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        cerr << "Error reading variable information." << endl;
        exit(1);
    }

    long int screensize = vinfo.yres_virtual * finfo.line_length;
    fbp = (uint8_t*)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);

    if (reinterpret_cast<intptr_t>(fbp) == -1) {
        cerr << "Error: failed to map framebuffer device to memory." << endl;
        exit(1);
    }

    cout << "Framebuffer mapped: " << vinfo.xres << "x" << vinfo.yres << ", "
         << vinfo.bits_per_pixel << "bpp" << endl;
}

void run(const Options& options) {
    srand(time(nullptr));
    signal(SIGPIPE, SIG_IGN);

    setupCard(options);

    Feature::SetClipboardIntegration(false);  // Disabled for PoC

    ScreenDimensions::Kind screenDimensionsKind = gSession->GetDevice().GetScreenDimensions();
    ScreenDimensions screenDimensions(screenDimensionsKind);

    int scale = 2;

    initFB();

    unique_ptr<MainLoop> mainLoop = make_unique<MainLoop>(fbp, vinfo, finfo, scale);

    cout << "Inniting main loop" << endl;
    while (mainLoop->IsRunning()) {
        if (gSession->GetDevice().GetScreenDimensions() != screenDimensionsKind) {
            cout << "Diff dimensions" << endl;
            screenDimensionsKind = gSession->GetDevice().GetScreenDimensions();
            mainLoop = make_unique<MainLoop>(fbp, vinfo, finfo, scale);
        }

        mainLoop->Cycle();

        handleSuspend();
    };

    // Cleanup FB
    munmap(fbp, vinfo.yres_virtual * finfo.line_length);
    close(fbfd);
}

int main(int argc, const char** argv) {
    class bad_device_id : public exception {};

    argparse::ArgumentParser program("cloudpilot-emu");

    program.add_description("CloudpilotEmu is an emulator for dragonball-based PalmOS devices.");
    program.add_argument("image").help("image or ROM file").required();
    program.add_argument("--device-id", "-d")
        .help("specify device ID")
        .metavar("<device>")
        .action([](const string& value) -> string {
            for (auto& deviceId : util::SUPPORTED_DEVICES)
                if (value == deviceId) return deviceId;
            throw bad_device_id();
        });

    program.add_argument("--proxy-insecure")
        .help("do not validate server certificate when using the network proxy")
        .default_value(false)
        .implicit_value(true);
    program.add_argument("--proxy-ca")
        .help("root certificate for network proxy")
        .metavar("<certificate file>");
    program.add_argument("--mount").metavar("<image file>").help("mount card image");
    program.add_argument("--script", "-s")
        .metavar("<script file>")
        .help("execute script on startup");

#ifdef ENABLE_DEBUGGER
    program.add_argument("--listen", "-l")
        .metavar("<port>")
        .help("listen for GDB on port")
        .scan<'u', unsigned int>();
    program.add_argument("--wait-for-attach")
        .help("wait for debugger on launch")
        .default_value(false)
        .implicit_value(true);
    program.add_argument("--debug-app")
        .metavar("<elf file>")
        .help("configure debugger to debug app (elf file)");
    program.add_argument("--trace-debugger")
        .help("trace gdb stub")
        .default_value(false)
        .implicit_value(true);
#endif

    program.add_argument("--trace-netlib")
        .help("trace network API")
        .default_value(false)
        .implicit_value(true);
    program.add_argument("--trace-installer")
        .help("trace database installer")
        .default_value(false)
        .implicit_value(true);
    program.add_argument("--small-window")
        .help("use small window")
        .default_value(false)
        .implicit_value(true);

    try {
        program.parse_args(argc, argv);
    } catch (const bad_device_id& e) {
        cerr << "bad device ID; valid IDs are:" << endl;
        for (auto& deviceId : util::SUPPORTED_DEVICES) cerr << "  " << deviceId << endl;
        exit(1);
    } catch (const invalid_argument& e) {
        cerr << "invalid argument" << endl << endl << program;
        exit(1);
    } catch (const runtime_error& e) {
        cerr << e.what() << endl << endl << program;
        exit(1);
    }

    Options options{.traceDebugger = false};

    options.image = program.get("image");
    options.traceNetlib = program.get<bool>("--trace-netlib");
    options.traceInstaller = program.get<bool>("--trace-installer");
    options.mountImage = program.present("--mount");
    options.deviceId = program.present("--device-id");
    options.scriptFile = program.present("--script");
    options.smallWindow = program.get<bool>("--small-window");

#ifdef ENABLE_DEBUGGER
    if (auto port = program.present<unsigned int>("--listen"))
        options.debuggerConfiguration = {.enabled = true,
                                         .port = *port,
                                         .waitForAttach = program.get<bool>("--wait-for-attach")};
    options.traceDebugger = program.get<bool>("--trace-debugger");
    options.debuggerConfiguration.appFile = program.present("--debug-app");
#endif

    run(options);
}
