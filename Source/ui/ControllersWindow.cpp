#include "ControllersWindow.h"
#include "LookAndFeel.h"

namespace bizgroove
{

// ============================================================== ControllerStore

juce::File ControllerStore::file()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "BizGroove";
    options.filenameSuffix = ".settings";
    options.folderName = "BizGroove";
    options.osxLibrarySubFolder = "Application Support";
    return options.getDefaultFile().getSiblingFile ("Controllers.json");
}

void ControllerStore::load (MidiRouter& router)
{
    auto profiles = ControllerProfile::builtIns();

    const auto json = juce::JSON::parse (file());
    if (const auto* saved = json.getProperty ("profiles", {}).getArray())
    {
        for (const auto& v : *saved)
        {
            auto p = ControllerProfile::fromVar (v);
            auto it = std::find_if (profiles.begin(), profiles.end(), [&] (const ControllerProfile& b) { return b.name == p.name; });
            if (it != profiles.end()) *it = p;   // an edited built-in
            else profiles.push_back (p);         // a user profile
        }
    }
    router.setProfiles (profiles);

    if (auto* obj = json.getProperty ("assignments", {}).getDynamicObject())
        for (const auto& prop : obj->getProperties())
            router.assignProfile (prop.name.toString(), prop.value.toString());
}

void ControllerStore::save (const MidiRouter& router)
{
    auto* root = new juce::DynamicObject();
    juce::Array<juce::var> list;
    for (const auto& p : router.getProfiles())
        list.add (p.toVar());
    root->setProperty ("profiles", list);

    auto* assignments = new juce::DynamicObject();
    for (const auto& [device, profile] : router.getAssignments())
        assignments->setProperty (device, profile);
    root->setProperty ("assignments", juce::var (assignments));
    root->setProperty ("formatVersion", 1);

    const auto f = file();
    f.getParentDirectory().createDirectory();
    f.replaceWithText (juce::JSON::toString (juce::var (root)));
}

// ============================================================== BindingRow

class ControllersComponent::BindingRow final : public juce::Component
{
public:
    explicit BindingRow (ControllersComponent& o) : owner (o)
    {
        control.setEditable (false, true);
        control.setColour (juce::Label::textColourId, Colours::text);
        control.onTextChange = [this] {
            if (juce::isPositiveAndBelow (row, (int) owner.editing.bindings.size()))
            {
                owner.editing.bindings[(size_t) row].control = control.getText();
                owner.commit();
            }
        };
        addAndMakeVisible (control);

        owner.fillTargetMenu (target);
        target.onChange = [this] {
            if (! juce::isPositiveAndBelow (row, (int) owner.editing.bindings.size()))
                return;
            const int id = target.getSelectedId();
            const auto& all = owner.targets.all();
            owner.editing.bindings[(size_t) row].target = id >= 2 && id - 2 < (int) all.size() ? all[(size_t) (id - 2)].id : juce::String();
            owner.commit();
        };
        addAndMakeVisible (target);

        midi.setColour (juce::Label::textColourId, Colours::textDim);
        addAndMakeVisible (midi);

        learn.setWantsKeyboardFocus (false);
        learn.onClick = [this] {
            if (owner.learningRow == row) owner.stopLearning();
            else owner.learnRow (row, false);
        };
        addAndMakeVisible (learn);

        clear.setTooltip ("Forget this control's MIDI message");
        clear.onClick = [this] {
            owner.editing.bindings[(size_t) row].midi = {};
            owner.commit();
            update (row);
        };
        addAndMakeVisible (clear);

        remove.setTooltip ("Delete this row");
        remove.onClick = [this] { owner.deleteRow (row); };
        addAndMakeVisible (remove);
    }

    void update (int newRow)
    {
        row = newRow;
        if (! juce::isPositiveAndBelow (row, (int) owner.editing.bindings.size()))
            return;

        const auto& b = owner.editing.bindings[(size_t) row];
        control.setText (b.control, juce::dontSendNotification);

        int id = 1;
        const auto& all = owner.targets.all();
        for (size_t i = 0; i < all.size(); ++i)
            if (all[i].id == b.target)
                id = (int) i + 2;
        target.setSelectedId (id, juce::dontSendNotification);

        const bool armed = owner.learningRow == row;
        midi.setText (armed ? "Move or press it now..." : b.midi.describe(), juce::dontSendNotification);
        midi.setColour (juce::Label::textColourId, armed ? Colours::accent : (b.midi.isSet() ? Colours::text : Colours::textDim));
        learn.setButtonText (armed ? "Cancel" : "Learn");
        learn.setToggleState (armed, juce::dontSendNotification);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (Colours::panelEdge);
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (4, 3);
        remove.setBounds (r.removeFromRight (24));
        r.removeFromRight (4);
        clear.setBounds (r.removeFromRight (24));
        r.removeFromRight (4);
        learn.setBounds (r.removeFromRight (64));
        r.removeFromRight (8);
        control.setBounds (r.removeFromLeft (170));
        r.removeFromLeft (8);
        target.setBounds (r.removeFromLeft (240));
        r.removeFromLeft (8);
        midi.setBounds (r);
    }

private:
    ControllersComponent& owner;
    int row = -1;
    juce::Label control, midi;
    juce::ComboBox target;
    juce::TextButton learn { "Learn" }, clear { "x" }, remove { "-" };
};

// ============================================================== ControllersComponent

ControllersComponent::ControllersComponent (MidiRouter& r, const ControlTargets& t, juce::AudioDeviceManager& d)
    : router (r), targets (t), devices (d)
{
    auto heading = [] (juce::Label& l, const juce::String& text) {
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        l.setColour (juce::Label::textColourId, Colours::text);
    };

    heading (devicesHeader, "Connected controllers");
    addAndMakeVisible (devicesHeader);
    noDevices.setText ("No MIDI devices found. Plug one in; it appears here automatically.", juce::dontSendNotification);
    noDevices.setColour (juce::Label::textColourId, Colours::textDim);
    addChildComponent (noDevices);

    heading (profileHeader, "Edit profile");
    addAndMakeVisible (profileHeader);
    profileMenu.onChange = [this] { selectProfile (profileMenu.getText()); };
    addAndMakeVisible (profileMenu);

    duplicateButton.onClick = [this] { duplicateProfile(); };
    deleteButton.onClick = [this] { deleteProfile(); };
    resetButton.setTooltip ("Restore the built-in bindings for this controller");
    resetButton.onClick = [this] { resetProfile(); };
    for (auto* b : { &duplicateButton, &deleteButton, &resetButton })
        addAndMakeVisible (b);

    notes.setColour (juce::Label::textColourId, Colours::textDim);
    notes.setJustificationType (juce::Justification::topLeft);
    notes.setFont (juce::FontOptions (13.0f));
    addAndMakeVisible (notes);

    passNotes.setColour (juce::ToggleButton::textColourId, Colours::text);
    passNotes.onClick = [this] { editing.passUnmappedNotes = passNotes.getToggleState(); commit(); };
    addAndMakeVisible (passNotes);

    list.setRowHeight (32);
    list.setColour (juce::ListBox::backgroundColourId, Colours::panel);
    list.setOutlineThickness (1);
    list.setColour (juce::ListBox::outlineColourId, Colours::panelEdge);
    addAndMakeVisible (list);

    addButton.onClick = [this] { addRow(); };
    learnAllButton.setTooltip ("Step through every row: move each control when it is named");
    learnAllButton.onClick = [this] {
        if (learningAll) stopLearning();
        else learnRow (nextLearnableRow (-1), true);
    };
    skipButton.onClick = [this] {
        if (learningAll && nextLearnableRow (learningRow) >= 0) learnRow (nextLearnableRow (learningRow), true);
        else stopLearning();
    };
    addAndMakeVisible (addButton);
    addAndMakeVisible (learnAllButton);
    addChildComponent (skipButton);

    status.setColour (juce::Label::textColourId, Colours::accent);
    status.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    addAndMakeVisible (status);

    monitor.setColour (juce::Label::textColourId, Colours::textDim);
    monitor.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (monitor);

    rebuildDeviceRows();
    rebuildProfileMenu();

    // Open on the profile used by the first connected device, else the default.
    const auto inputs = juce::MidiInput::getAvailableDevices();
    selectProfile (inputs.isEmpty() ? juce::String (ControllerProfile::defaultProfileName)
                                    : router.resolveProfileName (inputs[0].identifier, inputs[0].name));

    setSize (860, 640);
    startTimerHz (10);
}

ControllersComponent::~ControllersComponent()
{
    router.cancelLearn();
}

void ControllersComponent::fillTargetMenu (juce::ComboBox& box) const
{
    box.clear (juce::dontSendNotification);
    box.addItem (juce::String::fromUTF8 ("\xe2\x80\x94 nothing \xe2\x80\x94"), 1);
    juce::String group;
    const auto& all = targets.all();
    for (size_t i = 0; i < all.size(); ++i)
    {
        if (all[i].group != group)
        {
            group = all[i].group;
            box.addSectionHeading (group);
        }
        box.addItem (all[i].name, (int) i + 2);
    }
}

void ControllersComponent::rebuildDeviceRows()
{
    for (auto& row : deviceRows)
    {
        removeChildComponent (row.name.get());
        removeChildComponent (row.profile.get());
    }
    deviceRows.clear();

    lastDeviceIds.clear();
    const auto profiles = router.getProfiles();

    for (const auto& input : juce::MidiInput::getAvailableDevices())
    {
        lastDeviceIds.add (input.identifier);

        DeviceRow row;
        row.identifier = input.identifier;
        row.name = std::make_unique<juce::Label> (juce::String(), input.name);
        row.name->setColour (juce::Label::textColourId, Colours::text);

        row.profile = std::make_unique<juce::ComboBox>();
        const auto assigned = router.getAssignedProfile (input.identifier);
        router.assignProfile (input.identifier, {});
        const auto automatic = router.resolveProfileName (input.identifier, input.name);
        router.assignProfile (input.identifier, assigned);

        row.profile->addItem ("Automatic (" + automatic + ")", 1);
        for (size_t i = 0; i < profiles.size(); ++i)
            row.profile->addItem (profiles[i].name, (int) i + 2);

        int selected = 1;
        for (size_t i = 0; i < profiles.size(); ++i)
            if (profiles[i].name == assigned)
                selected = (int) i + 2;
        row.profile->setSelectedId (selected, juce::dontSendNotification);

        auto* combo = row.profile.get();
        const auto id = input.identifier;
        combo->onChange = [this, combo, id] {
            router.assignProfile (id, combo->getSelectedId() == 1 ? juce::String() : combo->getText());
            ControllerStore::save (router);
        };

        addAndMakeVisible (*row.name);
        addAndMakeVisible (*row.profile);
        deviceRows.push_back (std::move (row));
    }

    noDevices.setVisible (deviceRows.empty());
    resized();
}

void ControllersComponent::rebuildProfileMenu()
{
    profileMenu.clear (juce::dontSendNotification);
    const auto profiles = router.getProfiles();
    for (size_t i = 0; i < profiles.size(); ++i)
        profileMenu.addItem (profiles[i].name, (int) i + 1);
}

void ControllersComponent::selectProfile (const juce::String& name)
{
    stopLearning();
    for (const auto& p : router.getProfiles())
        if (p.name == name)
            editing = p;

    for (int i = 0; i < profileMenu.getNumItems(); ++i)
        if (profileMenu.getItemText (i) == editing.name)
            profileMenu.setSelectedItemIndex (i, juce::dontSendNotification);

    bool builtIn = false;
    for (const auto& b : ControllerProfile::builtIns())
        builtIn = builtIn || b.name == editing.name;
    resetButton.setEnabled (builtIn);
    deleteButton.setEnabled (! builtIn);

    notes.setText (editing.notes, juce::dontSendNotification);
    passNotes.setToggleState (editing.passUnmappedNotes, juce::dontSendNotification);
    list.updateContent();
    list.repaint();
}

void ControllersComponent::commit()
{
    router.updateProfile (editing);
    ControllerStore::save (router);
}

int ControllersComponent::getNumRows()
{
    return (int) editing.bindings.size();
}

juce::Component* ControllersComponent::refreshComponentForRow (int row, bool, juce::Component* existing)
{
    auto* rowComp = dynamic_cast<BindingRow*> (existing);
    if (row >= getNumRows())
    {
        delete existing;
        return nullptr;
    }
    if (rowComp == nullptr)
    {
        delete existing;
        rowComp = new BindingRow (*this);
    }
    rowComp->update (row);
    return rowComp;
}

void ControllersComponent::learnRow (int row, bool continueAfter)
{
    if (! juce::isPositiveAndBelow (row, (int) editing.bindings.size()))
    {
        stopLearning();
        return;
    }

    commit(); // the router learns into its own copy, so give it ours first
    learningRow = row;
    learningAll = continueAfter;

    const auto& b = editing.bindings[(size_t) row];
    const auto* t = targets.find (b.target);
    status.setText ("Move or press:  " + b.control + (t != nullptr ? juce::String::fromUTF8 ("  \xe2\x86\x92  ") + t->name : juce::String()),
                    juce::dontSendNotification);
    learnAllButton.setButtonText (learningAll ? "Stop" : "Learn all");
    skipButton.setVisible (learningAll);
    list.scrollToEnsureRowIsOnscreen (row);
    list.updateContent();
    list.repaint();

    router.startLearn (editing.name, row, [safe = juce::Component::SafePointer<ControllersComponent> (this), row] (MidiTrigger trigger) {
        if (safe == nullptr || safe->learningRow != row)
            return;
        safe->editing.bindings[(size_t) row].midi = trigger;
        safe->commit();
        if (safe->learningAll && safe->nextLearnableRow (row) >= 0)
            safe->learnRow (safe->nextLearnableRow (row), true);
        else
        {
            safe->stopLearning();
            safe->status.setText ("Learned " + trigger.describe() + ".", juce::dontSendNotification);
        }
    });
}

int ControllersComponent::nextLearnableRow (int after) const
{
    // "Learn all" skips rows that drive nothing.
    for (int i = after + 1; i < (int) editing.bindings.size(); ++i)
        if (editing.bindings[(size_t) i].target.isNotEmpty())
            return i;
    return -1;
}

void ControllersComponent::stopLearning()
{
    router.cancelLearn();
    learningRow = -1;
    learningAll = false;
    status.setText ({}, juce::dontSendNotification);
    learnAllButton.setButtonText ("Learn all");
    skipButton.setVisible (false);
    list.updateContent();
    list.repaint();
}

void ControllersComponent::addRow()
{
    editing.bindings.push_back ({ "Control " + juce::String ((int) editing.bindings.size() + 1), "", {} });
    commit();
    list.updateContent();
    list.scrollToEnsureRowIsOnscreen ((int) editing.bindings.size() - 1);
}

void ControllersComponent::deleteRow (int row)
{
    if (! juce::isPositiveAndBelow (row, (int) editing.bindings.size()))
        return;
    stopLearning();
    editing.bindings.erase (editing.bindings.begin() + row);
    commit();
    list.updateContent();
    list.repaint();
}

void ControllersComponent::resetProfile()
{
    for (const auto& b : ControllerProfile::builtIns())
    {
        if (b.name == editing.name)
        {
            editing = b;
            commit();
            selectProfile (b.name);
            status.setText ("Restored the built-in " + b.name + " bindings.", juce::dontSendNotification);
        }
    }
}

void ControllersComponent::duplicateProfile()
{
    auto copy = editing;
    const auto profiles = router.getProfiles();
    juce::String name;
    for (int n = 1; ; ++n)
    {
        name = editing.name + (n == 1 ? " (my copy)" : " (my copy " + juce::String (n) + ")");
        if (std::none_of (profiles.begin(), profiles.end(), [&] (const ControllerProfile& p) { return p.name == name; }))
            break;
    }
    copy.name = name;
    copy.deviceMatches.clear(); // assign it to a device explicitly
    router.updateProfile (copy);
    ControllerStore::save (router);
    rebuildProfileMenu();
    rebuildDeviceRows();
    selectProfile (name);
}

void ControllersComponent::deleteProfile()
{
    const auto name = editing.name;
    router.removeProfile (name);
    for (const auto& [device, profile] : router.getAssignments())
        if (profile == name)
            router.assignProfile (device, {});
    ControllerStore::save (router);
    rebuildProfileMenu();
    rebuildDeviceRows();
    selectProfile (ControllerProfile::defaultProfileName);
}

void ControllersComponent::timerCallback()
{
    juce::StringArray ids;
    for (const auto& input : juce::MidiInput::getAvailableDevices())
        ids.add (input.identifier);
    if (ids != lastDeviceIds)
        rebuildDeviceRows();

    const auto last = router.getLastMessageDescription();
    monitor.setText (last.isEmpty() ? "MIDI monitor: move any control to see what it sends."
                                    : "MIDI monitor:  " + last,
                     juce::dontSendNotification);
}

void ControllersComponent::paint (juce::Graphics& g)
{
    g.fillAll (Colours::background);
}

void ControllersComponent::resized()
{
    auto r = getLocalBounds().reduced (16);

    devicesHeader.setBounds (r.removeFromTop (24));
    r.removeFromTop (4);
    if (deviceRows.empty())
        noDevices.setBounds (r.removeFromTop (26));
    for (auto& row : deviceRows)
    {
        auto line = r.removeFromTop (30);
        row.name->setBounds (line.removeFromLeft (300));
        row.profile->setBounds (line.removeFromLeft (360).reduced (0, 2));
    }
    r.removeFromTop (16);

    auto head = r.removeFromTop (30);
    profileHeader.setBounds (head.removeFromLeft (110));
    resetButton.setBounds (head.removeFromRight (80).reduced (0, 2));
    head.removeFromRight (6);
    deleteButton.setBounds (head.removeFromRight (80).reduced (0, 2));
    head.removeFromRight (6);
    duplicateButton.setBounds (head.removeFromRight (90).reduced (0, 2));
    head.removeFromRight (12);
    profileMenu.setBounds (head.reduced (0, 2));

    r.removeFromTop (6);
    notes.setBounds (r.removeFromTop (54));
    passNotes.setBounds (r.removeFromTop (26));
    r.removeFromTop (6);

    monitor.setBounds (r.removeFromBottom (22));
    auto bottom = r.removeFromBottom (34);
    r.removeFromBottom (8);
    addButton.setBounds (bottom.removeFromLeft (110).reduced (0, 3));
    bottom.removeFromLeft (8);
    learnAllButton.setBounds (bottom.removeFromLeft (90).reduced (0, 3));
    bottom.removeFromLeft (8);
    skipButton.setBounds (bottom.removeFromLeft (70).reduced (0, 3));
    bottom.removeFromLeft (12);
    status.setBounds (bottom);

    list.setBounds (r);
}

} // namespace bizgroove
