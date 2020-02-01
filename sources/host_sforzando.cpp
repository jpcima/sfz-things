#include "host_sforzando.h"
#include <zstr.hpp>
#include <iostream>
#include <cstdio>

Host_Sforzando::Host_Sforzando(const char *dllname, f32 srate, u32 bsize)
    : srate_(srate), bsize_(bsize), temp_(new f32[2 * bsize])
{
    Dl_Handle handle = Dl_open(dllname);
    if (!handle) {
        fprintf(stderr, "Cannot load VST: %s\n", dllname);
        return;
    }
    handle_.reset(handle);

    typedef AEffect *(plugin_main_t)(audioMasterCallback);
    plugin_main_t *plugin_main = reinterpret_cast<plugin_main_t *>(Dl_sym(handle, "main"));
    if (!plugin_main)
        plugin_main = reinterpret_cast<plugin_main_t *>(Dl_sym(handle, "VSTPluginMain"));

    if (!plugin_main)
    {
        fprintf(stderr, "Cannot find VST entry point.\n");
        return;
    }

    AEffect *aeff = plugin_main(&host_callback);
    if (!aeff) {
        fprintf(stderr, "Cannot instance VST effect.\n");
        return;
    }
    aeff_.reset(aeff);

    aeff->ptr1 = this;

    if (aeff->numInputs != 0 && aeff->numOutputs != 2) {
        fprintf(stderr, "Unexpected number of inputs and outputs.\n");
        aeff_.reset();
        return;
    }

    aeff->dispatcher(aeff, effOpen, 0, 0, nullptr, 0);
    aeff->dispatcher(aeff, effSetSampleRate, 0, 0, nullptr, srate);
    aeff->dispatcher(aeff, effSetBlockSize, 0, bsize, nullptr, 0);
    aeff->dispatcher(aeff, effMainsChanged, 0, 1, nullptr, 0);
}

std::vector<u8> Host_Sforzando::get_chunk()
{
    AEffect *aeff = aeff_.get();
    u8 *data = nullptr;
    u32 size = aeff->dispatcher(aeff, 23 /*effGetChunk*/, 1, 0, (void **)&data, 0);
    return std::vector<u8>(data, data + size);
}

void Host_Sforzando::set_chunk(const u8 *data, u32 len)
{
    AEffect *aeff = aeff_.get();
    aeff->dispatcher(aeff, 24 /*effSetChunk*/, 1, len, (void *)data, 0);
}

bool Host_Sforzando::send_midi(const u8 *msg, size_t len, uint32_t frame)
{
    if (len > 4)
        return false;

    AEffect *aeff = aeff_.get();
    VstMidiEvent event = {};
    event.deltaFrames = frame;
    event.type = kVstMidiType;
    event.byteSize = sizeof(event);
    std::copy(msg, msg + len, event.midiData);

    VstEvents events = {};
    events.numEvents = 1;
    events.events[0] = reinterpret_cast<VstEvent *>(&event);

    aeff->dispatcher(aeff, effProcessEvents, 0, 0, &events, 0);
    return true;
}

bool Host_Sforzando::send_sysex(const u8 *msg, size_t len)
{
    AEffect *aeff = aeff_.get();

    struct Event {
        i32 type;
        i32 byteSize;
        i32 deltaFrames;
        i32 flags;
        i32 dumpBytes;
        void *resvd1;
        char *sysexDump;
        void *resvd2;
    };

    Event event = {};
    event.type = 6;
    event.byteSize = sizeof(event);
    event.dumpBytes = len;
    event.sysexDump = (char *)msg;

    VstEvents events = {};
    events.numEvents = 1;
    events.events[0] = reinterpret_cast<VstEvent *>(&event);

    aeff->dispatcher(aeff, effProcessEvents, 0, 0, &events, 0);
    return true;
}

void Host_Sforzando::get_editor_rect(Vst_ERect **rect)
{
    AEffect *aeff = aeff_.get();
    aeff->dispatcher(aeff, effEditGetRect, 0, 0, rect, 0);
}

void Host_Sforzando::open_editor(void *window)
{
    AEffect *aeff = aeff_.get();
    aeff->dispatcher(aeff, effEditOpen, 0, 0, window, 0);
}

void Host_Sforzando::close_editor()
{
    AEffect *aeff = aeff_.get();
    aeff->dispatcher(aeff, effEditClose, 0, 0, nullptr, 0);
}

void Host_Sforzando::idle_editor()
{
    AEffect *aeff = aeff_.get();
    aeff->dispatcher(aeff, effEditIdle, 0, 0, nullptr, 0);
}

void Host_Sforzando::process(f32 *out[], u32 nframes)
{
    AEffect *aeff = aeff_.get();
    aeff->processReplacing(aeff, nullptr, out, nframes);
}

intptr_t Host_Sforzando::host_callback(
    AEffect *aeff, i32 opcode, i32 index, intptr_t value, void *ptr, f32 opt)
{
    Host_Sforzando *self = reinterpret_cast<Host_Sforzando *>(aeff->ptr1);

    switch (opcode) {
    case audioMasterVersion:
        return 2400;

    case audioMasterUpdateDisplay:
        return 1;

    case audioMasterGetSampleRate:
        return self->srate_;

    case audioMasterGetCurrentProcessLevel:
        return 4;

    case audioMasterGetVendorString:
        strcpy((char *)ptr, "MyVendor");
        return 1;

    case audioMasterGetProductString:
        strcpy((char *)ptr, "MyProduct");
        return 1;

    case audioMasterGetVendorVersion:
        return 1;

    case audioMasterWantMidi:
        return 1;

    case audioMasterCanDo:
        //fprintf(stderr, "can do? %s\n", (char *)ptr);
        return 0;

#warning TODO(jpc) this host opcode would crash it, reason unknown
    case audioMasterGetBlockSize:
        //return self->bsize_;
        break;

    default:
        //fprintf(stderr, "Host: unhandled master opcode: %u\n", opcode);
        break;
    }

    return 0;
}

bool Host_Sforzando::convert_xml_to_fxb(const pugi::xml_document &doc, std::ostream &out)
{
    out.write("CcnK", 4); // chunk magic
    out.write("\0\0\0\0", 4); // byte size (0?)
    out.write("FJuc", 4); // fx magic
    out.write("\0\0\0\x01", 4); // version = 1
    out.write("PLGQ", 4); // unique id
    out.write("\0\0\0\x01", 4); // fx version = 1
    out.write("\0\0\0\0", 4); // number of programs
    for (uint32_t i = 0; i < 128; ++i)
        out.put('\0'); // reserved

    std::string payload;
    {
        std::ostringstream zstream{std::ios::binary};
        {
            zstr::ostream xmlstream{zstream};
            doc.save(xmlstream);
        }
        payload = zstream.str();
    }

    auto write_u32be = [](std::ostream &out, uint32_t src) {
        uint8_t data[4];
        data[0] = (src >> 24) & 0xff;
        data[1] = (src >> 16) & 0xff;
        data[2] = (src >> 8) & 0xff;
        data[3] = src & 0xff;
        out.write((char *)data, 4);
    };

    write_u32be(out, 8 + payload.size());

    out.write("CEGP", 4);
    out.write("\x11\x05\x00\x00", 4);
    out.write(payload.data(), payload.size());
    out.flush();

    return out.good();
}
