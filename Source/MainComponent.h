#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include <vector>

class MainComponent final : public juce::AudioAppComponent,
                            private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;
    void resized() override;
    void paint(juce::Graphics& graphics) override;

private:
    struct Track
    {
        juce::String name;
        juce::File file;
        std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
        juce::AudioTransportSource transport;
        std::unique_ptr<juce::AudioProcessor> effect;
        juce::AudioBuffer<float> scratch;
        std::vector<float> waveform;
        std::atomic<float> gain { 0.8f };
        std::atomic<bool> muted { false };
        double durationSeconds = 0.0;
    };

    class SampleListModel final : public juce::ListBoxModel
    {
    public:
        explicit SampleListModel(MainComponent& owner) : main(owner) {}
        int getNumRows() override;
        void paintListBoxItem(int row, juce::Graphics& graphics, int width, int height, bool selected) override;
        void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;
        juce::File fileForRow(int row) const;
        void refresh();

    private:
        MainComponent& main;
        juce::Array<juce::File> files;
    };

    class ArrangementView final : public juce::Component,
                                  public juce::FileDragAndDropTarget
    {
    public:
        explicit ArrangementView(MainComponent& owner) : main(owner) {}
        void paint(juce::Graphics& graphics) override;
        void mouseDown(const juce::MouseEvent& event) override;
        bool isInterestedInFileDrag(const juce::StringArray& files) override;
        void filesDropped(const juce::StringArray& files, int x, int y) override;

    private:
        MainComponent& main;
    };

    void timerCallback() override;
    void importLibraryFolder();
    void addSampleToArrangement(const juce::File& file);
    void refreshLibrary();
    void loadVst3();
    void selectTrack(int index);
    juce::File getLibraryDirectory() const;
    static bool isSupportedAudioFile(const juce::File& file);
    static std::vector<float> makeWaveform(juce::AudioFormatReader& reader);

    juce::AudioFormatManager formatManager;
    juce::AudioPluginFormatManager pluginFormats;
    std::vector<std::unique_ptr<Track>> tracks;
    juce::CriticalSection audioLock;
    std::atomic<bool> playing { false };
    int selectedTrack = -1;
    double playheadSeconds = 0.0;

    juce::TextButton playButton { "PLAY" };
    juce::TextButton stopButton { "STOP" };
    juce::TextButton importButton { "IMPORT LIBRARY" };
    juce::TextButton addSampleButton { "ADD TO ARRANGEMENT" };
    juce::TextButton loadPluginButton { "LOAD VST3" };
    juce::Label titleLabel;
    juce::Label timeLabel;
    juce::Label libraryLabel;
    juce::Label inspectorLabel;
    juce::Label trackNameLabel;
    juce::Label effectLabel;
    juce::Label gainValueLabel;
    juce::Slider gainSlider;
    juce::ToggleButton muteButton { "Mute track" };
    juce::ListBox sampleList;
    SampleListModel sampleModel;
    ArrangementView arrangement;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};