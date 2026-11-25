#include <JuceHeader.h>

juce::AudioProcessor* JUCE_CALLTYPE createFETLimiter74();
juce::AudioProcessor* JUCE_CALLTYPE createLead85MKII();

static bool capture(std::unique_ptr<juce::AudioProcessor> processor, const juce::File& output)
{
    const std::unique_ptr<juce::AudioProcessorEditor> editor(processor->createEditor());
    if (editor == nullptr)
        return false;

    editor->setVisible(true);
    const auto image = editor->createComponentSnapshot(editor->getLocalBounds());
    juce::FileOutputStream stream(output);
    return image.isValid() && stream.openedOk() && juce::PNGImageFormat().writeImageToStream(image, stream);
}

int main()
{
    juce::ScopedJuceInitialiser_GUI initialise;
    const auto directory = juce::File::getCurrentWorkingDirectory();
    return capture(std::unique_ptr<juce::AudioProcessor>(createFETLimiter74()),
                   directory.getChildFile("FETLimiter74-GUI.png"))
        && capture(std::unique_ptr<juce::AudioProcessor>(createLead85MKII()),
                   directory.getChildFile("Lead85MKII-GUI.png")) ? 0 : 1;
}
