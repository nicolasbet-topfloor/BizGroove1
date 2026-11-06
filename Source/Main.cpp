#include <juce_gui_extra/juce_gui_extra.h>
#include "ui/MainComponent.h"

namespace bizgroove
{

class BizGrooveApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "BizGroove"; }
    const juce::String getApplicationVersion() override { return BIZGROOVE_VERSION; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String& commandLine) override
    {
        mainWindow = std::make_unique<MainWindow> (getApplicationName());

        // A project passed on the command line (or with "Open with") is opened at once.
        const auto arg = commandLine.unquoted().trim();
        if (arg.endsWithIgnoreCase (".bizgroove"))
            if (auto* main = dynamic_cast<MainComponent*> (mainWindow->getContentComponent()))
                main->openFile (juce::File (arg));
    }

    void shutdown() override { mainWindow = nullptr; }

    void systemRequestedQuit() override
    {
        if (mainWindow != nullptr)
            mainWindow->confirmQuit();
        else
            quit();
    }

private:
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (const juce::String& name)
            : DocumentWindow (name, Colours::background, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);
            setResizable (true, true);
            setResizeLimits (1200, 720, 2600, 1600);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override { JUCEApplication::getInstance()->systemRequestedQuit(); }

        /** Offers to save unsaved changes before quitting. */
        void confirmQuit()
        {
            if (auto* main = dynamic_cast<MainComponent*> (getContentComponent()))
                main->confirmClose ([] { JUCEApplication::getInstance()->quit(); });
            else
                JUCEApplication::getInstance()->quit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

    std::unique_ptr<MainWindow> mainWindow;
};

} // namespace bizgroove

START_JUCE_APPLICATION (bizgroove::BizGrooveApplication)
