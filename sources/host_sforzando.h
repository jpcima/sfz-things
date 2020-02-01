#pragma once
#include "utility/load_library.h"
#include "utility/types.h"
#include <pugixml.hpp>
#include <vestige.h>
#include <vector>
#include <memory>
#include <cstdio>
namespace cws80 { struct Bank; }

struct Vst_ERect {
    int16_t top, left, bottom, right;
};

class Host_Sforzando {
public:
    Host_Sforzando(const char *dllname, f32 srate, u32 bsize);

    f32 sample_rate() const noexcept { return srate_; }
    u32 buffer_size() const noexcept { return bsize_; }

    std::vector<u8> get_chunk();
    void set_chunk(const u8 *data, u32 len);
    bool send_midi(const u8 *msg, size_t len, uint32_t frame);
    bool send_sysex(const u8 *msg, size_t len);

    void get_editor_rect(Vst_ERect **rect);
    void open_editor(void *window);
    void close_editor();
    void idle_editor();

    void process(f32 *out[], u32 nframes);

    explicit operator bool() const noexcept { return aeff_ != nullptr; }

    static bool convert_xml_to_fxb(const pugi::xml_document &doc, std::ostream &out);

private:
    static intptr_t host_callback(
        AEffect *aeff, i32 opcode, i32 index, intptr_t value, void *ptr, f32 opt);

private:
    struct AEffect_deleter { void operator()(AEffect *x) const noexcept { x->dispatcher(x, effClose, 0, 0, nullptr, 0); } };

    f32 srate_ = 0;
    u32 bsize_ = 0;
    std::unique_ptr<f32> temp_;

    Dl_Handle_U handle_;
    std::unique_ptr<AEffect, AEffect_deleter> aeff_;

private:
    Host_Sforzando(const Host_Sforzando &) = delete;
    Host_Sforzando &operator=(const Host_Sforzando &) = delete;
};
