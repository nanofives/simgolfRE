// P24D (log/roadmap/P24/P24D_notes.md): the game's text in another language, drawing only.
//
// A LANGUAGE PACK is a UTF-8 text file of `english<TAB>translation` lines (`#` starts a comment line; `\n`, `\t`, `\\` escape a
// newline, a tab and a backslash). It is not part of the repository: the user's own install carries it in game\Lang\ (the LAN
// build's stager copies it there from the gitignored lang\ folder). The English side is the text exactly as the game hands it to
// a text drawer; a printf conversion in it (%s %d %i %u %c %x, flags / width / precision / l h allowed, %% a literal percent)
// matches a variable part: %s any non-empty text, %c one byte, the numeric ones an optional '-' and digits (',' and '.' between
// digits too). The translation must hold the same conversions in the same order (an entry that does not is refused at load);
// each is replaced by the text its English conversion matched. Exact lines are looked up first, then the patterns, the one
// with the most literal bytes winning.
//
// WHERE IT APPLIES. Only where the graphics backend fetches a text to draw or to measure for drawing (lang::redirect on the
// text's address, lang::tr on a text the backend built), never where the simulation reads a text (names, keys, file names,
// saves): the GameState is the same whatever the language. A redirected text lives in a host block mapped into the context's
// address space at kLangVa (outside the GameState, so no state hash sees it), so every downstream reader that takes an address
// (the provider's glyph_width, the markup scanner) reads the translation.
//
// The language is process-wide (the app's selector): set_language switches it at the next fetch, no restart.
#ifndef PORTSIM_SIM_LANG_H
#define PORTSIM_SIM_LANG_H

#include <cstdint>
#include <string>
#include <vector>

namespace portsim {

class Ctx;

namespace lang {

enum : int { kEnglish = 0, kSpanish = 1, kCount = 2 };
constexpr uint32_t kLangVa = 0xFC000000u, kLangSize = 4u << 20;  // the redirected texts' block (4 MB, reused when full)

// Parse pack `utf8` for language `id` (replacing the one it had). Returns the number of entries kept; refused lines are
// appended to `errors` ("line N: why").
int load_pack(int id, const std::string &utf8, std::vector<std::string> *errors);
bool has_pack(int id);
void set_language(int id);  // kEnglish (the game's own text) or a language with a pack; others are ignored
// P24J: a %s part of a pattern's match that has a translation of its own is translated, this many levels deep (a comment tip is
// tip -> 'thought' Name -> thought -> tile: three). set_capture_depth changes it (clears the cached results; tools' checks only)
constexpr int kCaptureDepth = 3;
void set_capture_depth(int levels);
int language();
// The translation of `s` (code page 1252 bytes) in the current language; `s` when there is none (always in English)
std::string tr(const std::string &s);
// `va`'s text translated: the address of its translation in the kLangVa block, or `va` itself (no translation, English, 0)
uint32_t redirect(Ctx &ctx, uint32_t va);
// SG_LANG_DUMP=<file> (read once): every distinct text tr / redirect saw, `hit|miss<TAB>where<TAB>text` (UTF-8, escaped)
void note(const char *where, const std::string &s, bool hit);

// helpers shared with the tools' checks
std::string utf8_to_cp1252(const std::string &u, bool *lossy = nullptr);
std::string cp1252_to_utf8(const std::string &s);
// the conversions of a printf format, in order ("%s", "%d", "%5.1f" ...; "%%" is not one)
std::vector<std::string> conversions(const std::string &fmt);

}  // namespace lang
}  // namespace portsim

#endif  // PORTSIM_SIM_LANG_H
