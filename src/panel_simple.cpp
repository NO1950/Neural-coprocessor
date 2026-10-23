// MGPU Bridge - panel_simple: the one-click panel for people who just want to play.
//
// WHAT THIS IS. A second view of the SAME controls the full panel has, with the log lines, the
// counters and the timing readouts stripped out, so a player who does not care about any of that can
// change what matters and get on with the game. It is NOT a mock-up: every row here calls the same
// setter the full panel calls (ui_nrs_stage / ui_nrs_set / ...), so a change applies live, to the
// same state, and persists the same way. If a row is added here it must carry real behaviour - the
// whole point of the mode is that it is the full panel with less on screen, not a simplified copy.
//
// THE LAYOUT IS DATA, NOT CODE. `panel_simple.txt` next to the add-on decides which rows appear and
// in what order, and carries their Chinese labels and their help text. A row that exists in code but
// not in the file does not appear; a file line whose id has no drawer is reported and skipped. So a
// new feature lands by adding one `row()` call and one line in the file - the simple panel never has
// to be "unfrozen" for it.
//
// CHINESE. The add-on cannot add a font to ReShade's ImGui (it links only through the function
// table, which exposes no ImFontAtlas entry point - see the LNK2019 note in dllmain). The glyphs
// therefore have to come from the font ReShade itself loaded: `Font=` in ReShade.ini. When that font
// has no CJK glyphs the panel falls back to the English label automatically, so a player who never
// touches ReShade.ini still gets a usable panel instead of boxes.
//
// Log tags: [R351] init/file, [R352] mode switch, [R353] unknown id.

#include "panel_simple.hpp"

#include <windows.h>
#include <shellapi.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>

// Same rule as dllmain: imgui.h has to come first so reshade.hpp binds the ImGui function table, and
// ImTextureID must be ReShade's 64-bit handle. Guarded so a build without the headers still compiles
// (the panel is then absent, as in dllmain's P7.1).
#if defined(__has_include)
#  if __has_include(<imgui.h>)
#    define MGPU_HAVE_IMGUI 1
#    define ImTextureID ImU64
#    include <imgui.h>
#  endif
#endif

#include <reshade.hpp>          // the ImGui wrappers (function table), as everywhere else
#include "gpu1_context.hpp"     // ui_ini_read / ui_ini_write / ui_nrs_* / ui_read
#include "fg_map.hpp"           // R351d: frame-generation mode/keep are live only through fgmap
#include "diag.hpp"

namespace mgpu::psimple
{
    namespace
    {
        struct entry
        {
            std::string id;
            std::string zh;      // Chinese label, empty = use the code's English one
            std::string help;    // \n allowed, expanded when drawn
            // R351e: a line that starts with '=' is a SECTION HEADER, not a control. Its text is the
            // title drawn with SeparatorText, and the rows that follow it belong to that section. An
            // empty banner (just '======') draws a plain separator.
            bool        header = false;
        };

        struct row_def
        {
            std::string id;
            std::string en;
            draw_fn     fn = nullptr;
        };

        std::string              g_dir;              // the add-on's directory, trailing backslash
        std::vector<entry>       g_layout;           // as the file has it, in order
        std::vector<row_def>     g_rows;             // registered drawers, in code order
        std::map<std::string, entry> g_by_id;        // lookup for labels/help
        unsigned long long       g_file_mtime = 0;
        bool                     g_tried_load = false;
        std::vector<std::string> g_reported;         // ids already complained about

        std::string path_of(const char *name)
        {
            std::string p = g_dir;
            p += name;
            return p;
        }

        bool read_file(const std::string &path, std::string &out)
        {
            FILE *f = nullptr;
            if (fopen_s(&f, path.c_str(), "rb") != 0 || f == nullptr) return false;
            fseek(f, 0, SEEK_END);
            const long n = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (n <= 0) { fclose(f); out.clear(); return true; }
            out.resize((size_t)n);
            const size_t got = fread(&out[0], 1, (size_t)n, f);
            out.resize(got);
            fclose(f);
            return true;
        }

        unsigned long long mtime_of(const std::string &path)
        {
            WIN32_FILE_ATTRIBUTE_DATA d{};
            if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &d)) return 0;
            return ((unsigned long long)d.ftLastWriteTime.dwHighDateTime << 32) | d.ftLastWriteTime.dwLowDateTime;
        }

        std::string trim(const std::string &s)
        {
            size_t a = 0, b = s.size();
            while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
            while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
            return s.substr(a, b - a);
        }

        void parse(const std::string &text)
        {
            g_layout.clear();
            g_by_id.clear();
            size_t pos = 0;
            while (pos <= text.size())
            {
                size_t nl = text.find('\n', pos);
                if (nl == std::string::npos) nl = text.size();
                std::string line = text.substr(pos, nl - pos);
                pos = nl + 1;
                // A UTF-8 BOM on the first line (Notepad writes one) is not a comment and not an id.
                if (line.size() >= 3 && (unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB &&
                    (unsigned char)line[2] == 0xBF)
                    line.erase(0, 3);
                line = trim(line);
                if (line.empty() || line[0] == '#' || line[0] == ';') continue;
                // R351e: a banner is a header. '==== 模型 / 画风 ====' becomes a section title, and an
                // empty '======' becomes a plain separator. The '=' runs are stripped from both ends,
                // so the operator writes the title once, the way the commented banners already read.
                if (line[0] == '=')
                {
                    size_t a = 0, b = line.size();
                    while (a < b && line[a] == '=') ++a;
                    while (b > a && line[b - 1] == '=') --b;
                    entry h;
                    h.header = true;
                    h.id = "section:" + trim(line.substr(a, b - a));
                    h.zh = trim(line.substr(a, b - a));
                    g_layout.push_back(h);
                    continue;
                }
                entry e;
                const size_t p1 = line.find('|');
                if (p1 == std::string::npos)
                {
                    e.id = line;
                }
                else
                {
                    e.id = trim(line.substr(0, p1));
                    const size_t p2 = line.find('|', p1 + 1);
                    if (p2 == std::string::npos)
                        e.zh = trim(line.substr(p1 + 1));
                    else
                    {
                        e.zh = trim(line.substr(p1 + 1, p2 - p1 - 1));
                        e.help = trim(line.substr(p2 + 1));
                    }
                }
                if (e.id.empty()) continue;
                g_by_id[e.id] = e;
                g_layout.push_back(e);
            }
        }

        void load_if_changed(bool force)
        {
            const std::string p = path_of("panel_simple.txt");
            const unsigned long long mt = mtime_of(p);
            if (!force && g_tried_load && mt == g_file_mtime) return;
            g_tried_load = true;
            g_file_mtime = mt;
            std::string text;
            if (!read_file(p, text))
            {
                g_layout.clear();
                g_by_id.clear();
                char l[400];
                snprintf(l, sizeof l, "[MGPU][R351] panel_simple.txt not found at %s - the simple panel will "
                                      "show nothing. Use 'export items' in the full panel to write a template.",
                         p.c_str());
                mgpu::diag::warn(l);
                return;
            }
            parse(text);
            char l[320];
            snprintf(l, sizeof l, "[MGPU][R351] panel_simple.txt: %u row(s) in the layout (%s)",
                     (unsigned)g_layout.size(), p.c_str());
            mgpu::diag::info(l);
        }

        const row_def *find_row(const std::string &id)
        {
            for (const row_def &r : g_rows)
                if (r.id == id) return &r;
            return nullptr;
        }

        void report_once(const std::string &id, const char *why)
        {
            for (const std::string &s : g_reported)
                if (s == id) return;
            g_reported.push_back(id);
            char l[320];
            snprintf(l, sizeof l, "[MGPU][R353] panel_simple.txt names '%s' but %s - that line is skipped.",
                     id.c_str(), why);
            mgpu::diag::warn(l);
        }

        // "\n" written as two characters in the file becomes a real line break here.
        std::string unescape(const std::string &s)
        {
            std::string out;
            out.reserve(s.size());
            for (size_t i = 0; i < s.size(); ++i)
            {
                if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == 'n') { out.push_back('\n'); ++i; }
                else out.push_back(s[i]);
            }
            return out;
        }
    }   // namespace

    void init(HMODULE mod)
    {
        wchar_t p[MAX_PATH]{};
        if (GetModuleFileNameW(mod, p, MAX_PATH) == 0) return;
        wchar_t *slash = wcsrchr(p, L'\\');
        if (slash != nullptr) *(slash + 1) = 0;
        char narrow[MAX_PATH * 2]{};
        WideCharToMultiByte(CP_UTF8, 0, p, -1, narrow, (int)sizeof narrow - 1, nullptr, nullptr);
        g_dir = narrow;
        load_if_changed(true);
        char l[400];
        snprintf(l, sizeof l, "[MGPU][R351] simple panel ready: directory %s, %u layout row(s)",
                 g_dir.c_str(), (unsigned)g_layout.size());
        mgpu::diag::info(l);
    }

#ifdef MGPU_HAVE_IMGUI

    bool cjk()
    {
        ImFont *f = ImGui::GetFont();
        return f != nullptr && f->IsGlyphInFont((ImWchar)0x4E2D) != false;
    }

    bool enabled()
    {
        load_if_changed(false);
        return mgpu::gpu1::ui_ini_read("PanelSimple", 0) != 0 && !g_layout.empty();
    }

    void set_enabled(bool on)
    {
        mgpu::gpu1::ui_ini_write("PanelSimple", on ? 1 : 0);
        char l[200];
        snprintf(l, sizeof l, "[MGPU][R352] PanelSimple=%d (%s mode)", on ? 1 : 0,
                 on ? "simple" : "full");
        mgpu::diag::info(l);
    }

    void reload() { load_if_changed(true); }
    int layout_count() { return (int)g_layout.size(); }

    void row(const char *id, const char *en, draw_fn fn)
    {
        if (id == nullptr || fn == nullptr) return;
        for (const row_def &r : g_rows)
            if (r.id == id) return;   // registered once
        row_def r;
        r.id = id;
        r.en = (en != nullptr) ? en : id;
        r.fn = fn;
        g_rows.push_back(r);
    }

    const char *zh(const char *id)
    {
        if (id == nullptr) return nullptr;
        auto it = g_by_id.find(id);
        if (it == g_by_id.end() || it->second.zh.empty()) return nullptr;
        return it->second.zh.c_str();
    }

    void label(const char *id, const char *en)
    {
        const char *z = cjk() ? zh(id) : nullptr;
        ImGui::TextUnformatted(z != nullptr ? z : (en != nullptr ? en : ""));
    }

    void help(const char *id)
    {
        auto it = g_by_id.find(id != nullptr ? id : "");
        if (it == g_by_id.end() || it->second.help.empty()) return;
        const std::string body = unescape(it->second.help);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            // First line only on hover - the popup carries the whole text.
            const size_t nl = body.find('\n');
            ImGui::SetTooltip("%s", body.substr(0, nl).c_str());
        }
        if (ImGui::IsItemClicked())
            ImGui::OpenPopup(id);
        if (ImGui::BeginPopup(id))
        {
            ImGui::PushTextWrapPos(420.0f);
            ImGui::TextUnformatted(body.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndPopup();
        }
    }

    void open_editor()
    {
        const std::string p = path_of("panel_simple.txt");
        const std::wstring wp(p.begin(), p.end());
        // notepad is always there; the file's own association is deliberately not used so this works
        // on a machine where .txt is bound to something modal.
        ShellExecuteW(nullptr, L"open", L"notepad.exe", wp.c_str(), nullptr, SW_SHOWNORMAL);
        mgpu::diag::info("[MGPU][R351] opened panel_simple.txt in notepad");
    }

    void export_items()
    {
        const std::string p = path_of("panel_simple_items.txt");
        FILE *f = nullptr;
        if (fopen_s(&f, p.c_str(), "wb") != 0 || f == nullptr)
        {
            mgpu::diag::warn("[MGPU][R351] could not write panel_simple_items.txt");
            return;
        }
        // A template the operator can copy lines out of: every registered row with its English name
        // and a placeholder Chinese label, in the file's own syntax.
        fprintf(f, "# Every row this build can put in the simple panel.\n");
        fprintf(f, "# Copy a line into panel_simple.txt and edit the label and the help text.\n");
        fprintf(f, "# format:  id | 中文标签 | 说明（可写 \\n 换行）\n\n");
        for (const row_def &r : g_rows)
            fprintf(f, "%-18s | %s | \n", r.id.c_str(), r.en.c_str());
        fclose(f);
        char l[320];
        snprintf(l, sizeof l, "[MGPU][R351] wrote %u row(s) to %s", (unsigned)g_rows.size(), p.c_str());
        mgpu::diag::info(l);
    }

    // ---------------------------------------------------------------------
    // The rows themselves. Each one is the SAME call the full panel makes, with the label removed
    // (the simple panel draws the label from the file): ui_nrs_stage / ui_nrs_set are the live
    // setters, so a drag here is a drag there.
    // ---------------------------------------------------------------------
    namespace
    {
        void row_nrs_scale()
        {
            mgpu::gpu1::ui_state st;
            mgpu::gpu1::ui_read(st);
            if (!st.nrs_on) { ImGui::TextDisabled("NRScale not set in mgpu.ini"); return; }
            float sc = st.nrs_scale;
            const float top = (st.nrs_cap > 1.0f) ? st.nrs_cap : 1.0f;
            ImGui::SetNextItemWidth(190.0f);
            if (ImGui::SliderFloat("##nrs_scale", &sc, 0.30f, top, "%.2f"))
                mgpu::gpu1::ui_nrs_stage(sc);
            ImGui::SameLine();
            if (ImGui::SmallButton("默认##nrs_scale")) mgpu::gpu1::ui_nrs_stage(0.75f);
        }

        void row_nrs_gain()
        {
            mgpu::gpu1::ui_state st;
            mgpu::gpu1::ui_read(st);
            if (!st.nrs_on) { ImGui::TextDisabled("-"); return; }
            float v = st.nrs_gain;
            ImGui::SetNextItemWidth(190.0f);
            if (ImGui::SliderFloat("##nrs_gain", &v, 0.0f, 2.0f, "%.2f"))
                mgpu::gpu1::ui_nrs_set(0, v);
            ImGui::SameLine();
            if (ImGui::SmallButton("默认##nrs_gain")) mgpu::gpu1::ui_nrs_set(0, 1.00f);
        }

        void row_nrs_sharp()
        {
            mgpu::gpu1::ui_state st;
            mgpu::gpu1::ui_read(st);
            if (!st.nrs_on) { ImGui::TextDisabled("-"); return; }
            float v = st.nrs_sharpness;
            ImGui::SetNextItemWidth(190.0f);
            if (ImGui::SliderFloat("##nrs_sharp", &v, 0.0f, 1.0f, "%.2f"))
                mgpu::gpu1::ui_nrs_set(1, v);
            ImGui::SameLine();
            if (ImGui::SmallButton("默认##nrs_sharp")) mgpu::gpu1::ui_nrs_set(1, 0.50f);
        }

        void row_nrs_colour()
        {
            mgpu::gpu1::ui_state st;
            mgpu::gpu1::ui_read(st);
            if (!st.nrs_on) { ImGui::TextDisabled("-"); return; }
            float v = st.nrs_color;
            ImGui::SetNextItemWidth(190.0f);
            if (ImGui::SliderFloat("##nrs_col", &v, 0.0f, 1.0f, "%.2f"))
                mgpu::gpu1::ui_nrs_set(2, v);
            ImGui::SameLine();
            if (ImGui::SmallButton("默认##nrs_col")) mgpu::gpu1::ui_nrs_set(2, 0.90f);
        }

        void row_nrs_mode()
        {
            mgpu::gpu1::ui_state st;
            mgpu::gpu1::ui_read(st);
            if (!st.nrs_on) { ImGui::TextDisabled("-"); return; }
            int mo = (int)st.nrs_mode;
            if (ImGui::RadioButton("残差##nrs_mode", mo == 1)) mgpu::gpu1::ui_nrs_set(3, 1.0f);
            ImGui::SameLine();
            if (ImGui::RadioButton("直接##nrs_mode", mo == 0)) mgpu::gpu1::ui_nrs_set(3, 0.0f);
        }

        // ---- R351b: the rows a player may also want, all live setters ----
        void row_passes()
        {
            mgpu::gpu1::ui_state st;
            mgpu::gpu1::ui_read(st);
            int p = (int)st.passes;
            if (ImGui::RadioButton("×1##passes", p == 1)) mgpu::gpu1::ui_set_passes(1);
            ImGui::SameLine();
            if (ImGui::RadioButton("×2##passes", p == 2)) mgpu::gpu1::ui_set_passes(2);
        }

        void row_intensity()
        {
            mgpu::gpu1::ui_state st;
            mgpu::gpu1::ui_read(st);
            // R351f: index 0 is the full panel's "all passes" - it distributes across every pass. The
            // first cut used 1 (pass 1 only), which silently ignored pass 2 whenever x2 was on.
            float v = st.intensity[0];
            ImGui::SetNextItemWidth(190.0f);
            if (ImGui::SliderFloat("##intensity", &v, 0.0f, 2.0f, "%.2f"))
                mgpu::gpu1::ui_set_intensity(0, v);
            ImGui::SameLine();
            if (ImGui::SmallButton("默认##intensity")) mgpu::gpu1::ui_set_intensity(0, 2.00f);
        }

        void row_view()
        {
            mgpu::gpu1::ui_state st;
            mgpu::gpu1::ui_read(st);
            int pm = st.present_mode;
            if (ImGui::RadioButton("输出##view", pm == 0)) mgpu::gpu1::ui_set_present_mode(0);
            ImGui::SameLine();
            if (ImGui::RadioButton("输入##view", pm == 1)) mgpu::gpu1::ui_set_present_mode(1);
            ImGui::SameLine();
            if (ImGui::RadioButton("分屏##view", pm == 2)) mgpu::gpu1::ui_set_present_mode(2);
            if (pm == 2)
            {
                float sp = st.split_pos;
                ImGui::SetNextItemWidth(190.0f);
                if (ImGui::SliderFloat("##seam", &sp, 0.0f, 1.0f, "分界 %.2f"))
                    mgpu::gpu1::ui_set_split_pos(sp);
            }
        }

        void row_mvec_mode()
        {
            mgpu::gpu1::ui_state st;
            mgpu::gpu1::ui_read(st);
            int m = st.mvec_mode;
            if (ImGui::RadioButton("关##mvecmode", m == 0)) mgpu::gpu1::ui_set_mvec_mode(0);
            ImGui::SameLine();
            if (ImGui::RadioButton("合成##mvecmode", m == 1)) mgpu::gpu1::ui_set_mvec_mode(1);
            ImGui::SameLine();
            if (ImGui::RadioButton("真实##mvecmode", m == 3)) mgpu::gpu1::ui_set_mvec_mode(3);
        }

        // ---- R351c: the model group, the preset curves and the two keys that need a re-arm ----
        void row_model_style()
        {
            mgpu::gpu1::ui_state st;
            mgpu::gpu1::ui_read(st);
            int v = (int)st.style;
            if (ImGui::RadioButton("A##style", v == 0)) mgpu::gpu1::ui_set_tuning_value(3, 0.0f);
            ImGui::SameLine();
            if (ImGui::RadioButton("B##style", v == 1)) mgpu::gpu1::ui_set_tuning_value(3, 1.0f);
            ImGui::SameLine();
            if (ImGui::RadioButton("C##style", v == 2)) mgpu::gpu1::ui_set_tuning_value(3, 2.0f);
        }

        void row_model_tone()
        {
            mgpu::gpu1::ui_state st;
            mgpu::gpu1::ui_read(st);
            float v = st.tone_strength;
            ImGui::SetNextItemWidth(190.0f);
            if (ImGui::SliderFloat("##tone", &v, 0.0f, 2.0f, "%.2f"))
                mgpu::gpu1::ui_set_tuning_value(0, v);
            ImGui::SameLine();
            if (ImGui::SmallButton("默认##tone")) mgpu::gpu1::ui_set_tuning_value(0, 1.00f);
        }

        void row_model_structure()
        {
            mgpu::gpu1::ui_state st;
            mgpu::gpu1::ui_read(st);
            float v = st.structure_strength;
            ImGui::SetNextItemWidth(190.0f);
            if (ImGui::SliderFloat("##structure", &v, 0.0f, 2.0f, "%.2f"))
                mgpu::gpu1::ui_set_tuning_value(1, v);
            ImGui::SameLine();
            if (ImGui::SmallButton("默认##structure")) mgpu::gpu1::ui_set_tuning_value(1, 1.00f);
        }

        void row_model_skin()
        {
            mgpu::gpu1::ui_state st;
            mgpu::gpu1::ui_read(st);
            float v = st.skin_strength;
            ImGui::SetNextItemWidth(190.0f);
            if (ImGui::SliderFloat("##skin", &v, 0.0f, 2.0f, "%.2f"))
                mgpu::gpu1::ui_set_tuning_value(2, v);
            ImGui::SameLine();
            if (ImGui::SmallButton("默认##skin")) mgpu::gpu1::ui_set_tuning_value(2, 1.00f);
        }

        void row_model_mask()
        {
            mgpu::gpu1::ui_state st;
            mgpu::gpu1::ui_read(st);
            bool on = st.auto_mask;
            if (ImGui::Checkbox("##automask", &on)) mgpu::gpu1::ui_set_tuning_value(4, on ? 1.0f : 0.0f);
        }

        void row_preset()
        {
            mgpu::gpu1::ui_state st;
            mgpu::gpu1::ui_read(st);
            int p = st.preset;
            if (ImGui::RadioButton("手动##preset", p == 0)) mgpu::gpu1::ui_set_preset(0);
            ImGui::SameLine();
            if (ImGui::RadioButton("前置##preset", p == 1)) mgpu::gpu1::ui_set_preset(1);
            ImGui::SameLine();
            if (ImGui::RadioButton("后置##preset", p == 2)) mgpu::gpu1::ui_set_preset(2);
        }

        // R351h: one slider, 0 = off. The checkbox that used to sit here read as "an empty box and a
        // number", which is exactly the ambiguity the simple panel is meant to remove.
        void row_frame_cap()
        {
            int v = mgpu::gpu1::ui_ini_read("FrameCap", 0);
            ImGui::SetNextItemWidth(150.0f);
            if (ImGui::SliderInt("##capfps", &v, 0, 240, (v == 0) ? "off" : "%d"))
                (void)mgpu::gpu1::ui_ini_write("FrameCap", v);
            ImGui::SameLine();
            if (ImGui::SmallButton("关闭##cap_zero")) (void)mgpu::gpu1::ui_ini_write("FrameCap", 0);
        }

        void row_colour16()
        {
            bool on = mgpu::gpu1::ui_ini_read("NRInput16", 0) != 0;
            if (ImGui::Checkbox("##n16", &on))
                (void)mgpu::gpu1::ui_ini_write("NRInput16", on ? 4 : 0);
        }

        void row_crop()
        {
            const int mode = mgpu::gpu1::ui_ini_read("ScaleMode", 0);
            const int auto_crop = mgpu::gpu1::ui_ini_read("CropAuto", 1);
            const int cur = (mode == 0) ? 0 : (auto_crop != 0 ? 2 : 1);
            if (ImGui::RadioButton("关##crop", cur == 0))
                (void)mgpu::gpu1::ui_ini_write("ScaleMode", 0);
            ImGui::SameLine();
            if (ImGui::RadioButton("开##crop", cur == 1))
            {
                (void)mgpu::gpu1::ui_ini_write("CropAuto", 0);
                (void)mgpu::gpu1::ui_ini_write("ScaleMode", 1);
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("自动##crop", cur == 2))
            {
                (void)mgpu::gpu1::ui_ini_write("CropAuto", 1);
                (void)mgpu::gpu1::ui_ini_write("ScaleMode", 1);
            }
        }

        // ---- R351d: the blocks that only exist on titles with DLSS, frame generation, or x2 ----
        // The bridge is not an FC6 add-on. On a title with native DLSS these are the controls a
        // player actually touches, so they belong in the simple panel too - and every one of them is
        // marked for what it is: live, or "needs a restart".
        void row_sr_enable()
        {
            bool on = mgpu::gpu1::ui_ini_read("SRUpscale", 0) != 0;
            if (ImGui::Checkbox("##sron", &on))
                (void)mgpu::gpu1::ui_ini_write("SRUpscale", on ? 1 : 0);
        }

        void row_sr_quality()
        {
            // The panel writes SRScale and SRUpscale as a PAIR - SRScale=0 with the low-res flag on
            // was rejected by NGX (FAIL_PlatformError), so the pair is written together here too.
            const int sc = mgpu::gpu1::ui_ini_read("SRScale", 0);
            const bool on = mgpu::gpu1::ui_ini_read("SRUpscale", 0) != 0;
            const int cur = !on ? -1 : (sc == 67 ? 2 : (sc == 58 ? 1 : 0));
            const char *names[3] = { "性能##srq", "均衡##srq", "质量##srq" };
            const int scales[3] = { 50, 58, 67 };
            for (int i = 0; i < 3; ++i)
            {
                if (i != 0) ImGui::SameLine();
                if (ImGui::RadioButton(names[i], cur == i))
                {
                    (void)mgpu::gpu1::ui_ini_write("SRUpscale", 1);
                    (void)mgpu::gpu1::ui_ini_write("SRScale", scales[i]);
                    (void)mgpu::gpu1::ui_ini_write("SRMvLowRes", 1);
                }
            }
            ImGui::SameLine();
        }

        void row_sr_preset()
        {
            const int pr = mgpu::gpu1::ui_ini_read("SRPreset", 0);
            if (ImGui::RadioButton("标题默认##srp", pr == 0))       (void)mgpu::gpu1::ui_ini_write("SRPreset", 0);
            ImGui::SameLine();
            if (ImGui::RadioButton("K##srp", pr == 11))          (void)mgpu::gpu1::ui_ini_write("SRPreset", 11);
            ImGui::SameLine();
            if (ImGui::RadioButton("L##srp", pr == 12))          (void)mgpu::gpu1::ui_ini_write("SRPreset", 12);
            ImGui::SameLine();
            if (ImGui::RadioButton("M##srp", pr == 13))          (void)mgpu::gpu1::ui_ini_write("SRPreset", 13);
        }

        void row_fg_mode()
        {
            const int fm = (int)mgpu::fgmap::ui_mode();
            if (ImGui::RadioButton("关##fg", fm == 0))
            { mgpu::fgmap::ui_set_mode(0); (void)mgpu::gpu1::ui_ini_write("NRFrameFilter", 0); }
            ImGui::SameLine();
            if (ImGui::RadioButton("修正向量##fg", fm == 2))
            { mgpu::fgmap::ui_set_mode(2); (void)mgpu::gpu1::ui_ini_write("NRFrameFilter", 2); }
            ImGui::SameLine();
            if (ImGui::RadioButton("修正+抽帧##fg", fm == 3))
            { mgpu::fgmap::ui_set_mode(3); (void)mgpu::gpu1::ui_ini_write("NRFrameFilter", 3); }
        }

        void row_fg_keep()
        {
            int n = (int)mgpu::fgmap::ui_keep();
            ImGui::SetNextItemWidth(110.0f);
            if (ImGui::SliderInt("##fgkeep", &n, 2, 8, "%d"))
                mgpu::fgmap::ui_set_keep((unsigned)n);
            if (ImGui::IsItemDeactivatedAfterEdit())
                (void)mgpu::gpu1::ui_ini_write("NRFrameKeep", (int)mgpu::fgmap::ui_keep());
            ImGui::SameLine();
            ImGui::TextDisabled("每 N 帧保留 1 张");
        }

        void row_x2_mode()
        {
            // Same three writes the full panel makes: Full is ResidualPass=0; Medium and Light are
            // ResidualPass=2 plus a 0.75 scale, differing only in ResidualRender.
            const int rp = mgpu::gpu1::ui_ini_read("ResidualPass", 0);
            const int rr = mgpu::gpu1::ui_ini_read("ResidualRender", 0);
            const int cur = (rp == 0) ? 2 : (rr == 1 ? 1 : 0);   // 0 Medium, 1 Light, 2 Full
            if (ImGui::RadioButton("中等##x2", cur == 0))
            {
                (void)mgpu::gpu1::ui_ini_write("ResidualPass", 2);
                (void)mgpu::gpu1::ui_ini_write_float("ResidualScale", 0.75f);
                (void)mgpu::gpu1::ui_ini_write("ResidualRender", 0);
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("轻量##x2", cur == 1))
            {
                (void)mgpu::gpu1::ui_ini_write("ResidualPass", 2);
                (void)mgpu::gpu1::ui_ini_write_float("ResidualScale", 0.75f);
                (void)mgpu::gpu1::ui_ini_write("ResidualRender", 1);
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("完整##x2", cur == 2))
                (void)mgpu::gpu1::ui_ini_write("ResidualPass", 0);
            ImGui::SameLine();
        }

        void row_nrs1()
        {
            char v[32] = {};
            mgpu::gpu1::ui_ini_read_word("NRScale", v, sizeof v);
            const bool on = (v[0] != 0 && atof(v) > 0.05);
            if (ImGui::RadioButton("off##nrs1", !on))
                (void)mgpu::gpu1::ui_ini_write_float("NRScale", 0.0f);
            ImGui::SameLine();
            if (ImGui::RadioButton("on##nrs1", on))
            {
                (void)mgpu::gpu1::ui_ini_write_float("NRScale", 0.75f);
                (void)mgpu::gpu1::ui_ini_write("NRScaleResolveDepth", 0);
            }
            ImGui::SameLine();
            ImGui::TextDisabled("pass 1 at 0.75, detail added back at full size (restart)");
        }

        void register_rows()
        {
            row("nrs.scale",  "NRScale",        &row_nrs_scale);
            row("nrs.gain",   "detail gain",    &row_nrs_gain);
            row("nrs.sharp",  "sharpness",      &row_nrs_sharp);
            row("nrs.colour", "colour strength",&row_nrs_colour);
            row("nrs.mode",   "mode",           &row_nrs_mode);
            // R351g: nrs1.on is the author's "Pass 1 scale (NRS)" On/Off radio, nrs.scale is the live
            // slider under "Neural scale (our own path)". BOTH already exist in 0.4.0 and both write
            // NRScale, so they are one control: the simple panel exposes a single slider and accepts
            // either id in the layout file for it.
            row("nrs1.on",    "pass 1 scale",   &row_nrs_scale);
            row("passes",     "passes",         &row_passes);
            row("intensity",  "intensity",      &row_intensity);
            row("intensity.preset", "intensity curve", &row_preset);
            row("view",       "view",           &row_view);
            row("mvec.mode",  "motion vectors", &row_mvec_mode);
            row("model.style",     "model style",  &row_model_style);
            row("model.tone",      "tone",         &row_model_tone);
            row("model.structure", "structure",    &row_model_structure);
            row("model.skin",      "skin",         &row_model_skin);
            row("model.mask",      "auto mask",    &row_model_mask);
            row("fps.cap",         "fps cap",      &row_frame_cap);
            row("colour.16bit",    "16-bit colour",&row_colour16);
            row("crop",            "crop",         &row_crop);
            // R351h: these seven were written but never registered, so the DLSS / frame-generation /
            // x2 sections came up empty. A row has to be BOTH drawn and registered.
            row("x2.mode",         "2nd pass (x2)",&row_x2_mode);
            row("sr.enable",       "DLSS on GPU 1",&row_sr_enable);
            row("sr.quality",      "DLSS quality", &row_sr_quality);
            row("sr.preset",       "DLSS preset",  &row_sr_preset);
            row("fg.mode",         "frame generation", &row_fg_mode);
            row("fg.keep",         "keep 1 in N",  &row_fg_keep);
        }
    }   // namespace

    void draw_simple_panel()
    {
        load_if_changed(false);
        register_rows();

        const char *title_zh = cjk() ? zh("panel.title") : nullptr;
        ImGui::TextUnformatted(title_zh != nullptr ? title_zh : "MGPU - neural rendering");
        help("panel.title");

        unsigned drawn = 0;
        for (const entry &e : g_layout)
        {
            if (e.id == "panel.title") continue;
            // R351e: a banner in the file becomes a section title here. An empty banner is just a
            // separator, so '======' on its own is a rule and '==== 画质 ====' is a heading.
            if (e.header)
            {
                if (e.zh.empty()) ImGui::Separator();
                else              ImGui::SeparatorText(e.zh.c_str());
                continue;
            }
            const row_def *r = find_row(e.id);
            if (r == nullptr) { report_once(e.id, "no control in this build has that id"); continue; }
            ImGui::PushID(e.id.c_str());
            // The label and help come from the file, the control itself from the same code the full
            // panel uses - so the simple panel cannot drift away from what it is controlling.
            // One row per control: label in a fixed left column, the control beside it, "?" last.
            label(e.id.c_str(), r->en.c_str());
            ImGui::SameLine(150.0f);
            r->fn();
            help(e.id.c_str());
            ImGui::PopID();
            ++drawn;
        }
        if (drawn == 0)
            ImGui::TextDisabled("panel_simple.txt has no row this build can draw.");

        ImGui::Separator();
        if (ImGui::Button("高级  >>"))
            set_enabled(false);
        ImGui::SameLine();
        if (ImGui::SmallButton("重新加载"))
            reload();
    }

    void draw_editor()
    {
        load_if_changed(false);
        ImGui::SeparatorText("Simple panel");
        bool on = mgpu::gpu1::ui_ini_read("PanelSimple", 0) != 0;
        if (ImGui::Checkbox("PanelSimple - show the simple panel instead of this one", &on))
            set_enabled(on);
        ImGui::SameLine();
        ImGui::TextDisabled("cjk font: %s | %d row(s) in panel_simple.txt, %d registered in this build",
                            cjk() ? "yes" : "NO (labels fall back to English)",
                            (int)g_layout.size(), (int)g_rows.size());
        if (ImGui::SmallButton("open panel_simple.txt")) open_editor();
        ImGui::SameLine();
        if (ImGui::SmallButton("export items")) export_items();
        ImGui::SameLine();
        if (ImGui::SmallButton("重新加载")) reload();
        ImGui::TextDisabled("A row appears in the simple panel only if panel_simple.txt names it, and"
                            " it must have a control in this build to be drawn.");
    }
}
#else

    // No ImGui headers in this build: the panel does not exist, so neither does the simple mode.
    bool cjk() { return false; }
    void label(const char *, const char *) {}
    void help(const char *) {}
    void draw_simple_panel() {}
    void draw_editor() {}

#endif  // MGPU_HAVE_IMGUI
