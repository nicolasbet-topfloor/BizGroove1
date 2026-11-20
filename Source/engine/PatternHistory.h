#pragma once

#include "Song.h"
#include <deque>
#include <memory>

namespace bizgroove
{

/** Undo for pattern edits. Before a pattern changes, the UI remembers how it was;
    Undo puts the last remembered version back. Clearing or pasting a whole scene
    is remembered as one step for all 16 tracks. Message thread only.

    Edits that come quickly one after another on the same pattern (scrolling a
    note, turning a knob on a held step) count as one: they are remembered once. */
class PatternHistory
{
public:
    /** Call before changing one track's pattern in a scene. `kind` groups quick
        repeats of the same kind of edit into one undo step (0 = never grouped). */
    void remember (Song& song, int scene, int track, int kind = 0)
    {
        const auto now = juce::Time::getMillisecondCounter();
        if (kind != 0 && ! entries.empty())
        {
            const auto& last = entries.back();
            if (last.kind == kind && last.scene == scene && last.track == track && now - lastTime < 1200u)
            {
                lastTime = now;
                return;
            }
        }
        lastTime = now;
        push (song, scene, track, kind, nextGroup++);
    }

    /** Call before changing every track of a scene (Clear, Paste). */
    void rememberScene (Song& song, int scene)
    {
        const int group = nextGroup++;
        for (int t = 0; t < Song::numTracks; ++t)
            push (song, scene, t, 0, group);
        lastTime = 0;
    }

    bool canUndo() const { return ! entries.empty(); }

    /** Puts back the last remembered edit. False when there is nothing to undo. */
    bool undo (Song& song)
    {
        if (entries.empty())
            return false;
        const int group = entries.back().group;
        while (! entries.empty() && entries.back().group == group)
        {
            auto& e = entries.back();
            song.scene (e.scene).tracks[(size_t) e.track].copyFrom (*e.pattern);
            entries.pop_back();
        }
        lastTime = 0;
        return true;
    }

    void clear() { entries.clear(); }

private:
    struct Entry
    {
        int scene = 0, track = 0, kind = 0, group = 0;
        std::unique_ptr<Song::TrackPattern> pattern;
    };

    void push (Song& song, int scene, int track, int kind, int group)
    {
        if (! juce::isPositiveAndBelow (scene, Song::numScenes) || ! juce::isPositiveAndBelow (track, Song::numTracks))
            return;
        Entry e;
        e.scene = scene; e.track = track; e.kind = kind; e.group = group;
        e.pattern = std::make_unique<Song::TrackPattern>();
        e.pattern->copyFrom (song.scene (scene).tracks[(size_t) track]);
        entries.push_back (std::move (e));
        while (entries.size() > maxEntries)
            entries.pop_front();
    }

    static constexpr size_t maxEntries = 400;
    std::deque<Entry> entries;
    int nextGroup = 0;
    juce::uint32 lastTime = 0;
};

} // namespace bizgroove
