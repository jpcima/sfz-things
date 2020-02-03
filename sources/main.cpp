#include "defs.h"
#include "host_sforzando.h"
#include "utility/file.h"
#include <sndfile.hh>
#include <fmidi/fmidi.h>
#include <pugixml.hpp>
#include <zstr.hpp>
#include <memory>
#include <atomic>
#include <thread>
#include <iostream>
#include <sstream>
#include <cstdio>
#include <cstring>
#include <fcntl.h>

std::string command_name;

static void usage()
{
    const char *cmd = command_name.c_str();

    fprintf(
        stderr,
        "Usage: %s <cmd> [arg]...\n"
        "\n"
        "  * %s render <midi-file> <sfz-file> <sound-file>\n"
        "  * %s fxb-to-xml <file>\n"
        "  * %s xml-to-fxb <file>\n",
        cmd, cmd, cmd, cmd);
}

static std::string getFullPath(const std::string &path)
{
    std::string full;
    full.resize(MAX_PATH + 1);
    DWORD count = GetFullPathNameA(path.c_str(), full.size(), &full[0], nullptr);
    full.resize(count);
    return full;
}

static int render(int argc, char *argv[])
{
    if (argc != 4) {
        usage();
        return 1;
    }

    std::string midipath = getFullPath(argv[1]);
    if (midipath.empty()) {
        fprintf(stderr, "Cannot get the MIDI full path.\n");
        return 1;
    }
    std::string sfzpath = getFullPath(argv[2]);
    if (sfzpath.empty()) {
        fprintf(stderr, "Cannot get the SFZ full path.\n");
        return 1;
    }
    std::string sndpath = getFullPath(argv[3]);
    if (sfzpath.empty()) {
        fprintf(stderr, "Cannot get the Sound full path.\n");
        return 1;
    }

    fprintf(stderr, "* Load MIDI file: %s\n", midipath.c_str());
    fmidi_smf_u smf{fmidi_smf_file_read(midipath.c_str())};
    if (!smf) {
        fprintf(stderr, "Cannot read the MIDI file: %s\n", midipath.c_str());
        return 1;
    }

    const char default_state_data[] = {
        "<?xml version=\"1.0\" ?>\n"
        "<AriaSave version=\"1961\" productID=\"1014\">\n"
        "    <Settings quality=\"1\" streaming=\"32\" maxStreamAllocMB=\"512\" MIDIOutMode=\"1\" automationSlot=\"0\" automationlevel=\"instrument\" liveMode=\"0\" sc=\"1\" scala=\"01 - equal.scl\" scalaCenter=\"60\" globalTuning=\"0\" />\n"
        "    <Slot id=\"0\" name=\"C:/File.sfz\" bankId=\"-1\" version=\"0\" channel=\"-1\" poly=\"64\" tuning=\"0\" pb_range=\"-1\" ptrans=\"0\" mtrans=\"0\" moctave=\"0\" sc=\"1\" mute=\"0\">\n"
        "        <Main id=\"0\" value=\"1\" />\n"
        "        <Main id=\"1\" value=\"0\" />\n"
        "        <Main id=\"2\" value=\"0\" />\n"
        "        <Main id=\"3\" value=\"0\" />\n"
        "        <Main id=\"4\" value=\"0\" />\n"
        "        <Main id=\"5\" value=\"0\" />\n"
        "        <Main id=\"6\" value=\"0\" />\n"
        "        <Main id=\"7\" value=\"0\" />\n"
        "        <Main id=\"8\" value=\"0\" />\n"
        "        <Main id=\"9\" value=\"0\" />\n"
        "        <Main id=\"10\" value=\"0\" />\n"
        "        <Main id=\"11\" value=\"0\" />\n"
        "        <Main id=\"12\" value=\"0\" />\n"
        "        <Main id=\"13\" value=\"0\" />\n"
        "        <Main id=\"14\" value=\"0\" />\n"
        "        <Main id=\"15\" value=\"0\" />\n"
        "    </Slot>\n"
        "    <EffectSlot id=\"0\" sc=\"1\" name=\"Ambience\" bankId=\"1014\" version=\"1949\" procMode=\"0\" />\n"
        "    <GUI id=\"0\" activeISlot=\"0\" activeESlot=\"0\" selectedTab=\"-1\" />\n"
        "</AriaSave>\n"
    };

    pugi::xml_document doc;
    doc.load_string(default_state_data);

    for (char &c : sfzpath) {
        if (c == '\\')
            c = '/';
    }

    if (access(sfzpath.c_str(), F_OK) != 0) {
        fprintf(stderr, "The SFZ file does not exist: %s\n", sfzpath.c_str());
        return 1;
    }

    doc.root().child("AriaSave").child("Slot")
        .attribute("name").set_value(sfzpath.c_str());
    // doc.save(std::cout);

    std::string fxb = [&doc]() -> std::string {
        std::ostringstream out{std::ios::binary};
        Host_Sforzando::convert_xml_to_fxb(doc, out);
        return out.str();
    }();

    ///
    constexpr double samplerate = 44100.0;
    constexpr uint32_t buffersize = 512;

    const char *dllpath = SFORZANDO_DLL_DEFAULT_PATH;
    fprintf(stderr, "* Load VST: %s\n", dllpath);

    Host_Sforzando host{dllpath, samplerate, buffersize};
    if (!host)
        return 1;

    ///
    Vst_ERect *rect = nullptr;
    host.get_editor_rect(&rect);
    if (!rect)
        return 1;

    HINSTANCE hinst = GetModuleHandle(nullptr);

    WNDCLASSEX wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = 0;
    wc.lpfnWndProc = &DefWindowProc;
    wc.hInstance = hinst;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszClassName = "SFZ Host";
    if (!RegisterClassEx(&wc))
        return 1;

    HWND wnd = CreateWindowEx(
        WS_EX_OVERLAPPEDWINDOW,
        wc.lpszClassName,
        "SFZ Host",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect->right - rect->left, rect->bottom - rect->top,
        nullptr, nullptr, hinst, nullptr);

    ShowWindow(wnd, SW_SHOW);
    UpdateWindow(wnd);

    host.open_editor(wnd);
    SetTimer(wnd, 0, 50, nullptr);
    ///

    std::atomic<int> work_status;

    auto work = [&host, &fxb, &sfzpath, &smf, &sndpath, &work_status]() -> void {
        fprintf(stderr, "* Load state: %s\n", sfzpath.c_str());
        host.set_chunk((const uint8_t *)fxb.data(), fxb.size());

        fprintf(stderr, "* Play MIDI\n");

        fmidi_player_u player{fmidi_player_new(smf.get())};
        bool play_finished = false;
        uint32_t midi_frame = 0;

        auto on_event = [&host, &midi_frame](const fmidi_event_t *ev) {
                            if (ev->type == fmidi_event_message)
                                host.send_midi(ev->data, ev->datalen, midi_frame);
                        };
        auto on_finish = [&play_finished]() {
                             play_finished = true;
                         };

        fmidi_player_event_callback(
            player.get(), [](const fmidi_event_t *ev, void *ptr) {
                              (*static_cast<decltype(on_event) *>(ptr))(ev);
                          }, &on_event);
        fmidi_player_finish_callback(
            player.get(), [](void *ptr) {
                              (*static_cast<decltype(on_finish) *>(ptr))();
                          }, &on_finish);

        fmidi_player_start(player.get());

        std::unique_ptr<float[]> buffer_l{new float[buffersize]};
        std::unique_ptr<float[]> buffer_r{new float[buffersize]};
        uint32_t totalframes = 0;

        SndfileHandle snd(
            sndpath.c_str(), SFM_WRITE, SF_FORMAT_WAV|SF_FORMAT_PCM_16, 2, samplerate);
        if (!snd) {
            fprintf(stderr, "Cannot open WAV file for writing: %s\n", sndpath.c_str());
            work_status.store(1);
            return;
        }

        fprintf(stderr, "* Write sound: %s\n", sndpath.c_str());

        while (!play_finished) {
            for (midi_frame = 0; midi_frame < buffersize && !play_finished; ++midi_frame)
                fmidi_player_tick(player.get(), 1.0 / samplerate);

            float *outs[] = {buffer_l.get(), buffer_r.get()};
            host.process(outs, midi_frame);

            for (uint32_t i = 0; i < midi_frame; ++i) {
                auto saturate = [](float &x) {
                                    if (x > 1) x = 1;
                                    if (x < -1) x = -1;
                                };
                saturate(buffer_l[i]);
                saturate(buffer_r[i]);
            }

            for (uint32_t i = 0; i < midi_frame; ++i) {
                float frame[2] = {buffer_l[i], buffer_r[i]};
                snd.writef(frame, 1);
            }

            if (0) {
                for (uint32_t i = 0; i < midi_frame; ++i)
                    printf("%u %f %f\n", totalframes + i, buffer_l[i], buffer_r[i]);
            }

            totalframes += midi_frame;
        }

        snd.writeSync();

        work_status.store(0);
    };

    ///
    work_status.store(-1);
    std::thread work_thread{work};

    ///
    MSG msg;
    for (bool is_open = true; is_open && work_status.load() == -1 && GetMessage(&msg, nullptr, 0, 0);) {
        if (msg.message == WM_SYSCOMMAND && msg.hwnd == wnd && msg.wParam == SC_CLOSE) {
            is_open = false;
        }
        else if (msg.message == WM_TIMER && msg.hwnd == wnd) {
            host.idle_editor();
        }
        else {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    ///
    work_thread.join();

    ///
    host.close_editor();
    DestroyWindow(wnd);
    ///

    ///
    if (work_status.load() == 1)
        return 1;

    ///
    return 0;
}

static int fxb_to_xml(int argc, char *argv[])
{
    if (argc != 2) {
        usage();
        return 1;
    }

    const char *filename = argv[1];
    FILE *file = fopen(filename, "rb");

    FILE_u file_cleanup{file};

    if (!file) {
        fprintf(stderr, "Cannot open input file.\n");
        return 1;
    }

    if (fseek(file, 0x9c, SEEK_SET) == -1) { // skip header
        fprintf(stderr, "Bad format.\n");
        return 1;
    }

    auto read_u32be = [](FILE *file, uint32_t *dst) -> bool {
        uint8_t data[4];
        if (fread(data, 4, 1, file) != 1)
            return false;
        *dst = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
        return true;
    };

    uint32_t size;
    if (!read_u32be(file, &size)) {
        fprintf(stderr, "Bad format.\n");
        return 1;
    }

    if (size > 1024 * 1024) {
        fprintf(stderr, "Payload too large.\n");
        return 1;
    }

    if (size < 8) {
        fprintf(stderr, "Payload too small.\n");
        return 1;
    }

    size -= 8;
    if (fseek(file, 8, SEEK_CUR) == -1) {
        fprintf(stderr, "Bad format.\n");
        return 1;
    }

    std::string zstring;
    zstring.resize(size);
    if (fread(&zstring[0], size, 1, file) != 1) {
        fprintf(stderr, "Read error.\n");
        return 1;
    }

    std::istringstream zin{zstring, std::ios::binary};
    zstr::istream xmlin{zin};

    pugi::xml_document doc;
    if (!doc.load(xmlin)) {
        fprintf(stderr, "Cannot parse the payload as XML.\n");
        return 1;
    }

    doc.save(std::cout);

    return 0;
}

static int xml_to_fxb(int argc, char *argv[])
{
    if (argc != 2) {
        usage();
        return 1;
    }

    const char *filename = argv[1];
    pugi::xml_document doc;

    if (!doc.load_file(filename)) {
        fprintf(stderr, "Cannot parse the payload as XML.\n");
        return 1;
    }

    if (isatty(fileno(stdout))) {
        fprintf(stderr, "Refusing to write binary to terminal.\n");
        return 1;
    }

    if (!Host_Sforzando::convert_xml_to_fxb(doc, std::cout)) {
        fprintf(stderr, "Write error.\n");
        return 1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    std::string command_program = argv[0];
    size_t command_separator = command_program.find_last_of("/\\");
    if (command_separator != std::string::npos)
        command_name = command_program.substr(command_separator + 1);
    else
        command_name = command_program;

    _setmode(fileno(stdout), O_BINARY);

    int (*cmdfn)(int, char *[]) = nullptr;

    if (argc >= 2) {
        const char *cmd = argv[1];

        if (!strcmp(cmd, "render")) {
            cmdfn = &render;
        }
        else if (!strcmp(cmd, "fxb-to-xml")) {
            cmdfn = &fxb_to_xml;
        }
        else if ("xml-to-fxb") {
            cmdfn = &xml_to_fxb;
        }
    }

    if (!cmdfn) {
        usage();
        return 1;
    }

    return cmdfn(argc - 1, argv + 1);
}
