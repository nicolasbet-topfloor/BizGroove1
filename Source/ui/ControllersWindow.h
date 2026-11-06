#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "engine/MidiRouter.h"

namespace bizgroove
{

/** Loads and saves controller profiles and device assignments as JSON in the
    BizGroove settings folder (Controllers.json). */
namespace ControllerStore
{
    juce::File file();
    void load (MidiRouter&);
    void save (const MidiRouter&);
}

/** The Controllers window: which profile each connected device uses, and an
    editor for any profile's bindings with MIDI learn. Every change is saved
    at once. */
class ControllersComponent final : public juce::Component,
                                   private juce::ListBoxModel,
                                   private juce::Timer
{
public:
    ControllersComponent (MidiRouter& router, const ControlTargets& targets, juce::AudioDeviceManager& devices);
    ~ControllersComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class BindingRow;
    friend class BindingRow;

    // ListBoxModel
    int getNumRows() override;
    void paintListBoxItem (int, juce::Graphics&, int, int, bool) override {}
    juce::Component* refreshComponentForRow (int row, bool selected, juce::Component* existing) override;

    void timerCallback() override;

    void rebuildDeviceRows();
    void selectProfile (const juce::String& name);
    void rebuildProfileMenu();
    void commit();                         // push the edited profile to the router and save
    void learnRow (int row, bool continueAfter);
    void stopLearning();
    int nextLearnableRow (int after) const;
    void addRow();
    void deleteRow (int row);
    void resetProfile();
    void duplicateProfile();
    void deleteProfile();
    void fillTargetMenu (juce::ComboBox&) const;

    MidiRouter& router;
    const ControlTargets& targets;
    juce::AudioDeviceManager& devices;

    ControllerProfile editing;
    int learningRow = -1;
    bool learningAll = false;

    // Devices section
    juce::Label devicesHeader;
    struct DeviceRow { juce::String identifier; std::unique_ptr<juce::Label> name; std::unique_ptr<juce::ComboBox> profile; };
    std::vector<DeviceRow> deviceRows;
    juce::Label noDevices;
    juce::StringArray lastDeviceIds;

    // Profile editor
    juce::Label profileHeader;
    juce::ComboBox profileMenu;
    juce::TextButton duplicateButton { "Duplicate" }, deleteButton { "Delete" }, resetButton { "Reset" };
    juce::Label notes;
    juce::ToggleButton passNotes { "Unmapped notes play the selected track" };
    juce::ListBox list { "bindings", this };
    juce::TextButton addButton { "Add control" }, learnAllButton { "Learn all" }, skipButton { "Skip" };
    juce::Label status, monitor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControllersComponent)
};

} // namespace bizgroove
