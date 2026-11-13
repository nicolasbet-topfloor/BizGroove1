#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <cstdint>
#include <map>
#include <vector>

namespace bizgroove
{

/** Text to speech sounds, for the Voice instrument: words in Spanish or English
    are turned into a list of speech sounds (phonemes), each with its formants
    (the resonances of the mouth that make an "a" an "a"), how much buzz and
    hiss it has and how long it lasts. Speech.h only writes the list; the
    Voice instrument (VoiceSynth.h) speaks it.

    The rules are simple, like the speaking toys and synths of the 1980s: Spanish
    is written much as it sounds and comes out well; English spelling is not, so
    common words are looked up in a small dictionary and the rest follow rules,
    with the odd word mispronounced, robot style. */
namespace speech
{

enum Kind : std::uint8_t { vowel = 0, glide, liquid, tap, trill, nasal, fricative, closure, burst, aspirate, pause };

/** One speech sound: its formants (Hz), how much voice (buzz) and frication
    (hiss) it has, the hiss's centre and spread, and how long it lasts (ms). */
struct PhoneDef
{
    const char* name;
    Kind kind;
    float f1, f2, f3;
    float voice, hiss, hissHz, hissWidth;
    float ms;
};

enum Phone : std::uint8_t
{
    // vowels
    A = 0, E, I, O, U, AE, AA, AO, EH, IH, UH, AH, AX, ER,
    // glides and liquids
    Yg, W, YY, L, Rtap, RR, Ren,
    // nasals
    M, N, NY, NG,
    // fricatives
    F, V, TH, DH, S, Z, SH, ZH, X, H,
    // stops: the silence (closure) and the release (burst) of each
    Pc, Pb, Bc, Bb, Tc, Tb, Dc, Db, Kc, Kb, Gc, Gb,
    // pauses
    Gap, Comma, Stop,
    numPhones
};

inline const PhoneDef& phone (int p)
{
    static const PhoneDef table[numPhones] = {
        // name   kind       F1    F2    F3     voice hiss  hissHz hissW   ms
        { "a",  vowel,      750, 1300, 2550,   1.0f, 0.0f,    0,    0,  100 },
        { "e",  vowel,      470, 1950, 2600,   1.0f, 0.0f,    0,    0,   90 },
        { "i",  vowel,      290, 2300, 3000,   1.0f, 0.0f,    0,    0,   85 },
        { "o",  vowel,      480,  880, 2500,   1.0f, 0.0f,    0,    0,   95 },
        { "u",  vowel,      320,  780, 2350,   1.0f, 0.0f,    0,    0,   90 },
        { "ae", vowel,      680, 1720, 2450,   1.0f, 0.0f,    0,    0,  120 },
        { "aa", vowel,      720, 1100, 2450,   1.0f, 0.0f,    0,    0,  120 },
        { "ao", vowel,      580,  850, 2450,   1.0f, 0.0f,    0,    0,  115 },
        { "eh", vowel,      560, 1800, 2500,   1.0f, 0.0f,    0,    0,   95 },
        { "ih", vowel,      400, 1950, 2550,   1.0f, 0.0f,    0,    0,   80 },
        { "uh", vowel,      450, 1050, 2250,   1.0f, 0.0f,    0,    0,   80 },
        { "ah", vowel,      640, 1200, 2400,   1.0f, 0.0f,    0,    0,   90 },
        { "ax", vowel,      500, 1450, 2500,   1.0f, 0.0f,    0,    0,   60 },
        { "er", vowel,      480, 1350, 1650,   1.0f, 0.0f,    0,    0,  120 },

        { "y",  glide,      280, 2250, 3000,   0.85f, 0.0f,   0,    0,   45 },
        { "w",  glide,      300,  650, 2200,   0.85f, 0.0f,   0,    0,   50 },
        { "yy", glide,      280, 2200, 2950,   0.8f, 0.12f, 3000, 1500,  65 },
        { "l",  liquid,     360, 1250, 2650,   0.75f, 0.0f,   0,    0,   60 },
        { "r",  tap,        450, 1450, 2400,   0.45f, 0.0f,   0,    0,   24 },
        { "rr", trill,      450, 1450, 2400,   0.6f, 0.0f,    0,    0,   95 },
        { "R",  liquid,     330, 1100, 1450,   0.8f, 0.0f,    0,    0,   65 },

        { "m",  nasal,      280, 1050, 2200,   0.7f, 0.0f,    0,    0,   70 },
        { "n",  nasal,      280, 1650, 2600,   0.7f, 0.0f,    0,    0,   65 },
        { "ny", nasal,      280, 2100, 2800,   0.7f, 0.0f,    0,    0,   75 },
        { "ng", nasal,      280, 2250, 2750,   0.7f, 0.0f,    0,    0,   70 },

        { "f",  fricative,  300, 1100, 2300,   0.0f, 0.25f, 6500, 6000,  95 },
        { "v",  fricative,  300, 1100, 2300,   0.5f, 0.12f, 6500, 6000,  70 },
        { "th", fricative,  320, 1400, 2600,   0.0f, 0.2f,  6000, 6000,  95 },
        { "dh", fricative,  320, 1400, 2600,   0.5f, 0.1f,  6000, 6000,  55 },
        { "s",  fricative,  320, 1700, 2600,   0.0f, 1.0f,  6500, 2200, 105 },
        { "z",  fricative,  320, 1700, 2600,   0.5f, 0.55f, 6500, 2200,  80 },
        { "sh", fricative,  300, 1900, 2400,   0.0f, 0.9f,  3000, 1600, 105 },
        { "zh", fricative,  300, 1900, 2400,   0.5f, 0.5f,  3000, 1600,  80 },
        { "x",  fricative,  350, 1300, 2500,   0.0f, 0.55f, 1800, 1500,  90 },
        { "h",  aspirate,   500, 1500, 2500,   0.0f, 0.0f,     0,   0,   60 },

        { "p-", closure,    300,  900, 2200,   0.0f, 0.0f,    0,    0,   60 },
        { "p",  burst,      300,  900, 2200,   0.0f, 0.4f,   900, 1500,  18 },
        { "b-", closure,    250,  900, 2200,   0.25f, 0.0f,   0,    0,   45 },
        { "b",  burst,      300,  900, 2200,   0.3f, 0.25f,  900, 1500,  10 },
        { "t-", closure,    320, 1700, 2600,   0.0f, 0.0f,    0,    0,   60 },
        { "t",  burst,      320, 1700, 2600,   0.0f, 0.7f,  4500, 2500,  18 },
        { "d-", closure,    250, 1700, 2600,   0.25f, 0.0f,   0,    0,   45 },
        { "d",  burst,      320, 1700, 2600,   0.3f, 0.4f,  4500, 2500,  10 },
        { "k-", closure,    300, 2000, 2300,   0.0f, 0.0f,    0,    0,   60 },
        { "k",  burst,      300, 2000, 2300,   0.0f, 0.7f,  2200, 1200,  22 },
        { "g-", closure,    250, 2000, 2300,   0.25f, 0.0f,   0,    0,   45 },
        { "g",  burst,      300, 2000, 2300,   0.3f, 0.4f,  2200, 1200,  12 },

        { "_",  pause,      500, 1500, 2500,   0.0f, 0.0f,    0,    0,   15 },
        { ",",  pause,      500, 1500, 2500,   0.0f, 0.0f,    0,    0,  180 },
        { ".",  pause,      500, 1500, 2500,   0.0f, 0.0f,    0,    0,  350 }
    };
    return table[juce::jlimit (0, (int) numPhones - 1, p)];
}

/** One sound in the list: which, for how long, and its pitch (from the intonation). */
struct Seg
{
    enum Flags : std::uint8_t { stressed = 1, lastVowel = 2 };
    std::uint8_t phone = Gap;
    std::uint8_t flags = 0;
    std::int16_t cents = 0;      // pitch from the intonation, before the Intonation knob
    std::uint16_t ms = 0;
    std::uint16_t word = 0;
};

/** A whole text, ready to speak. The words are numbered, so the Voice can say
    one word per note. */
struct Script
{
    static constexpr int maxSegs = 1024, maxWords = 160;
    std::array<Seg, maxSegs> segs {};
    int numSegs = 0;
    std::array<std::uint16_t, maxWords + 1> wordStart {};   // first segment of each word; wordStart[numWords] = numSegs
    int numWords = 0;
};

enum Language { spanish = 0, english = 1, numLanguages };

inline juce::String languageName (int l) { return l == english ? "English" : juce::String::fromUTF8 ("Espa\xc3\xb1ol"); }

// ============================================================ text to sounds

/** A sound of a word, before it is laid out in time. */
struct Tok
{
    int phone;
    bool stressed = false;
};

namespace detail
{
    inline bool isSpanishVowel (juce::juce_wchar c) { return juce::String::fromUTF8 ("aeiou\xc3\xa1\xc3\xa9\xc3\xad\xc3\xb3\xc3\xba\xc3\xbc").containsChar (c); }
    inline bool isAccented (juce::juce_wchar c)     { return juce::String::fromUTF8 ("\xc3\xa1\xc3\xa9\xc3\xad\xc3\xb3\xc3\xba").containsChar (c); }
    inline bool isFrontSpanish (juce::juce_wchar c) { return juce::String::fromUTF8 ("ei\xc3\xa9\xc3\xad").containsChar (c); }

    inline int spanishVowelPhone (juce::juce_wchar c)
    {
        switch (c)
        {
            case 'a': case 0xe1: return A;
            case 'e': case 0xe9: return E;
            case 'i': case 0xed: return I;
            case 'o': case 0xf3: return O;
            default:             return U;   // u, ú, ü
        }
    }

    inline bool isWeak (juce::juce_wchar c) { return c == 'i' || c == 'u' || c == 0xfc; }

    /** Spanish (Latin American: c and z sound as s, ll and y the same). */
    inline std::vector<Tok> spanishWord (const juce::String& w)
    {
        std::vector<Tok> out;
        std::vector<int> nuclei;   // indices into out of the vowels that carry syllables
        int accented = -1;
        const int n = w.length();
        auto at = [&w, n] (int i) -> juce::juce_wchar { return i >= 0 && i < n ? w[i] : (juce::juce_wchar) 0; };

        for (int i = 0; i < n; ++i)
        {
            const auto c = w[i], next = at (i + 1), prev = at (i - 1);
            if (isSpanishVowel (c))
            {
                // An unaccented i or u next to another vowel glides into it (diphthong): "bien", "auto".
                const bool weak = isWeak (c);
                const bool glideBefore = weak && isSpanishVowel (next) && ! (isWeak (next) && i + 2 >= n);
                const bool glideAfter = weak && isSpanishVowel (prev) && ! isWeak (prev);
                if (glideBefore || glideAfter)
                    out.push_back ({ c == 'i' ? (int) Yg : (int) W });
                else
                {
                    if (isAccented (c))
                        accented = (int) nuclei.size();
                    nuclei.push_back ((int) out.size());
                    out.push_back ({ spanishVowelPhone (c) });
                }
                continue;
            }
            switch (c)
            {
                case 'b': case 'v': out.push_back ({ Bc }); out.push_back ({ Bb }); break;
                case 'c':
                    if (next == 'h')            { out.push_back ({ Tc }); out.push_back ({ SH }); ++i; }
                    else if (isFrontSpanish (next)) out.push_back ({ S });
                    else                        { out.push_back ({ Kc }); out.push_back ({ Kb }); }
                    break;
                case 'd': out.push_back ({ Dc }); out.push_back ({ Db }); break;
                case 'f': out.push_back ({ F }); break;
                case 'g':
                    if (isFrontSpanish (next)) out.push_back ({ X });
                    else
                    {
                        out.push_back ({ Gc }); out.push_back ({ Gb });
                        if (next == 'u' && isFrontSpanish (at (i + 2))) ++i;                   // "gue", "gui": the u is silent
                        else if (next == 0xfc) { out.push_back ({ W }); ++i; }                // "pingüino"
                    }
                    break;
                case 'h': break;   // silent
                case 'j': out.push_back ({ X }); break;
                case 'k': out.push_back ({ Kc }); out.push_back ({ Kb }); break;
                case 'l':
                    if (next == 'l') { out.push_back ({ YY }); ++i; }
                    else out.push_back ({ L });
                    break;
                case 'm': out.push_back ({ M }); break;
                case 'n':
                    if (next == 'b' || next == 'p' || next == 'v' || next == 'm') out.push_back ({ M });
                    else if (next == 'g' || next == 'k' || next == 'j' || (next == 'c' && ! isFrontSpanish (at (i + 2)) && at (i + 2) != 'h')) out.push_back ({ NG });
                    else out.push_back ({ N });
                    break;
                case 0xf1: out.push_back ({ NY }); break;   // ñ
                case 'p': out.push_back ({ Pc }); out.push_back ({ Pb }); break;
                case 'q':
                    out.push_back ({ Kc }); out.push_back ({ Kb });
                    if (next == 'u') ++i;
                    break;
                case 'r':
                    if (next == 'r') { out.push_back ({ RR }); ++i; }
                    else if (i == 0 || prev == 'n' || prev == 'l' || prev == 's') out.push_back ({ RR });
                    else out.push_back ({ Rtap });
                    break;
                case 's': out.push_back ({ S }); break;
                case 't': out.push_back ({ Tc }); out.push_back ({ Tb }); break;
                case 'w': out.push_back ({ W }); break;
                case 'x': out.push_back ({ Kc }); out.push_back ({ Kb }); out.push_back ({ S }); break;
                case 'y':
                    if (n == 1 || i == n - 1)
                    {
                        if (isSpanishVowel (prev)) out.push_back ({ Yg });              // "hoy", "muy"
                        else { nuclei.push_back ((int) out.size()); out.push_back ({ I }); }   // "y"
                    }
                    else out.push_back ({ YY });
                    break;
                case 'z': out.push_back ({ S }); break;
                default: break;
            }
        }

        // Stress: the accented vowel; otherwise the one before last when the word ends in a vowel, n or s, else the last.
        static const juce::StringArray clitics { "el", "la", "los", "las", "lo", "le", "les", "de", "del", "al", "en", "y", "a",
                                                 "que", "un", "una", "mi", "tu", "su", "se", "me", "te", "nos", "con", "por", "o", "e", "u" };
        if (! nuclei.empty() && ! clitics.contains (w))
        {
            int s = accented;
            if (s < 0)
            {
                const auto last = w.getLastCharacter();
                const bool penult = isSpanishVowel (last) || last == 'n' || last == 's' || last == 'y';
                s = (penult && nuclei.size() >= 2) ? (int) nuclei.size() - 2 : (int) nuclei.size() - 1;
            }
            out[(size_t) nuclei[(size_t) s]].stressed = true;
        }
        return out;
    }

    // ---------------------------------------------------------------- English

    inline int englishPhoneByName (const juce::String& s)
    {
        static const std::map<juce::String, int> names {
            { "AA", AA }, { "AE", AE }, { "AH", AH }, { "AO", AO }, { "AX", AX }, { "EH", EH }, { "ER", ER },
            { "IH", IH }, { "UH", UH }, { "i", I }, { "u", U }, { "e", E }, { "o", O }, { "a", A },
            { "y", Yg }, { "w", W }, { "l", L }, { "R", Ren }, { "m", M }, { "n", N }, { "NG", NG },
            { "f", F }, { "v", V }, { "TH", TH }, { "DH", DH }, { "s", S }, { "z", Z }, { "SH", SH }, { "ZH", ZH }, { "h", H },
            { "p", Pb }, { "b", Bb }, { "t", Tb }, { "d", Db }, { "k", Kb }, { "g", Gb }
        };
        const auto it = names.find (s);
        return it != names.end() ? it->second : -1;
    }

    /** Adds one English sound by its dictionary name; diphthongs and affricates become two. */
    inline void addEnglish (std::vector<Tok>& out, const juce::String& name, bool stressed)
    {
        auto two = [&out, stressed] (int a, int b) { out.push_back ({ a, stressed }); out.push_back ({ b }); };
        if (name == "EY") return two (E, IH);
        if (name == "AY") return two (A, IH);
        if (name == "AW") return two (A, UH);
        if (name == "OW") return two (O, UH);
        if (name == "OY") return two (AO, IH);
        if (name == "CH") { out.push_back ({ Tc }); out.push_back ({ SH }); return; }
        if (name == "JH") { out.push_back ({ Dc }); out.push_back ({ ZH }); return; }
        const int p = englishPhoneByName (name);
        if (p < 0)
            return;
        // Stops: a closure before the burst; p, t and k breathe out a little after.
        if (p == Pb || p == Tb || p == Kb || p == Bb || p == Db || p == Gb)
        {
            out.push_back ({ p - 1 });
            out.push_back ({ p });
            if (p == Pb || p == Tb || p == Kb)
                out.push_back ({ H });
            return;
        }
        out.push_back ({ p, stressed });
    }

    inline const std::map<juce::String, const char*>& englishDictionary()
    {
        // The words English spelling gets wrong, and the most common ones. ' marks the stressed vowel when it isn't the first.
        static const std::map<juce::String, const char*> dict {
            { "the", "DH AX" }, { "a", "AX" }, { "an", "AE n" }, { "and", "AE n d" }, { "to", "t u" }, { "of", "AH v" },
            { "in", "IH n" }, { "is", "IH z" }, { "it", "IH t" }, { "you", "y u" }, { "that", "DH AE t" }, { "he", "h i" },
            { "was", "w AH z" }, { "for", "f AO R" }, { "on", "AA n" }, { "are", "AA R" }, { "as", "AE z" }, { "with", "w IH DH" },
            { "his", "h IH z" }, { "they", "DH EY" }, { "i", "AY" }, { "at", "AE t" }, { "be", "b i" }, { "this", "DH IH s" },
            { "have", "h AE v" }, { "from", "f R AH m" }, { "or", "AO R" }, { "one", "w AH n" }, { "had", "h AE d" }, { "by", "b AY" },
            { "word", "w ER d" }, { "words", "w ER d z" }, { "but", "b AH t" }, { "not", "n AA t" }, { "what", "w AH t" }, { "all", "AO l" },
            { "were", "w ER" }, { "we", "w i" }, { "when", "w EH n" }, { "your", "y AO R" }, { "can", "k AE n" }, { "said", "s EH d" },
            { "there", "DH EH R" }, { "their", "DH EH R" }, { "they're", "DH EH R" }, { "use", "y u z" }, { "each", "i CH" }, { "which", "w IH CH" },
            { "she", "SH i" }, { "do", "d u" }, { "does", "d AH z" }, { "done", "d AH n" }, { "how", "h AW" }, { "if", "IH f" },
            { "will", "w IH l" }, { "up", "AH p" }, { "other", "AH DH ER" }, { "about", "AX b 'AW t" }, { "out", "AW t" },
            { "many", "m EH n i" }, { "then", "DH EH n" }, { "them", "DH EH m" }, { "these", "DH i z" }, { "those", "DH OW z" },
            { "so", "s OW" }, { "some", "s AH m" }, { "her", "h ER" }, { "would", "w UH d" }, { "could", "k UH d" }, { "should", "SH UH d" },
            { "make", "m EY k" }, { "like", "l AY k" }, { "him", "h IH m" }, { "into", "IH n t u" }, { "time", "t AY m" }, { "has", "h AE z" },
            { "look", "l UH k" }, { "more", "m AO R" }, { "go", "g OW" }, { "see", "s i" }, { "no", "n OW" }, { "way", "w EY" },
            { "people", "p i p AX l" }, { "my", "m AY" }, { "than", "DH AE n" }, { "first", "f ER s t" }, { "water", "w AO t ER" },
            { "been", "b IH n" }, { "call", "k AO l" }, { "who", "h u" }, { "now", "n AW" }, { "find", "f AY n d" }, { "day", "d EY" },
            { "did", "d IH d" }, { "get", "g EH t" }, { "give", "g IH v" }, { "come", "k AH m" }, { "made", "m EY d" }, { "may", "m EY" },
            { "hello", "h AX l 'OW" }, { "world", "w ER l d" }, { "love", "l AH v" }, { "music", "m y u z IH k" }, { "spell", "s p EH l" },
            { "speak", "s p i k" }, { "computer", "k AX m p 'y u t ER" }, { "robot", "R OW b AA t" }, { "yes", "y EH s" }, { "please", "p l i z" },
            { "thank", "TH AE NG k" }, { "thanks", "TH AE NG k s" }, { "good", "g UH d" }, { "night", "n AY t" }, { "dance", "d AE n s" },
            { "groove", "g R u v" }, { "biz", "b IH z" }, { "bizgroove", "b IH z g R u v" }, { "am", "AE m" }, { "me", "m i" }, { "our", "AW ER" },
            { "us", "AH s" }, { "know", "n OW" }, { "show", "SH OW" }, { "slow", "s l OW" }, { "low", "l OW" }, { "grow", "g R OW" },
            { "snow", "s n OW" }, { "blow", "b l OW" }, { "flow", "f l OW" }, { "window", "w IH n d OW" }, { "yellow", "y EH l OW" },
            { "follow", "f AA l OW" }, { "want", "w AA n t" }, { "here", "h IH R" }, { "says", "s EH z" }, { "gone", "g AO n" },
            { "again", "AX g 'EH n" }, { "eye", "AY" }, { "heart", "h AA R t" }, { "baby", "b EY b i" }, { "every", "EH v R i" },
            { "only", "OW n l i" }, { "very", "v EH R i" }, { "any", "EH n i" }, { "friend", "f R EH n d" }, { "school", "s k u l" },
            { "where", "w EH R" }, { "why", "w AY" }, { "because", "b IH k 'AH z" }, { "zero", "z IH R OW" }, { "two", "t u" },
            { "three", "TH R i" }, { "four", "f AO R" }, { "five", "f AY v" }, { "six", "s IH k s" }, { "seven", "s EH v AX n" },
            { "eight", "EY t" }, { "nine", "n AY n" }, { "ten", "t EH n" }, { "oh", "OW" }, { "ok", "OW k 'EY" }, { "okay", "OW k 'EY" },
            { "great", "g R EY t" }, { "machine", "m AX SH 'i n" }, { "voice", "v OY s" }, { "sing", "s IH NG" }, { "song", "s AO NG" },
            { "danger", "d EY n JH ER" }, { "woman", "w UH m AX n" }, { "women", "w IH m IH n" }, { "move", "m u v" }, { "body", "b AA d i" },
            { "touch", "t AH CH" }, { "young", "y AH NG" }, { "put", "p UH t" }, { "full", "f UH l" }, { "push", "p UH SH" }, { "pull", "p UH l" },
            { "fire", "f AY ER" }, { "light", "l AY t" }, { "right", "R AY t" }, { "high", "h AY" }, { "life", "l AY f" }, { "live", "l IH v" },
            { "mother", "m AH DH ER" }, { "father", "f AA DH ER" }, { "brother", "b R AH DH ER" }, { "nothing", "n AH TH IH NG" },
            { "something", "s AH m TH IH NG" }, { "together", "t AX g 'EH DH ER" }, { "today", "t AX d 'EY" }, { "tonight", "t AX n 'AY t" },
            { "forever", "f AO R 'EH v ER" }, { "future", "f y u CH ER" }, { "space", "s p EY s" }, { "star", "s t AA R" }, { "stars", "s t AA R z" }
        };
        return dict;
    }

    inline bool isEnglishVowel (juce::juce_wchar c) { return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'y'; }
    inline bool isPlainVowel (juce::juce_wchar c)   { return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u'; }

    /** English by rule, for words not in the dictionary. */
    inline std::vector<Tok> englishByRule (const juce::String& w)
    {
        std::vector<juce::String> names;   // sound names, as in the dictionary
        std::vector<bool> isNucleus;
        auto add = [&names, &isNucleus] (const char* list, bool nucleusFirst = false)
        {
            const auto parts = juce::StringArray::fromTokens (list, " ", "");
            for (int i = 0; i < parts.size(); ++i)
            {
                names.push_back (parts[i]);
                isNucleus.push_back (nucleusFirst && i == 0);
            }
        };

        const int n = w.length();
        auto at = [&w, n] (int i) -> juce::juce_wchar { return i >= 0 && i < n ? w[i] : (juce::juce_wchar) 0; };
        auto is = [&w] (int i, const char* s) { return w.substring (i).startsWith (s); };
        int vowelsSeen = 0;

        // The final silent e: "make", "time", "home". A long vowel before one consonant and it.
        const bool magicE = n >= 3 && w.getLastCharacter() == 'e' && ! isEnglishVowel (at (n - 2)) && isPlainVowel (at (n - 3));
        const int magicVowel = magicE ? n - 3 : -1;

        for (int i = 0; i < n; ++i)
        {
            const auto c = w[i], next = at (i + 1), prev = at (i - 1);
            const bool reduce = vowelsSeen > 0;   // vowels after the first (stressed) one are weak: "banana"

            // ---- vowels
            if (isPlainVowel (c))
            {
                if (c == 'e' && i == n - 1 && vowelsSeen > 0) continue;                                // silent final e
                if (c == 'e' && i == n - 2 && (next == 's' || next == 'd') && vowelsSeen > 0
                    && ! (next == 'd' && (prev == 't' || prev == 'd'))) continue;                         // "makes", "named"
                ++vowelsSeen;
                if (is (i, "igh"))  { add ("AY", true); i += 2; continue; }
                if (is (i, "tion")) { add ("SH AX n", true); i += 3; continue; }
                if (is (i, "air"))  { add ("EH R", true); i += 2; continue; }
                if (is (i, "ear"))  { add ("IH R", true); i += 2; continue; }
                if (is (i, "all"))  { add ("AO l", true); i += 2; continue; }
                if (is (i, "alk"))  { add ("AO k", true); i += 2; continue; }
                if (is (i, "ee") || is (i, "ea")) { add ("i", true); ++i; continue; }
                if (is (i, "ie"))   { add (i + 2 >= n ? "AY" : "i", true); ++i; continue; }
                if (is (i, "ei") || is (i, "ai") || is (i, "ay")) { add ("EY", true); ++i; continue; }
                if (is (i, "oa"))   { add ("OW", true); ++i; continue; }
                if (is (i, "oo"))   { add (at (i + 2) == 'k' ? "UH" : "u", true); ++i; continue; }
                if (is (i, "ou"))   { add ("AW", true); ++i; continue; }
                if (is (i, "ow"))   { add (i + 2 >= n ? "OW" : "AW", true); ++i; continue; }
                if (is (i, "oi") || is (i, "oy")) { add ("OY", true); ++i; continue; }
                if (is (i, "au") || is (i, "aw")) { add ("AO", true); ++i; continue; }
                if (is (i, "ew") || is (i, "ue") || is (i, "ui")) { add ("u", true); ++i; continue; }
                if (next == 'r' && ! isEnglishVowel (at (i + 2)))
                {
                    if (c == 'a') add ("AA R", true);
                    else if (c == 'o') add ("AO R", true);
                    else add ("ER", true);
                    ++i;
                    continue;
                }
                if (i == magicVowel)
                {
                    add (c == 'a' ? "EY" : c == 'e' ? "i" : c == 'i' ? "AY" : c == 'o' ? "OW" : "u", true);
                    continue;
                }
                if (i == n - 1)   // a vowel at the very end
                {
                    add (c == 'o' ? "OW" : c == 'a' ? "AX" : c == 'u' ? "u" : "i", true);
                    continue;
                }
                // Open first syllable (one consonant, then a vowel): a long vowel, "paper", "music".
                if (! reduce && ! isEnglishVowel (next) && next != 0 && isPlainVowel (at (i + 2)) && at (i + 2) != 'e' && next != 'x')
                {
                    add (c == 'a' ? "EY" : c == 'e' ? "i" : c == 'i' ? "AY" : c == 'o' ? "OW" : "u", true);
                    continue;
                }
                if (reduce)
                    add (c == 'i' ? "IH" : "AX", true);
                else
                    add (c == 'a' ? "AE" : c == 'e' ? "EH" : c == 'i' ? "IH" : c == 'o' ? "AA" : "AH", true);
                continue;
            }
            if (c == 'y')
            {
                if (i == 0 && isPlainVowel (next)) { add ("y"); continue; }
                ++vowelsSeen;
                if (i == n - 1) add (vowelsSeen == 1 ? "AY" : "i", true);
                else add ("IH", true);
                continue;
            }

            // ---- consonants
            if (c == next && c != 'c') continue;   // double letters: one sound ("ll", "tt")
            if (is (i, "tch")) { add ("CH"); i += 2; continue; }
            if (is (i, "ch"))  { add ("CH"); ++i; continue; }
            if (is (i, "sh"))  { add ("SH"); ++i; continue; }
            if (is (i, "th"))  { add ("TH"); ++i; continue; }
            if (is (i, "ph"))  { add ("f"); ++i; continue; }
            if (is (i, "wh"))  { add ("w"); ++i; continue; }
            if (is (i, "ck"))  { add ("k"); ++i; continue; }
            if (is (i, "ng"))  { add ("NG"); ++i; continue; }
            if (is (i, "nk"))  { add ("NG k"); ++i; continue; }
            if (is (i, "qu"))  { add ("k w"); ++i; continue; }
            if (is (i, "gh"))  { ++i; continue; }
            if (is (i, "dge")) { add ("JH"); i += 2; continue; }
            if (is (i, "sion")) { add (isPlainVowel (prev) ? "ZH AX n" : "SH AX n"); i += 3; continue; }
            if (is (i, "ture")) { add ("CH ER"); i += 3; continue; }
            if (i == 0 && is (i, "kn")) { add ("n"); ++i; continue; }
            if (i == 0 && is (i, "wr")) { add ("R"); ++i; continue; }
            if (is (i, "mb") && i + 2 == n) { add ("m"); ++i; continue; }
            if (is (i, "le") && i + 2 == n && i > 0 && ! isEnglishVowel (prev)) { add ("AX l"); ++i; continue; }
            if (is (i, "ed") && i + 2 == n && vowelsSeen > 0)
            {
                if (prev == 't' || prev == 'd') add ("IH d");
                else add (juce::String ("pkfsx").containsChar (prev) || (i >= 2 && (is (i - 2, "sh") || is (i - 2, "ch"))) ? "t" : "d");
                ++i;
                continue;
            }
            const bool frontNext = next == 'e' || next == 'i' || next == 'y';
            switch (c)
            {
                case 'b': add ("b"); break;
                case 'c': add (frontNext ? "s" : "k"); break;
                case 'd': add ("d"); break;
                case 'f': add ("f"); break;
                case 'g': add (frontNext ? "JH" : "g"); break;
                case 'h': add ("h"); break;
                case 'j': add ("JH"); break;
                case 'k': add ("k"); break;
                case 'l': add ("l"); break;
                case 'm': add ("m"); break;
                case 'n': add ("n"); break;
                case 'p': add ("p"); break;
                case 'q': add ("k"); break;
                case 'r': add ("R"); break;
                case 's':
                    if ((isEnglishVowel (prev) && isPlainVowel (next) && i > 0) || (i == n - 1 && i > 0 && ! juce::String ("ptkf").containsChar (prev)))
                        add ("z");
                    else add ("s");
                    break;
                case 't': add ("t"); break;
                case 'v': add ("v"); break;
                case 'w': add ("w"); break;
                case 'x': add (i == 0 ? "z" : "k s"); break;
                case 'z': add ("z"); break;
                default: break;
            }
        }

        std::vector<Tok> out;
        bool stressDone = false;
        for (size_t k = 0; k < names.size(); ++k)
        {
            const bool stress = isNucleus[k] && ! stressDone;
            if (isNucleus[k]) stressDone = true;
            addEnglish (out, names[k], stress);
        }
        return out;
    }

    inline std::vector<Tok> englishWord (const juce::String& w)
    {
        const auto& dict = englishDictionary();
        const auto it = dict.find (w);
        if (it == dict.end())
            return englishByRule (w);

        std::vector<Tok> out;
        const auto parts = juce::StringArray::fromTokens (it->second, " ", "");
        const bool marked = juce::String (it->second).containsChar ('\'');
        bool first = true;
        for (auto p : parts)
        {
            const bool mark = p.startsWithChar ('\'');
            if (mark) p = p.substring (1);
            const bool isVowelName = p == "AA" || p == "AE" || p == "AH" || p == "AO" || p == "AX" || p == "EH" || p == "ER" || p == "IH" || p == "UH"
                                  || p == "i" || p == "u" || p == "EY" || p == "AY" || p == "AW" || p == "OW" || p == "OY";
            const bool stressed = marked ? mark : (isVowelName && first);
            if (isVowelName) first = false;
            addEnglish (out, p, stressed);
        }
        return out;
    }

    inline juce::String digitName (juce::juce_wchar d, int language)
    {
        static const char* es[] = { "cero", "uno", "dos", "tres", "cuatro", "cinco", "seis", "siete", "ocho", "nueve" };
        static const char* en[] = { "zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine" };
        const int i = juce::jlimit (0, 9, (int) (d - '0'));
        return language == english ? en[i] : es[i];
    }
}

/** Turns text into a script of speech sounds, in Spanish or English. Message thread. */
inline void compile (const juce::String& text, int language, Script& script)
{
    script.numSegs = 0;
    script.numWords = 0;

    // The words, and the pause after each (none, a comma, the end of a sentence or a question).
    struct Word { juce::String text; int pauseAfter = Gap; bool question = false; };
    std::vector<Word> words;
    juce::String current;
    auto finishWord = [&] { if (current.isNotEmpty()) { words.push_back ({ current }); current.clear(); } };
    auto addPause = [&] (int p, bool question)
    {
        finishWord();
        if (! words.empty() && words.back().pauseAfter == Gap)
        {
            words.back().pauseAfter = p;
            words.back().question = question;
        }
    };

    const auto lower = text.toLowerCase().substring (0, 600);
    for (auto ch = lower.getCharPointer(); ! ch.isEmpty(); )
    {
        const auto c = ch.getAndAdvance();
        if (juce::CharacterFunctions::isDigit (c))
        {
            finishWord();
            words.push_back ({ detail::digitName (c, language) });
        }
        else if (juce::CharacterFunctions::isLetter (c) || c == 0xf1 || c == 0xfc || detail::isAccented (c))
            current += c;
        else if (c == '\'' && language == english) {}   // "don't" -> "dont"
        else if (c == ',' || c == ';' || c == ':') addPause (Comma, false);
        else if (c == '.' || c == '!')             addPause (Stop, false);
        else if (c == '?')                         addPause (Stop, true);
        else                                       finishWord();
    }
    finishWord();

    // Each phrase (the words up to a pause): the pitch falls gently through it, rises on
    // stressed vowels, falls at the end of a sentence and rises at the end of a question.
    size_t phraseStart = 0;
    for (size_t wi = 0; wi < words.size() && script.numWords < Script::maxWords; ++wi)
    {
        const bool phraseEnds = words[wi].pauseAfter != Gap || wi + 1 == words.size();
        if (! phraseEnds)
            continue;

        const size_t phraseLength = wi - phraseStart + 1;
        for (size_t k = phraseStart; k <= wi && script.numWords < Script::maxWords; ++k)
        {
            const auto toks = language == english ? detail::englishWord (words[k].text) : detail::spanishWord (words[k].text);
            if (toks.empty())
                continue;
            const int wordIndex = script.numWords++;
            script.wordStart[(size_t) wordIndex] = (std::uint16_t) script.numSegs;

            const bool lastWord = k == wi;
            const float through = phraseLength > 1 ? (float) (k - phraseStart) / (float) (phraseLength - 1) : 0.5f;
            int lastVowelSeg = -1;
            for (size_t t = 0; t < toks.size() && script.numSegs < Script::maxSegs - 2; ++t)
            {
                const auto& def = phone (toks[t].phone);
                Seg s;
                s.phone = (std::uint8_t) toks[t].phone;
                s.word = (std::uint16_t) wordIndex;
                float ms = def.ms;
                float cents = 150.0f - 300.0f * through;   // declination
                if (def.kind == vowel)
                {
                    if (toks[t].stressed) { ms *= 1.35f; cents += 180.0f; s.flags |= Seg::stressed; }
                    else if (language == english && toks[t].phone == AX) ms *= 0.8f;
                    lastVowelSeg = script.numSegs;
                }
                if (lastWord)
                {
                    const float into = toks.size() > 1 ? (float) t / (float) (toks.size() - 1) : 1.0f;
                    cents += words[k].question ? 500.0f * into : (words[k].pauseAfter == Stop || k + 1 == words.size() ? -250.0f * into : 0.0f);
                    if (def.kind == vowel && t + 2 >= toks.size()) ms *= 1.3f;   // the last syllable lengthens
                }
                s.ms = (std::uint16_t) juce::roundToInt (ms);
                s.cents = (std::int16_t) juce::roundToInt (cents);
                script.segs[(size_t) script.numSegs++] = s;
            }
            if (lastVowelSeg >= 0)
                script.segs[(size_t) lastVowelSeg].flags |= Seg::lastVowel;

            // The pause after the word: a tiny gap between words, longer at commas and full stops.
            if (script.numSegs < Script::maxSegs)
            {
                Seg p;
                p.phone = (std::uint8_t) words[k].pauseAfter;
                p.word = (std::uint16_t) wordIndex;
                p.ms = (std::uint16_t) phone (p.phone).ms;
                script.segs[(size_t) script.numSegs++] = p;
            }
        }
        phraseStart = wi + 1;
    }
    script.wordStart[(size_t) script.numWords] = (std::uint16_t) script.numSegs;
}

/** A script cut into pieces, one per note: syllables ("ciu-dad"), or single sounds ("s", "a", "k").
    start[i] .. start[i + 1] are the segments of piece i, without the pauses after it. Returns how many. */
constexpr int maxPieces = 512;
using Pieces = std::array<std::uint16_t, maxPieces + 1>;

inline int syllables (const Script& s, Pieces& start, Pieces& end)
{
    auto isVowel = [&s] (int i) { return phone (s.segs[(size_t) i].phone).kind == vowel; };
    auto kindOf = [&s] (int i) { return phone (s.segs[(size_t) i].phone).kind; };
    int n = 0;
    for (int w = 0; w < s.numWords && n < maxPieces; ++w)
    {
        const int from = s.wordStart[(size_t) w], to = s.wordStart[(size_t) w + 1];
        int last = to;   // the word without its pause
        while (last > from && kindOf (last - 1) == pause) --last;
        // The vowels (a vowel straight after another, or a glide between two, stays in the same syllable).
        std::array<int, 64> nuclei {};
        int count = 0;
        for (int i = from; i < last && count < 64; ++i)
            if (isVowel (i) && (count == 0 || ! (i > from && (isVowel (i - 1) || kindOf (i - 1) == glide))))
                nuclei[(size_t) count++] = i;
        if (count == 0)
        {
            if (last > from) { start[(size_t) n] = (std::uint16_t) from; end[(size_t) n] = (std::uint16_t) last; ++n; }
            continue;
        }
        int pieceStart = from;
        for (int k = 1; k < count && n < maxPieces; ++k)
        {
            // The consonants between two vowels: the last one starts the next syllable (a stop with an
            // l or r after it goes with them: "pla", "tre"); with none, the next vowel starts it.
            int boundary = nuclei[(size_t) k];
            int c = nuclei[(size_t) k] - 1;
            while (c > nuclei[(size_t) k - 1] && (isVowel (c) || kindOf (c) == glide)) --c;
            if (c > nuclei[(size_t) k - 1])
            {
                boundary = c;
                if (kindOf (c) == burst && c - 1 > nuclei[(size_t) k - 1] && kindOf (c - 1) == closure) boundary = c - 1;
                const auto kc = kindOf (c);
                if ((kc == liquid || kc == tap) && boundary - 1 > nuclei[(size_t) k - 1] && kindOf (boundary - 1) == burst)
                {
                    boundary -= 1;
                    if (boundary - 1 > nuclei[(size_t) k - 1] && kindOf (boundary - 1) == closure) boundary -= 1;
                }
                // Glides before the vowel belong to it too ("ciu", "bien").
                while (boundary > nuclei[(size_t) k - 1] + 1 && kindOf (boundary - 1) == glide) --boundary;
            }
            start[(size_t) n] = (std::uint16_t) pieceStart;
            end[(size_t) n] = (std::uint16_t) boundary;
            ++n;
            pieceStart = boundary;
        }
        if (n < maxPieces) { start[(size_t) n] = (std::uint16_t) pieceStart; end[(size_t) n] = (std::uint16_t) last; ++n; }
    }
    return n;
}

/** One piece per sound you hear: each vowel, consonant or hiss alone (a stop is its release only). */
inline int sounds (const Script& s, Pieces& start, Pieces& end)
{
    int n = 0;
    for (int i = 0; i < s.numSegs && n < maxPieces; ++i)
    {
        const auto k = phone (s.segs[(size_t) i].phone).kind;
        if (k == pause || k == closure) continue;
        start[(size_t) n] = (std::uint16_t) i;
        end[(size_t) n] = (std::uint16_t) (i + 1);
        ++n;
    }
    return n;
}

/** The sounds the keys play in the "Sounds on keys" mode, from C2 (note 36) upwards: vowels first, then the
    consonants, as on the Elektron voice machines. */
inline const std::vector<int>& keySounds()
{
    static const std::vector<int> list { A, E, I, O, U, AE, AA, AO, EH, IH, UH, AH, ER,
                                         M, N, NG, L, Rtap, RR, W, Yg,
                                         S, SH, Z, F, V, TH, X, H,
                                         Pb, Bb, Tb, Db, Kb, Gb };
    return list;
}

inline int keySoundFor (int note)
{
    const auto& list = keySounds();
    return list[(size_t) ((note - 36) % (int) list.size() + (int) list.size()) % list.size()];
}

/** The sounds of a script as text, for tests: "o l a" (stops show as their release). */
inline juce::String describe (const Script& s)
{
    juce::StringArray parts;
    for (int i = 0; i < s.numSegs; ++i)
    {
        const auto& def = phone (s.segs[(size_t) i].phone);
        if (def.kind == closure || def.kind == pause) continue;
        parts.add (juce::String (def.name) + ((s.segs[(size_t) i].flags & Seg::stressed) ? "'" : ""));
    }
    return parts.joinIntoString (" ");
}

} // namespace speech
} // namespace bizgroove
